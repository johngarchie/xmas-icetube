#include <avr/eeprom.h>
#include <avr/io.h>      
#include <avr/power.h>
#include <avr/interrupt.h>

#include "alarm.h"
#include "time.h"
#include "power.h"
#include "button.h"

// current alarm settings
volatile alarm_t alarm;

// default alarm time
uint8_t ee_alarm_hour   EEMEM = ALARM_DEFAULT_HOUR;
uint8_t ee_alarm_minute EEMEM = ALARM_DEFAULT_MINUTE;


// initialize alarm on startup
void alarm_init(void) {
    alarm_load();  // load alarm time from eeprom

    // clamp alarm switch pin to ground
    PORTD &= ~_BV(PD2); // disable pull-up resistor
    DDRD  |=  _BV(PD2); // set as ouput, clamped to ground

    // set buzzer pins to output now, so the spi subsystem doesn't
    // hijack PB2 (aka Slave Select) when SPCR gets set
    DDRB |= _BV(PB2) | _BV(PB1);

    // pull both buzzer pins low
    PORTB &= ~_BV(PB2) & ~_BV(PB1);
}


// prepare alarm for sleep
void alarm_sleep(void) {
    // clamp alarm switch pin to ground
    PORTD &= ~_BV(PD2); // disable pull-up resistor
    DDRD  |=  _BV(PD2); // set as ouput, clamped to ground

    // disable buzzer, if enabled
    alarm_buzzeroff();
}


// initialize alarm after sleep
void alarm_wake(void) {
    // configure the alarm switch pin
    DDRD  &= ~_BV(PD2); // set as input pin
    PORTD |=  _BV(PD2); // enable pull-up resistor

    // set initial alarm status from alarm switch
    if(PIND & _BV(PD2)) {
	alarm.status = ALARM_SET;
    } else {
	alarm.status = 0;
    }
}


// check if alarm should sound
void alarm_tick(void) {
    if(alarm.status & ALARM_SET
	    && time.hour   == alarm.hour
	    && time.minute == alarm.minute
	    && time.second == 0) {

	alarm.alarm_timer  = 0;
	alarm.buzzer_timer = 0;
	alarm.status |= ALARM_SOUNDING;
	return;
    }

    if(alarm.status & ALARM_SNOOZE) {
	if(++alarm.alarm_timer == ALARM_SNOOZE_TIMEOUT) {
	    alarm.alarm_timer = 0;
	    alarm.buzzer_timer = 0;
	    alarm.status &= ~ALARM_SNOOZE;
	    alarm.status |=  ALARM_SOUNDING;
	    return;
	}
    }

    if(alarm.status & ALARM_SOUNDING) {
	if(time.second % 2) {
	    alarm_buzzeroff();
	} else {
	    alarm_buzzeron();
	}

	if(++alarm.alarm_timer >= ALARM_SOUNDING_TIMEOUT) {
	    alarm.status &= ~ALARM_SOUNDING;
	}
    }
}


// control alarm buzzer
void alarm_semitick(void) {
    // trigger snooze if button pressed during alarm
    if(alarm.status & ALARM_SOUNDING && button_process()) {
	alarm.status &= ~ALARM_SOUNDING;
	alarm.status |=  ALARM_SNOOZE;
	alarm.alarm_timer = 0;
	alarm_buzzeroff();
    }

    // extend snooze if any button is pressed
    if(alarm.status & ALARM_SNOOZE && button.pressed) {
	alarm.alarm_timer = 0;
    }

    if(alarm.status & ALARM_BEEP) {
	// stop buzzer if beeping
	if(--alarm.buzzer_timer == 0) {
	    alarm_buzzeroff();
	    alarm.status &= ~ALARM_BEEP;
	}
    } else if(alarm.status & ALARM_CLICK) {
	// make click noise if flag set
	--alarm.buzzer_timer;
	
	if(alarm.buzzer_timer == ALARM_CLICKTIME / 2) {
	    // flip from +5v to -5v on pizo
	    PORTB |=  _BV(PB2);
	    PORTB &= ~_BV(PB1);
	}

	if(alarm.buzzer_timer == 0) {
	    PORTB &= ~_BV(PB2) & ~_BV(PB1);
	    alarm.status &= ~ALARM_CLICK;
	}
    }

    // update alarm status if alarm switch has changed
    static uint8_t alarm_debounce = 0;
    if(PIND & _BV(PD2)) {
	if(alarm.status & ALARM_SET) {
	    alarm_debounce = 0;
	} else {
	    if(++alarm_debounce >= ALARM_DEBOUNCE_TIME) {
		alarm.status |= ALARM_SET;
	    }
	}
    } else {
	if(alarm.status & ALARM_SET) {
	    if(++alarm_debounce >= ALARM_DEBOUNCE_TIME) {
		alarm.status &= ~ALARM_SET & ~ALARM_SOUNDING & ~ALARM_SNOOZE;
		alarm_buzzeroff();
	    }
	} else {
	    alarm_debounce = 0;
	}
    }
}


// set alarm time and save to eeprom
void alarm_settime(uint8_t hour, uint8_t minute) {
    alarm.hour   = hour;
    alarm.minute = minute;
    eeprom_write_byte(&ee_alarm_hour,   alarm.hour  );
    eeprom_write_byte(&ee_alarm_minute, alarm.minute);
}


// set alarm time from eeprom
void alarm_load(void) {
    alarm.hour   = eeprom_read_byte(&ee_alarm_hour)   % 24;
    alarm.minute = eeprom_read_byte(&ee_alarm_minute) % 60;
}


// make a the pizo elemen click
void alarm_click(void) {
    // never beep and click, it could kill someone
    if(alarm.status & ALARM_BEEP) return;

    // +5v to buzzer
    PORTB |=  _BV(PB1);
    PORTB &= ~_BV(PB2);

    // set timer and flag, so click routine can be
    // completed in subsequent calls to alarm_semitick()
    alarm.buzzer_timer =  ALARM_CLICKTIME;
    alarm.status |= ALARM_CLICK;
}


// enable buzzer for the specified number of semiticks
void alarm_beep(uint16_t duration) {
    // never beep and click, it could kill someone
    if(alarm.status & ALARM_CLICK) return;

    // set timer and flag, so click routine can be
    // completed in subsequent calls to alarm_semitick()
    alarm_buzzeron();
    alarm.buzzer_timer = duration;
    alarm.status |= ALARM_BEEP;
}


void alarm_buzzeron(void) {
    power_timer1_enable();  // enable counter1 (buzzer timer)

    // configure Timer/Counter1 to generate buzzer sound:
    //power_timer1_enable();
    // COM1A1 = 10, clear OC1A on Compare Match, set OC1A at BOTTOM
    // COM1B1 = 11, set OC1B on Compare Match, clear OC1B at BOTTOM
    TCCR1A = _BV(COM1A1) | _BV(COM1B1) | _BV(COM1B0) | _BV(WGM11);

    // WGM1 = 1110, fast PWM with TOP is ICR1
    // CS1  = 010,  Timer/Counter1 increments at
    //              system clock / 8 (8 MHz / 8 = 1 MHz)
    TCCR1B = _BV(WGM13 ) | _BV(WGM12 ) | _BV(CS11);

    // set TOP to 250; buzzer frequency is 1 MHz / 250 or 4 kHz.
    ICR1 = 250;

    // set Compare Match to 125; 50% duty cycle for OC1A and OC1B
    OCR1A = OCR1B = 125;
}


void alarm_buzzeroff(void) {
    TCCR1A = 0; TCCR1B = 0;
    power_timer1_disable(); // disable timer/counter1 (buzzer timer)
    PORTB &= ~_BV(PB2) & ~_BV(PB1);  // pull both speaker pins low
}


// TODO: power saving: try setting speaker pins to input

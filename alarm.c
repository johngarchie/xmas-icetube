#include <avr/io.h>           // for using avr register names
#include <avr/pgmspace.h>     // for accessing data in program memory
#include <avr/eeprom.h>       // for accessing data in eeprom
#include <avr/power.h>        // for enabling/disabling microcontroller modules
#include <util/delay_basic.h> // for the _delay_loop_1() macro

#include "alarm.h"
#include "time.h"
#include "power.h"
#include "button.h"
#include "mode.h"


// extern'ed alarm data
volatile alarm_t alarm;


// default alarm time, volume range, and snooze timeout
uint8_t ee_alarm_hour        EEMEM = ALARM_DEFAULT_HOUR;
uint8_t ee_alarm_minute      EEMEM = ALARM_DEFAULT_MINUTE;
uint8_t ee_alarm_volume_min  EEMEM = ALARM_DEFAULT_VOLUME_MIN;
uint8_t ee_alarm_volume_max  EEMEM = ALARM_DEFAULT_VOLUME_MAX;
uint8_t ee_alarm_snooze_time EEMEM = ALARM_DEFAULT_SNOOZE_TIME;
uint8_t ee_alarm_ramp_time   EEMEM = ALARM_DEFAULT_RAMP_TIME;


// The table below is used to convert alarm volume (0 to 10) into timer
// settings.  The values were derived by ear.  With the exception of the first
// two (1 and 7), perceived volume seems roughly proportional to the log of the
// values below.  (cm = compare match)
const uint8_t alarm_vol2cm[] PROGMEM = {1,7,11,15,21,28,38,51,69,93,125};


// initialize alarm after system reset
void alarm_init(void) {
    // load alarm configuration and ensure reasonable values
    alarm.hour         = eeprom_read_byte(&ee_alarm_hour)        % 24;
    alarm.minute       = eeprom_read_byte(&ee_alarm_minute)      % 60;
    alarm.snooze_time  = eeprom_read_byte(&ee_alarm_snooze_time) % 31;
    alarm.ramp_time    = eeprom_read_byte(&ee_alarm_ramp_time)   % 31;
    alarm.volume_max   = eeprom_read_byte(&ee_alarm_volume_max)  % 10;
    alarm.volume_min   = eeprom_read_byte(&ee_alarm_volume_min);

    if(alarm.volume_min > alarm.volume_max) {
	alarm.volume_min = alarm.volume_min;
    }

    if(!alarm.ramp_time) alarm.ramp_time = 1;

    // initial volume should be minimum volume
    alarm.volume = alarm.volume_min;

    // convert snooze time from minutes to seconds
    alarm.snooze_time *= 60;

    // calculate time.ramp_int
    alarm_newramp();

    // set buzzer pins to output now, so the spi subsystem doesn't
    // hijack PB2 (aka Slave Select) when SPCR gets set
    DDRB |= _BV(PB2) | _BV(PB1);

    alarm_sleep(); // configure pins for low-power mode
}


// prepare alarm for sleep
void alarm_sleep(void) {
    // clamp alarm switch pin to ground
    PORTD &= ~_BV(PD2); // disable pull-up resistor
    DDRD  |=  _BV(PD2); // set as ouput, clamped to ground

    // disable buzzer, unless the power management code can
    // handle it properly while sleeping
    if(!(power.status & POWER_ALARMON)) {
	alarm_buzzeroff();
    }
    
    // pull both buzzer pins low
    PORTB &= ~_BV(PB2) & ~_BV(PB1);
}


// initialize alarm after sleep
void alarm_wake(void) {
    // configure the alarm switch pin
    DDRD  &= ~_BV(PD2); // set as input pin
    PORTD |=  _BV(PD2); // enable pull-up resistor

    // give the system time to set PIND
    _delay_loop_1(2);

    // set initial alarm status from alarm switch
    if(PIND & _BV(PD2)) {
	alarm.status |= ALARM_SET;
    } else {
	alarm.status = 0;
    }
}


// sound alarm at the correct time if alarm is set;
// control pizo buzzer
void alarm_tick(void) {
    // sound alarm at correct time if alarm is set
    if((power.status & POWER_SLEEP || alarm.status & ALARM_SET)
	    && time.hour   == alarm.hour
	    && time.minute == alarm.minute
	    && time.second == 0) {

	if(power.status & POWER_SLEEP) {
	    // briefly waking the alarm will update
	    // alarm.status from the alarm switch
	    alarm_wake();
	    alarm_sleep();
	}

	if(alarm.status & ALARM_SET) {
	    alarm.alarm_timer  = 0;
	    alarm.buzzer_timer = 0;
	    alarm.volume = alarm.volume_min;
	    alarm.status |= ALARM_SOUNDING;
	}
    }
    
    // sound alarm if snooze times out
    if(alarm.status & ALARM_SNOOZE) {
	if(++alarm.alarm_timer == alarm.snooze_time) {
	    alarm.alarm_timer  = 0;
	    alarm.buzzer_timer = 0;
	    alarm.volume       = alarm.volume_min;
	    alarm.status      &= ~ALARM_SNOOZE;
	    alarm.status      |=  ALARM_SOUNDING;
	}
    }

    // toggle buzzer if alarm sounding
    if(alarm.status & ALARM_SOUNDING) {
	if(power.status & POWER_SLEEP) {
	    // briefly waking the alarm will update
	    // alarm.status from the alarm switch
	    alarm_wake();
	    alarm_sleep();
	}

	if(alarm.status & ALARM_SOUNDING) {
	    if(time.second % 2) {
		power.status &= ~POWER_ALARMON;
		alarm_buzzeroff();
	    } else {
		power.status |= POWER_ALARMON;
		alarm_buzzeron();
	    }

	    ++alarm.alarm_timer;

	    if(alarm.alarm_timer >= ALARM_SOUNDING_TIMEOUT) {
		alarm.status &= ~ALARM_SOUNDING;
	    }

	    if(alarm.alarm_timer >= alarm.ramp_int
		    && alarm.volume < alarm.volume_max ) {
		++alarm.volume;
	        alarm.alarm_timer = 0;
	    }
	} else {
	    power.status &= ~POWER_ALARMON;
	    alarm_buzzeroff();
	}
    } else {
	power.status &= ~POWER_ALARMON;
    }
}


// control snooze, alarm_beep and alarm_click duration,
// and reads alarm switch
void alarm_semitick(void) {
    // trigger snooze if button pressed during alarm
    if(alarm.snooze_time && alarm.status & ALARM_SOUNDING) {
	if(button_process()) {
	    alarm.status &= ~ALARM_SOUNDING;
	    alarm.status |=  ALARM_SNOOZE;
	    alarm.alarm_timer = 0;
	    alarm_buzzeroff();
	    mode_snoozing();
	}
    }

    // extend snooze if any button is pressed
    if(alarm.status & ALARM_SNOOZE && button.pressed) {
	alarm.alarm_timer = 0;
    }

    if(alarm.status & ALARM_BEEP) {
	// stop buzzer if beep has timed out
	if(--alarm.buzzer_timer == 0) {
	    alarm_buzzeroff();
	    alarm.status &= ~ALARM_BEEP;
	}
    } else if(alarm.status & ALARM_CLICK) {
	// continue making clicking noise
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
		mode_alarmset();
	    }
	}
    } else {
	if(alarm.status & ALARM_SET) {
	    if(++alarm_debounce >= ALARM_DEBOUNCE_TIME) {
		alarm.status &= ~ALARM_SET & ~ALARM_SOUNDING & ~ALARM_SNOOZE;
		alarm_buzzeroff();
		mode_alarmoff();
	    }
	} else {
	    alarm_debounce = 0;
	}
    }
}


// set new time and save time to eeprom
void alarm_settime(uint8_t hour, uint8_t minute) {
    alarm.hour   = hour;
    alarm.minute = minute;
    eeprom_write_byte(&ee_alarm_hour,   alarm.hour  );
    eeprom_write_byte(&ee_alarm_minute, alarm.minute);
}


// compute new ramp interval from ramp time
void alarm_newramp(void) {
    alarm.ramp_int = alarm.ramp_time * 60
			 / (alarm.volume_max - alarm.volume_min + 1);
}


// save alarm volume to eeprom
void alarm_savevolume(void) {
    eeprom_write_byte(&ee_alarm_volume_min, alarm.volume_min);
    eeprom_write_byte(&ee_alarm_volume_max, alarm.volume_max);
}

// save ramp interval to eeprom
void alarm_saveramp(void) {
    eeprom_write_byte(&ee_alarm_ramp_time, alarm.ramp_time);
}


// save alarm snooze time (in seconds) to eeprom (in minutes)
void alarm_savesnooze(void) {
    // save snooze time as minutes, not seconds
    eeprom_write_byte(&ee_alarm_snooze_time, alarm.snooze_time / 60);
}


// make a clicking sound
void alarm_click(void) {
    // never click while beeping, it's dangerous
    if(alarm.status & ALARM_BEEP) return;

    // +5v to buzzer
    PORTB |=  _BV(PB1);
    PORTB &= ~_BV(PB2);

    // set timer and flag, so click routine can be
    // completed in subsequent calls to alarm_semitick()
    alarm.buzzer_timer =  ALARM_CLICKTIME;
    alarm.status |= ALARM_CLICK;
}


// beep for the specified duration (semiseconds)
void alarm_beep(uint16_t duration) {
    // let a beep override a click
    if(alarm.status & ALARM_CLICK) {
	PORTB &= ~_BV(PB2) & ~_BV(PB1); // pull both speaker pins low
	alarm.status &= ~ALARM_CLICK;
    }

    // set timer and flag, so click routine can be
    // completed in subsequent calls to alarm_semitick()
    alarm_buzzeron();
    alarm.buzzer_timer = duration;
    alarm.status |= ALARM_BEEP;
}


// enable the pizo buzzer
void alarm_buzzeron(void) {
    power_timer1_enable();  // enable counter1 (buzzer timer)

    // configure Timer/Counter1 to generate buzzer sound:
    // COM1A1 = 10, clear OC1A on Compare Match, set OC1A at BOTTOM
    // COM1B1 = 11, set OC1B on Compare Match, clear OC1B at BOTTOM
    TCCR1A = _BV(COM1A1) | _BV(COM1B1) | _BV(COM1B0) | _BV(WGM11);

    if(power.status & POWER_SLEEP) {
	// WGM1 = 1110, fast PWM with TOP is ICR1
	// CS1  = 010,  Timer/Counter1 increments at
	//              system clock / 1 (2 MHz)
	TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS10);

	// set buzzer frequency to 2 MHz / 500 = 4.00 kHz
	ICR1 = 500;

	// set compare match registers for desired volume; adjust
	// volume to compensate for lower voltage of battery power
	uint16_t extra_volume = alarm.volume + 1;
	if(extra_volume > 10) extra_volume = 10;
	uint16_t compare_match = pgm_read_byte(alarm_vol2cm + extra_volume);
	compare_match *= 2;
	OCR1A = compare_match;
	OCR1B = 500 - compare_match;
    } else {
	// WGM1 = 1110, fast PWM with TOP is ICR1
	// CS1  = 010,  Timer/Counter1 increments at
	//              system clock / 8 (8 MHz / 8 = 1 MHz)
	TCCR1B = _BV(WGM13 ) | _BV(WGM12 ) | _BV(CS11);

	// set buzzer frequency to 1 MHz / 250 = 4.00 kHz
	ICR1 = 250;

	// set compare match registers for desired volume
	uint16_t compare_match = pgm_read_byte(alarm_vol2cm + alarm.volume);
	OCR1A = compare_match;
	OCR1B = 250 - compare_match;
    }
}


// disable the pizo buzzer
void alarm_buzzeroff(void) {
    TCCR1A = 0; TCCR1B = 0;
    power_timer1_disable(); // disable timer/counter1 (buzzer timer)
    PORTB &= ~_BV(PB2) & ~_BV(PB1);  // pull both speaker pins low
}

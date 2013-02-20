// display.c  --  low-level control of VFD display
// (Display contents are controlled in mode.c)
//
//    PB5 (SCK)                      MAX6921 CLK pin
//    PB3 (MOSI)                     MAX6921 DIN pin
//    PC5                            photoresistor pull-up
//    PC4 (ADC4)                     photoresistor voltage
//    PC3                            MAX6921 BLANK pin
//    PC0                            MAX6921 LOAD pin
//    PD6                            boost transistor
//    PD3                            vfd power transistor
//    counter/timer0                 boost (PD6) and semiticks
//    analog to digital converter    photoresistor (ADC4) voltage
//


#include <avr/io.h>       // for using avr register names
#include <avr/pgmspace.h> // for accessing data in program memory
#include <avr/eeprom.h>   // for accessing data in eeprom memory
#include <avr/power.h>    // for enabling/disabling chip features
#include <util/atomic.h>  // for using atomic blocks


#include "display.h"
#include "usart.h"    // for debugging output
#include "system.h"   // for determining system status
#include "time.h"     // for determing current time


// extern'ed data pertaining the display
volatile display_t display;


uint8_t display_combineLR(uint8_t a, uint8_t b);
uint8_t display_shiftU1(uint8_t digit);
uint8_t display_shiftU2(uint8_t digit);
uint8_t display_shiftD1(uint8_t digit);
uint8_t display_shiftD2(uint8_t digit);


// permanent place to store display brightness
uint8_t ee_display_status     EEMEM = DISPLAY_ANIMATED;
#ifdef AUTOMATIC_DIMMER
uint8_t ee_display_bright_min EEMEM = 1;
uint8_t ee_display_bright_max EEMEM = 1;
uint8_t ee_display_off_threshold EEMEM = UINT8_MAX;
#else
uint8_t ee_display_brightness EEMEM = 1;
#endif  // AUTOMATIC_DIMMER
uint8_t ee_display_digit_times[] EEMEM = {15, 15, 15, 15, 15, 15, 15, 15, 15};
uint8_t ee_display_gradient EEMEM = 0;

uint8_t ee_display_off_hour   EEMEM = 23 | DISPLAY_NOOFF;
uint8_t ee_display_off_minute EEMEM = 0;
uint8_t ee_display_on_hour    EEMEM = 6;
uint8_t ee_display_on_minute  EEMEM = 0;

uint8_t ee_display_off_days EEMEM = 0;
uint8_t ee_display_on_days  EEMEM = 0;


// display of letters and numbers is coded by 
// the appropriate segment flags:
#define SEG_A 0x80  //
#define SEG_B 0x40  //        AAA
#define SEG_C 0x20  //       F   B
#define SEG_D 0x10  //       F   B
#define SEG_E 0x08  //        GGG
#define SEG_F 0x04  //       E   C
#define SEG_G 0x02  //       E   C
#define SEG_H 0x01  //        DDD  H

#define DISPLAY_SPACE    0
#define DISPLAY_DOT      SEG_H
#define DISPLAY_DASH     SEG_G
#define DISPLAY_SLASH    SEG_B | SEG_G | SEG_E
#define DISPLAY_WILDCARD SEG_A | SEG_G | SEG_D

// codes for vfd letter display
const uint8_t letter_segments[] PROGMEM = {
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_G, // a
    SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,         // b
    SEG_D | SEG_E | SEG_G,                         // c
    SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,         // d
    SEG_A | SEG_B | SEG_D | SEG_E | SEG_F | SEG_G, // e
    SEG_A | SEG_E | SEG_F | SEG_G,                 // f
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G, // g
    SEG_C | SEG_E | SEG_F | SEG_G,                 // h
    SEG_B | SEG_C,                                 // i
    SEG_B | SEG_C | SEG_D | SEG_E,                 // j
    SEG_A | SEG_C | SEG_E | SEG_F | SEG_G,         // k
    SEG_D | SEG_E | SEG_F,                         // l
    SEG_A | SEG_C | SEG_E | SEG_G,                 // m
    SEG_C | SEG_E | SEG_G,                         // n
    SEG_C | SEG_D | SEG_E | SEG_G,                 // o
    SEG_A | SEG_B | SEG_E | SEG_F | SEG_G,         // p
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_G,         // q
    SEG_E | SEG_G,                                 // r
    SEG_A | SEG_C | SEG_D | SEG_F | SEG_G,         // s
    SEG_D | SEG_E | SEG_F | SEG_G,                 // t
    SEG_C | SEG_D | SEG_E,                         // u
    SEG_C | SEG_D | SEG_E,                         // v
    SEG_A | SEG_C | SEG_D | SEG_E,                 // w
    SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,         // x
    SEG_B | SEG_C | SEG_D | SEG_F | SEG_G,         // y
    SEG_A | SEG_B | SEG_D | SEG_E | SEG_G,         // z
};

const uint8_t number_segments[] PROGMEM = {
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,         // 0
    SEG_B | SEG_C,                                         // 1
    SEG_A | SEG_B | SEG_D | SEG_E | SEG_G,                 // 2
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_G,                 // 3
    SEG_B | SEG_C | SEG_F | SEG_G,                         // 4
    SEG_A | SEG_C | SEG_D | SEG_F | SEG_G,                 // 5
    SEG_A | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,         // 6
    SEG_A | SEG_B | SEG_C,                                 // 7
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G, // 8
    SEG_A | SEG_B | SEG_C | SEG_F | SEG_G,                 // 9
};

// vfd digit selection wires are on these MAX6921 pins
const uint8_t vfd_digit_pins[] PROGMEM = {
    3,  // digit 9 (dash & circle)
    7,  // digit 8 (leftmost digit)
    8,  // digit 7
    9,  // digit 6
    6,  // digit 5
    10, // digit 4
    5,  // digit 3
    12, // digit 2
    4,  // digit 1 (rightmost digit)
};

// vfd segment selection wires are on these MAX6921 pins
const uint8_t vfd_segment_pins[] PROGMEM = {
    11, // segment H
    16, // segment G
    18, // segment F
    15, // segment E
    13, // segment D
    14, // segment C
    17, // segment B
    19, // segment A
};


// initialize display after system reset
void display_init(void) {
    // if any timer is disabled during sleep, the system locks up sporadically
    // and nondeterministically, so enable timer0 in PRR and leave it alone!
    power_timer0_enable();

    // disable boost and vfd
    DDRD  |= _BV(PD6) | _BV(PD3); // enable boost fet and vfd transistor
    PORTD &= ~_BV(PD6); // boost fet off (pull low)
    PORTD |=  _BV(PD3); // MAX6921 power off (push high)

    // configure MAX6921 load and blank pins
    DDRC  |=  _BV(PC0) |  _BV(PC3); // set to output
    PORTC &= ~_BV(PC0) & ~_BV(PC3); // clamp to ground

    // configure spi sck and mosi pins as floating inputs
    // (these pins seem to use less power when configured
    // as inputs *without* pull-ups!?!?)
    DDRB  &= ~_BV(PB5) & ~_BV(PB3);  // set to input

    // configure photoresistor pull-up pin
    DDRC  |=  _BV(PC5);  // set pin as output
    PORTC &= ~_BV(PC5);  // pull to ground

    // disable digital circuitry on photoresistor pins
    DIDR0 |= _BV(ADC5D) | _BV(ADC4D);

#ifdef AUTOMATIC_DIMMER
    // set initial photo_avg to maximum ambient light
    display.photo_avg = UINT16_MAX;

    // load the display-off threshold
    display_loadphotooff();
#endif  // AUTOMATIC_DIMMER

    // load the digit display times
    display_loaddigittimes();

    // load the auto off settings
    display_loadofftime();
    display_loadoffdays();
    display_loadondays();

    // set the initial transition type to none
    display.trans_type = DISPLAY_TRANS_NONE;

    // load display status
    display_loadstatus();
}


// returns true if display is disabled
uint8_t display_onbutton(void) {
    uint8_t status_old;
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
	status_old = display.status;
	display_on();
	display.off_timer = DISPLAY_OFF_TIMEOUT;
    }
    return status_old & DISPLAY_DISABLED;
}


// enable display after low-power mode
void display_wake(void) {
    // enable external and internal photoresistor pull-ups (the latter
    // ensures a defined value if no photoresistor installed)
    PORTC |= _BV(PC5) | _BV(PC4);  // output +5v, enable pull-up

    // enable analog to digital converter
    power_adc_enable();

    // select ADC4 as analog to digital input
    //   MUX3:0 = 0100:  ADC4 as input
    ADMUX = _BV(MUX2);

    // configure analog to digital converter
    // ADEN    =   1:  enable analog to digital converter
    // ADSC    =   1:  start ADC conversion now
    // ADPS2:0 = 110:  system clock / 64  (8 MHz / 4 = 125 kHz)
    ADCSRA = _BV(ADEN) | _BV(ADSC) | _BV(ADPS2) | _BV(ADPS1);

    // configure spi sck and mosi pins as outputs
    DDRB |= _BV(PB5) | _BV(PB3);

    // configure and start Timer/Counter0
    // COM0A1:0 = 10: clear OC0A on compare match; set at BOTTOM
    // WGM02:0 = 011: clear timer on compare match; TOP = 0xFF
    TCCR0A = _BV(COM0A1) | _BV(WGM00) | _BV(WGM01);
    TCCR0B = _BV(CS00);   // clock counter0 with system clock
    TIMSK0 = _BV(TOIE0);  // enable counter0 overflow interrupt

    // set OCR0A for desired display brightness
    display_loadbright();

    // MAX6921 power on (pull low)
    PORTD &= ~_BV(PD3);
}


// disable display for low-power mode
void display_sleep(void) {
    // disable Timer/Counter0
    TCCR0A = TCCR0B = 0;

    PORTD &= ~_BV(PD6); // boost fet off (pull low)
    PORTD |=  _BV(PD3); // MAX6921 power off (pull high)

    // disable external and internal photoresistor pull-ups
    PORTC &= ~_BV(PC5) & ~_BV(PC4);  // pull to ground, disable pull-up

    // disable analog to digital converter
    ADCSRA = 0;  // disable ADC before power_adc_disable()
    power_adc_disable();

    // configure MAX6921 LOAD and BLANK pins
    PORTC &= ~_BV(PC0) & ~_BV(PC3); // clamp to ground

    // configure MAX6921 CLK and DIN pins
    // (these pins seem to use less power when configured
    // as inputs *without* pull-ups!?!?)
    DDRB  &= ~_BV(PB5) & ~_BV(PB3);  // set as input
    PORTB &= ~_BV(PB5) & ~_BV(PB3);  // disable pull-ups
}


// decrements display-off timer
void display_tick(void) {
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
	if(display.off_timer) {
	    --display.off_timer;
	    return;
	}
    }

#ifdef AUTOMATIC_DIMMER
    // disable display if dark
    if((display.photo_avg >> 8) > display.off_threshold) {
	display_off();
	return;
    }
#endif  // AUTOMATIC_DIMMER

    // determine day-of-week flag for today
    uint8_t dowflag = _BV(time_dayofweek(time.year, time.month, time.day));

    // disable display if today is an "off day"
    if(display.off_days & dowflag) {
	display_off();
	return;
    }

    // consider time only if today is not an "on day"
    if(!(display.on_days & dowflag) && !(display.on_hour & DISPLAY_NOOFF)) {
	// if off time period does not span midnight
	if(display.off_hour < display.on_hour
		|| (   display.off_hour   == display.on_hour
		    && display.off_minute <= display.on_minute)) {
	    // if current time within off time period
	    if(        (display.off_hour < time.hour
			|| (   display.off_hour   == time.hour
			    && display.off_minute <  time.minute))
		    && (time.hour < display.on_hour
			|| (   time.hour   == display.on_hour
			    && time.minute <  display.on_minute))) {
		display_off();
		return;
	    }
	} else {  // off time period spans midnight
	    // if current time within off period
	    if(        (display.off_hour < time.hour
			|| (   display.off_hour   == time.hour
			    && display.off_minute <  time.minute))
		    || (time.hour < display.on_hour
			|| (   time.hour   == display.on_hour
			    && time.minute <  display.on_minute))) {
		display_off();
		return;
	    }
	}
    }

#ifdef AUTOMATIC_DIMMER
    // enable display if light or if dimmer threshold disabled;
    // otherwise maintain current on/off setting
    if((display.photo_avg >> 8) < display.off_threshold
	    || display.off_threshold == UINT8_MAX) {
	display_on();
    }

#else
    display_on();
#endif  // AUTOMATIC_DIMMER
}


// disable display
void display_off(void) {
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
	if(!(system.status & SYSTEM_SLEEP)
		&& !(display.status & DISPLAY_DISABLED)) {
	    display.status |=  DISPLAY_DISABLED;
	    TCCR0A = _BV(WGM00) | _BV(WGM01);
	    PORTD &= ~_BV(PD6);  // boost fet off (pull low)
	    PORTD |=  _BV(PD3);  // MAX6921 power off (pull high)
	}
    }
}


// enable display
void display_on(void) {
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
	if(!(system.status & SYSTEM_SLEEP)
		&& (display.status & DISPLAY_DISABLED)) {
	    display.status &= ~DISPLAY_DISABLED;
	    TCCR0A = _BV(COM0A1) | _BV(WGM00) | _BV(WGM01);
	    PORTD &= ~_BV(PD3);  // MAX6921 power on (pull low)
	}
    }
}


// called periodically to to control the VFD via the MAX6921
// returns time (in 32us units) to display current digit
uint8_t display_varsemitick(void) {
    static uint8_t digit_idx = DISPLAY_SIZE - 1;

    // bits to send MAX6921 (vfd driver chip)
    uint8_t bits[3] = {0, 0, 0};

    // calculate digit contents given transition state
    uint8_t digit = display.postbuf[digit_idx];

    switch(display.trans_type) {
	case DISPLAY_TRANS_UP:
	    switch(display.trans_timer) {
		case 4:
		    digit = display_shiftU1(display.postbuf[digit_idx]);
		    break;

		case 3:
		    digit = display_shiftU2(display.postbuf[digit_idx]);
		    break;

		case 2:
		    digit = display_shiftD2(display.prebuf[digit_idx]);
		    break;

		case 1:
		    digit = display_shiftD1(display.prebuf[digit_idx]);
		    break;

		default:
		    break;
	    }
	    break;

	case DISPLAY_TRANS_DOWN:
	    switch(display.trans_timer) {
		case 4:
		    digit = display_shiftD1(display.postbuf[digit_idx]);
		    break;

		case 3:
		    digit = display_shiftD2(display.postbuf[digit_idx]);
		    break;

		case 2:
		    digit = display_shiftU2(display.prebuf[digit_idx]);
		    break;

		case 1:
		    digit = display_shiftU1(display.prebuf[digit_idx]);
		    break;

		default:
		    break;
	    }
	    break;

	case DISPLAY_TRANS_LEFT:
	    if(display.trans_timer < 2 * DISPLAY_SIZE) {

		uint8_t trans_idx =   DISPLAY_SIZE
				    - (display.trans_timer >> 1)
		                    + digit_idx;

		// treat 0th digit as blank during transitions
		if(trans_idx == DISPLAY_SIZE) {
		    digit = 0;
		    break;
		}

		uint8_t digit_b = (trans_idx < DISPLAY_SIZE
			           ? display.postbuf[trans_idx]
			           : display.prebuf[trans_idx - DISPLAY_SIZE]);

		if(display.trans_timer & 0x01) {
		    uint8_t digit_a = (--trans_idx < DISPLAY_SIZE
			               ? display.postbuf[trans_idx]
			               : display.prebuf[trans_idx
				       			- DISPLAY_SIZE]);

		    digit = display_combineLR(digit_a, digit_b);
		} else {
		    digit = digit_b;
		}
	    }
	    break;

	default:
	    break;
    }

    // do not display first digit when transitioning
    if(display.trans_type != DISPLAY_TRANS_NONE && !digit_idx) {
	digit = 0;
    }


    // create the sequence of bits for the calculated digit
    if(!(display.status & DISPLAY_DISABLED)) {
	// select the digit position to display
	uint8_t bitidx = pgm_read_byte(&(vfd_digit_pins[digit_idx]));
	bits[bitidx >> 3] |= _BV(bitidx & 0x7);

	// select the segments to display
	for(uint8_t segment = 0; segment < 8; ++segment) {
	    if(digit & _BV(segment)) {
		bitidx = pgm_read_byte(&(vfd_segment_pins[segment]));
		bits[bitidx >> 3] |= _BV(bitidx & 0x7);
	    }
	}
    }


    // send bits to the MAX6921 (vfd driver chip)

    // Note that one system clock cycle is 1 / 8 MHz seconds or 125 ns.
    // According to the MAX6921 datasheet, the minimum pulse-width on the
    // MAX6921 CLK and LOAD pins need only be 90 ns and 55 ns,
    // respectively.  The minimum period for CLK must be at least 200 ns.
    // Therefore, no delays should be necessary in the code below.

    // Also, the bits could be sent by SPI (they are in the origional
    // Adafruit firmware), but I have found that doing so sometimes
    // results in display flicker.
    uint8_t bitflag = 0x08;
    for(int8_t bitidx=2; bitidx >= 0; --bitidx) {
        uint8_t bitbyte = bits[bitidx];

        for(; bitflag; bitflag >>= 1) {
            if(bitbyte & bitflag) {
                // output high on MAX6921 DIN pin
                PORTB |= _BV(PB3);
            } else {
                // output low on MAX6921 DIN pin
                PORTB &= ~_BV(PB3);
            }

            // pulse MAX6921 CLK pin:  shifts DIN input into
            // the 20-bit shift register on rising edge
            PORTB |=  _BV(PB5);
            PORTB &= ~_BV(PB5);
        }

	bitflag = 0x80;
    }
    
    // pulse MAX6921 LOAD pin:  transfers shift
    // register to latch when high; latches when low
    PORTC |=  _BV(PC0);
    PORTC &= ~_BV(PC0);

    // amount of time to display current digit
    uint8_t digit_time = display.digit_times[digit_idx];

    // apply flicker reduction shift
    digit_time >>= display.digit_time_shift;

    // compute gradient correction scaler
    static uint16_t gradient_correction;
    if(digit_idx >= DISPLAY_SIZE - 1) {
	// set to maximum on first digit
	gradient_correction = UINT16_MAX;
    } else {
	// calculate next step of exponentional decay
	gradient_correction -= display.gradient * (gradient_correction >> 6);
    }

    // apply gradient correction scaler to digit_time
    digit_time = (((gradient_correction >> 8) * digit_time) >> 8);

    // next time, display previous digit
    if(!digit_idx) {
	digit_idx = DISPLAY_SIZE - 1;
    } else {
	--digit_idx;
    }

    return digit_time;
}


// called every semisecond; updates ambient brightness running average
void display_semitick(void) {
    // Update the display transition variables as time passes:
    // During a transition, display_varsemitick() calculates the segments
    // to display on-the-fly from the transition variables.

    static uint8_t trans_delay_timer = 0;

    // calculate timer values for scrolling display
    if(display.trans_timer) {
	if(trans_delay_timer) {
	    --trans_delay_timer;
	} else {
	    if(--display.trans_timer) {
		switch(display.trans_type) {
		    case DISPLAY_TRANS_UP:
		    case DISPLAY_TRANS_DOWN:
			trans_delay_timer = DISPLAY_TRANS_UD_DELAY;
			break;

		    case DISPLAY_TRANS_LEFT:
			trans_delay_timer = DISPLAY_TRANS_LR_DELAY;
			break;

		    default:
			break;
		}
	    } else {
		ATOMIC_BLOCK(ATOMIC_FORCEON) {
		    for(uint8_t i = 0; i < DISPLAY_SIZE; ++i) {
			display.postbuf[i] = display.prebuf[i];
		    }

		    display.trans_type = DISPLAY_TRANS_NONE;
		}
	    }
	}
    }


#ifdef AUTOMATIC_DIMMER
    // get ambient lighting from photosensor every 16 semiseconds,
    // and update running average of photosensor values; note that
    // the running average has a range of [0, 0xFFFF]
    static uint8_t photo_timer = 16;

    if(!--photo_timer) {
	// repeat in 16 semiseconds
        photo_timer = 16;

	// update adc running average (display.photo_avg)
	display.photo_avg -= (display.photo_avg >> 6);
	display.photo_avg += ADC;

        // begin next analog to digital conversion
        ADCSRA |= _BV(ADSC);

	// update brightness from display.photo_avg if not pulsing
	if(!(display.status & DISPLAY_PULSING)) display_autodim();
    }
#endif  // AUTOMATIC_DIMMER


    // update display brightness if pulsing
    if(display.status & DISPLAY_PULSING) {
	static uint8_t pulse_timer = DISPLAY_PULSE_DELAY;

	if(! --pulse_timer) {
	    pulse_timer = DISPLAY_PULSE_DELAY;

	    if(display.status & DISPLAY_PULSE_DOWN) {
		if(OCR0A <= 20) {
		    display.status &= ~DISPLAY_PULSE_DOWN;
		} else {
		    --OCR0A;
		}
	    } else {
		if(OCR0A >= 90) {
		    display.status |=  DISPLAY_PULSE_DOWN;
		} else {
		    ++OCR0A;
		}
	    }
	}
    }
}


// save status to eeprom
void display_savestatus(void) {
    eeprom_write_byte(&ee_display_status, display.status
	    				  & DISPLAY_SETTINGS_MASK);
}


// load status from eeprom
void display_loadstatus(void) {
    display.status &= ~DISPLAY_SETTINGS_MASK;
    display.status |= eeprom_read_byte(&ee_display_status);
}


// clear the given display position
void display_clear(uint8_t idx) {
    display.prebuf[idx] = DISPLAY_SPACE;
}


// clears entire display
void display_clearall(void) {
    for(uint8_t i = 0; i < DISPLAY_SIZE; ++i) {
	display.prebuf[i] = DISPLAY_SPACE;
    }
}


// display the given program memory string at the given display index
void display_pstr(const uint8_t idx, PGM_P pstr) {
    uint8_t pstr_idx = 0;
    uint8_t disp_idx = idx;

    // clear first display position if idx is zero
    if(disp_idx == 0) display_clear(disp_idx++);

    // display the string
    char c = pgm_read_byte( &(pstr[pstr_idx++]) );
    while(c && disp_idx < DISPLAY_SIZE) {
	display_char(disp_idx++, c);
        c = pgm_read_byte( &(pstr[pstr_idx++]) );
    }

    // clear any remaining positions if idx is zero
    if(!idx) {
	for(; disp_idx < DISPLAY_SIZE; ++disp_idx) {
	    display_clear(disp_idx);
	}
    }
}


// load display brightness from eeprom
void display_loadbright(void) {
#ifdef AUTOMATIC_DIMMER
    display.bright_min = eeprom_read_byte(&ee_display_bright_min);
    display.bright_max = eeprom_read_byte(&ee_display_bright_max);
#else
    display.brightness = eeprom_read_byte(&ee_display_brightness);
#endif  // AUTOMATIC_DIMMER
    display_autodim();
}


// save display brightness to eeprom
void display_savebright(void) {
#ifdef AUTOMATIC_DIMMER
    eeprom_write_byte(&ee_display_bright_min, display.bright_min);
    eeprom_write_byte(&ee_display_bright_max, display.bright_max);
#else
    eeprom_write_byte(&ee_display_brightness, display.brightness);
#endif  // AUTOMATIC_DIMMER
}


// loads the times (32 us units) to display each digit
void display_loaddigittimes(void) {
    for(uint8_t i = 0; i < DISPLAY_SIZE; ++i) {
	display.digit_times[i] = eeprom_read_byte(&(ee_display_digit_times[i]));
    }

    display.gradient = eeprom_read_byte(&ee_display_gradient);

    display_noflicker();
}


// saves the times (32 us units) to display each digit
void display_savedigittimes(void) {
    for(uint8_t i = 0; i < DISPLAY_SIZE; ++i) {
	eeprom_write_byte(&(ee_display_digit_times[i]), display.digit_times[i]);
    }

    eeprom_write_byte(&ee_display_gradient, display.gradient);
}


// calculates new digit time shift to prevent flicker
void display_noflicker(void) {
    uint16_t total_digit_time = 0;

    for(uint8_t i = 0; i < DISPLAY_SIZE; ++i) {
	total_digit_time += display.digit_times[i];
    }

    display.digit_time_shift = 0;

    while( (total_digit_time >> display.digit_time_shift) > 512 ) {
	++display.digit_time_shift;
    }
}


#ifdef AUTOMATIC_DIMMER
// load the display-off threshold
void display_loadphotooff(void) {
    display.off_threshold = eeprom_read_byte(&ee_display_off_threshold);
}


// save the display-off threshold
void display_savephotooff(void) {
    eeprom_write_byte(&ee_display_off_threshold, display.off_threshold);
}
#endif  // AUTOMATIC_DIMMER


// load the display-off time period
void display_loadofftime(void) {
    display.off_hour   = eeprom_read_byte(&ee_display_off_hour);
    display.off_minute = eeprom_read_byte(&ee_display_off_minute);
    display.on_hour    = eeprom_read_byte(&ee_display_on_hour);
    display.on_minute  = eeprom_read_byte(&ee_display_on_minute);
}


// save the display-off time period
void display_saveofftime(void) {
    eeprom_write_byte(&ee_display_off_hour,   display.off_hour);
    eeprom_write_byte(&ee_display_off_minute, display.off_minute);
    eeprom_write_byte(&ee_display_on_hour,    display.on_hour);
    eeprom_write_byte(&ee_display_on_minute,  display.on_minute);
}


// load the display-off days
void display_loadoffdays(void) {
    display.off_days = eeprom_read_byte(&ee_display_off_days);
}


// save the display-off days
void display_saveoffdays(void) {
    eeprom_write_byte(&ee_display_off_days, display.off_days);
}


// load the display-on days
void display_loadondays(void) {
    display.on_days = eeprom_read_byte(&ee_display_on_days);
}


// save the display-on days
void display_saveondays(void) {
    eeprom_write_byte(&ee_display_on_days, display.on_days);
}


// set display brightness from display.bright_min,
// display.bright_max, and display.photo_avg
void display_autodim(void) {
#ifdef AUTOMATIC_DIMMER
    // convert photoresistor value to 20-90 for OCR0A
    int16_t new_OCR0A = OCR0A_MIN + OCR0A_SCALE * display.bright_max
			 - ( (display.photo_avg >> 8) * 7
                             * (display.bright_max - display.bright_min) >> 8);
#else
    int16_t new_OCR0A = OCR0A_MIN + OCR0A_SCALE * display.brightness;
#endif  // AUTOMATIC_DIMMER

    // ensure display will not be too dim or too bright:
    // if too dim, low voltage may not light digit segments;
    // if too bright, excessive voltage may damage display.
    if(new_OCR0A < 20) new_OCR0A = OCR0A_MIN;
    if(new_OCR0A > 90) new_OCR0A = OCR0A_MIN + OCR0A_SCALE * 10;

    // set new brightness
    OCR0A = new_OCR0A;
}


// display digit (n) on display position (idx)
void display_digit(uint8_t idx, uint8_t n) {
    display.prebuf[idx] = pgm_read_byte( &(number_segments[(n % 10)]) );

    if(n == 9 && (display.status & DISPLAY_ALTNINE)) {
	display.prebuf[idx] |= SEG_D;
    }
}


// display zero-padded two-digit number (n)
// at display positions (idx & idx+1)
void display_twodigit_rightadj(uint8_t idx, int8_t n) {
    if(display.status & DISPLAY_ZEROPAD) {
	display_twodigit_zeropad(idx, n);
    } else {
	if(n < 0) {
	    display_char(idx, '-');
	    n *= -1;
	} else if(n < 10) {
	    display_clear(idx);
	} else {
	    display_digit(idx, n / 10);
	}

	display_digit(++idx, n % 10);
    }
}


// display zero-padded two-digit number (n)
// at display positions (idx & idx+1)
void display_twodigit_leftadj(uint8_t idx, int8_t n) {
    if(display.status & DISPLAY_ZEROPAD) {
	display_twodigit_zeropad(idx, n);
    } else {
	if(n < 0) {
	    display_char(idx++, '-');
	    n *= -1;
	} else if(n < 10) {
	    display_clear(idx + 1);
	} else {
	    display_digit(idx++, n / 10);
	}

	display_digit(idx, n % 10);
    }
}


// display zero-padded two-digit number (n)
// at display positions (idx & idx+1)
void display_twodigit_zeropad(uint8_t idx, int8_t n) {
    if(n < 0) {
	display_char(   idx, '-');
	display_digit(++idx, n * -1);
    } else {
	display_digit(  idx, n / 10);
	display_digit(++idx, n % 10);
    }
}


// display character (c) at display position (idx)
void display_char(uint8_t idx, char c) {
    if('a' <= c && c <= 'z') {
	display.prebuf[idx] = pgm_read_byte( &(letter_segments[c - 'a']) );
    } else if('A' <= c && c <= 'Z') {
	display.prebuf[idx] = pgm_read_byte( &(letter_segments[c - 'A']) );
    } else if('0' <= c && c <= '9') {
	display_digit(idx, c - '0');
    } else {
	switch(c) {
	    case ' ':
		display.prebuf[idx] = DISPLAY_SPACE;
		break;
	    case '-':
		display.prebuf[idx] = DISPLAY_DASH;
		break;
	    case '/':
		display.prebuf[idx] = DISPLAY_SLASH;
		break;
	    default:
		display.prebuf[idx] = DISPLAY_WILDCARD;
		break;
	}
    }
}


// displays decimals after displayable characters
// between idx_start and idx_end, inclusive
void display_dotselect(uint8_t idx_start, uint8_t idx_end) {
    for(uint8_t idx = idx_start; idx <= idx_end && idx < DISPLAY_SIZE; ++idx) {
	if(display.prebuf[idx] & ~SEG_G & ~SEG_H) {
	    display.prebuf[idx] |= DISPLAY_DOT;
	}
    }
}


// if show is true, displays dot at specified display position (idx)
// if show is false, clears dot at specified display position (idx)
void display_dot(uint8_t idx, uint8_t show) {
    if(show) {
	display.prebuf[idx] |= DISPLAY_DOT;
    } else {
	display.prebuf[idx] &= ~DISPLAY_DOT;
    }
}


// if show is true, displays dash at specified display position (idx)
// if show is false, clears dash at specified display position (idx)
void display_dash(uint8_t idx, uint8_t show) {
    if(show) {
	display.prebuf[idx] |= DISPLAY_DASH;
    } else {
	display.prebuf[idx] &= ~DISPLAY_DASH;
    }
}


// formats and displays seconds as a dial
// at the specified diplays position (idx)
void display_dial(uint8_t idx, uint8_t seconds) {
    uint8_t segs = 0;

    if(seconds < 10) {
        segs |= SEG_A;
    } else if(seconds < 20) {
	segs |= SEG_B;
    } else if(seconds < 30) {
	segs |= SEG_C;
    } else if(seconds < 40) {
	segs |= SEG_D;
    } else if(seconds < 50) {
	segs |= SEG_E;
    } else {
	segs |= SEG_F;
    }

    if(seconds & 0x01) {
	segs |= SEG_G;
    }

    display.prebuf[idx] = segs;
}


// loads transitions display to prebuffered contents
// using the spefied transition type
void display_transition(uint8_t type) {
    // all transitions should be instant when animation disabled
    if(!(display.status & DISPLAY_ANIMATED)) {
	type = DISPLAY_TRANS_INSTANT;
    }
    
    // do nothing if transition already in-progress
    if(display.trans_timer) return;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
	display.trans_type = type;

	switch(display.trans_type) {
	    case DISPLAY_TRANS_UP:
	    case DISPLAY_TRANS_DOWN:
		display.trans_timer = 5;
		break;

	    case DISPLAY_TRANS_LEFT:
		display.trans_timer = 18;
		break;

	    case DISPLAY_TRANS_INSTANT:
		    for(uint8_t i = 0; i < DISPLAY_SIZE; ++i) {
			display.postbuf[i] = display.prebuf[i];
		    }

		    display.trans_type = DISPLAY_TRANS_NONE;
		    break;

	    default:
		break;
	}
    }
}


// utility function for display_varsemitick();
// combines two characters for the scroll-left transition
uint8_t display_combineLR(uint8_t a, uint8_t b) {
    uint8_t c = 0;

    if(a & SEG_B) c |= SEG_F;
    if(a & SEG_E) c |= SEG_E;
    if(b & SEG_F) c |= SEG_B;
    if(b & SEG_E) c |= SEG_C;

    return c;
}


// utility function for display_varsemitick();
// shifts the given digit up by one
uint8_t display_shiftU1(uint8_t digit) {
    uint8_t shifted = 0;

    if(digit & SEG_G) shifted |= SEG_A;
    if(digit & SEG_E) shifted |= SEG_F;
    if(digit & SEG_C) shifted |= SEG_B;
    if(digit & SEG_D) shifted |= SEG_G;

    return shifted;
}


// utility function for display_varsemitick();
// shifts the given digit up by two
uint8_t display_shiftU2(uint8_t digit) {
    uint8_t shifted = 0;

    if(digit & SEG_D) shifted |= SEG_A;

    return shifted;
}


// utility function for display_varsemitick();
// shifts the given digit down by one
uint8_t display_shiftD1(uint8_t digit) {
    uint8_t shifted = 0;

    if(digit & SEG_A) shifted |= SEG_G;
    if(digit & SEG_F) shifted |= SEG_E;
    if(digit & SEG_B) shifted |= SEG_C;
    if(digit & SEG_G) shifted |= SEG_D;

    return shifted;
}


// utility function for display_varsemitick();
// shifts the given digit down by two
uint8_t display_shiftD2(uint8_t digit) {
    uint8_t shifted = 0;

    if(digit & SEG_A) shifted |= SEG_D;

    return shifted;
}

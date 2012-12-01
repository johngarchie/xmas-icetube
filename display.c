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
#include <util/atomic.h>  // for enabling and disabling interrupts


#include "display.h"
#include "usart.h"  // for debugging output


// extern'ed data pertaining the display
volatile display_t display;


// permanent place to store display brightness
uint8_t ee_display_bright_min EEMEM = 1;
uint8_t ee_display_bright_max EEMEM = 1;


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
    // enable Timer/Counter0:
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

    // set initial photo_avg to maximum ambient light
    display.photo_avg = UINT16_MAX;
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

    // configure Timer/Counter0:
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


// called every semisecond; controls the MAX6921/VFD
void display_semitick(void) {
    static uint8_t digit_idx = 0;  
    digit_idx %= DISPLAY_SIZE;

    // select the digit position to display
    uint32_t bits = (uint32_t)1 << pgm_read_byte(vfd_digit_pins + digit_idx);

    // select the segments to display
    for(uint8_t segment = 0; segment < 8; ++segment) {
	if(display.buffer[digit_idx] & _BV(segment)) {
	    bits |= (uint32_t)1 << pgm_read_byte(vfd_segment_pins + segment);
	}
    }

    // send bits to the MAX6921 (vfd driver chip)
    ATOMIC_BLOCK(ATOMIC_FORCEON) {  // disable interrupts
	// Note that one system clock cycle is 1 / 8 MHz seconds or 125 ns.
	// According to the MAX6921 datasheet, the minimum pulse-width on the
	// MAX6921 CLK and LOAD pins need only be 90 ns and 55 ns,
	// respectively.  The minimum period for CLK must be at least 200 ns.
	// Therefore, no delays should be necessary in the code below.

	// Also, the bits could be sent by SPI (they are in the origional
	// Adafruit firmware), but I have found that doing so sometimes
	// results in display flicker.

	for(uint8_t i = 0; i < 20; ++i, bits <<= 1) {
	    if(bits & 0x00080000) {  // if 20th bit set
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

	// pulse MAX6921 LOAD pin:  transfers shift
	// register to latch when high; latches when low
	PORTC |=  _BV(PC0);
	PORTC &= ~_BV(PC0);
    }

    ++digit_idx;


    // get ambient lighting from photosensor every 50 semiseconds,
    // and update running average of photosensor values; note that
    // the running average has a range of [0, 0xFFFF]
    static uint8_t photo_timer = 50;

    if(! --photo_timer) {
	// update adc running average (display.photo_avg)
	display.photo_avg -= (display.photo_avg >> 6);
	display.photo_avg += ADC;

	// update brightness from display.photo_avg
	display_autodim();

        // begin next analog to digital conversion
        ADCSRA |= _BV(ADSC);

	// repeat in 50 semiseconds
        photo_timer = 50;
    }
}


// clear the given display position
void display_clear(uint8_t idx) {
    display.buffer[idx] = DISPLAY_SPACE;
}


// display the given string from program memory
void display_pstr(PGM_P pstr) {
    uint8_t idx = 0;

    // clear first display position
    display_clear(idx);

    // display the string
    char c = pgm_read_byte(pstr + idx++);
    while(c && idx < DISPLAY_SIZE) {
	display_char(idx, c);
        c = pgm_read_byte(pstr + idx++);
    }

    // clear any remaining positions
    for(; idx < DISPLAY_SIZE; ++idx) {
	display_clear(idx);
    }
}


// load display brightness from eeprom
void display_loadbright(void) {
    display.bright_min = eeprom_read_byte(&ee_display_bright_min);
    display.bright_max = eeprom_read_byte(&ee_display_bright_max);
    display_autodim();
}


// save display brightness to eeprom
void display_savebright(void) {
    eeprom_write_byte(&ee_display_bright_min, display.bright_min);
    eeprom_write_byte(&ee_display_bright_max, display.bright_max);
}


// set display brightness from display.bright_min,
// display.bright_max, and display.photo_avg
void display_autodim(void) {
    // convert photoresistor value to 20-90 for OCR0A
    uint8_t new_OCR0A = 20 + 7 * display.bright_max
			    - (display.photo_avg >> 8) * 7
                               * (display.bright_max - display.bright_min)
			           / 256;

    // ensure display will not be too dim or too bright:
    // if too dim, low voltage may not light digit segments;
    // if too bright, excessive voltage may damage display.
    if(new_OCR0A < 20) new_OCR0A = 20;
    if(new_OCR0A > 90) new_OCR0A = 90;

    // set new brightness
    OCR0A = new_OCR0A;
}


// display digit (n) on display position (idx)
void display_digit(uint8_t idx, uint8_t n) {
    display.buffer[idx] = pgm_read_byte(number_segments + (n % 10));
}


// display character (c) at display position (idx)
void display_char(uint8_t idx, char c) {
    if(c >= 'a' && c <= 'z') {
	display.buffer[idx] = pgm_read_byte(letter_segments + c - 'a');
    } else if(c >= 'A' && c <= 'Z') {
	display.buffer[idx] = pgm_read_byte(letter_segments + c - 'A');
    } else if(c >= '0' && c <= '9') {
	display.buffer[idx] = pgm_read_byte(number_segments + c - '0');
    } else {
	switch(c) {
	    case ' ':
		display.buffer[idx] = DISPLAY_SPACE;
		break;
	    case '-':
		display.buffer[idx] = DISPLAY_DASH;
		break;
	    default:
		display.buffer[idx] = DISPLAY_WILDCARD;
		break;
	}
    }
}


// displays decimals at positions between idx_start and idx_end, inclusive
void display_dotselect(uint8_t idx_start, uint8_t idx_end) {
    for(uint8_t idx = idx_start; idx <= idx_end && idx < DISPLAY_SIZE; ++idx) {
	display.buffer[idx] |= DISPLAY_DOT;
    }
}


// if show is true, displays dot at specified display position (idx)
// if show is false, clears dot at specified display position (idx)
void display_dot(uint8_t idx, uint8_t show) {
    if(show) {
	display.buffer[idx] |= DISPLAY_DOT;
    } else {
	display.buffer[idx] &= ~DISPLAY_DOT;
    }
}


// if show is true, displays dash at specified display position (idx)
// if show is false, clears dash at specified display position (idx)
void display_dash(uint8_t idx, uint8_t show) {
    if(show) {
	display.buffer[idx] |= DISPLAY_DASH;
    } else {
	display.buffer[idx] &= ~DISPLAY_DASH;
    }
}

#include <avr/io.h>       // for using avr register names
#include <avr/pgmspace.h> // for accessing data in program memory
#include <avr/eeprom.h>   // for accessing data in eeprom memory
#include <avr/power.h>    // for enabling/disabling chip features
#include <util/atomic.h>  // for enabling and disabling interrupts

#include "display.h"


// display brightness
uint8_t ee_brightness EEMEM = 0;


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

volatile display_t display;

void display_init(void) {
    // disable boost and vfd
    DDRD  |= _BV(PD6) | _BV(PD3); // enable boost fet and vfd transistor
    PORTD &= ~_BV(PD6); // boost fet off (pull low)
    PORTD |=  _BV(PD3); // MAX6921 power off (push high)

    // configure MAX6921 load and blank pins
    DDRC  |=  _BV(PC0) |  _BV(PC3); // set to output
    PORTC &= ~_BV(PC0) & ~_BV(PC3); // clamp to ground

    // set OCR0A (brightness) from eeprom memory
    display_loadbright();

    // for some reason, on my atmega168v, sleep fails unless spi has been
    // enabled and configured at least once (?!?!?)

    // enable spi module
    power_spi_enable();

    // set spi clk and mosi pins to output
    DDRB |= _BV(PB5) | _BV(PB3);

    // configure spi
    SPCR = _BV(SPE) | _BV(MSTR) | _BV(SPR0);

    // disable spi module
    power_spi_disable();
}

void display_sleep(void) {
    power_timer0_disable(); // disable counter0

    PORTD &= ~_BV(PD6); // boost fet off (pull low)
    PORTD |=  _BV(PD3); // MAX6921 power off (pull high)

    // disable display-control output pins
    DDRB &= ~_BV(PB5) & ~_BV(PB3); // spi clk and mosi outputs
    DDRC &= ~_BV(PC0) & ~_BV(PC3); // MAX6921 load and blank outputs

    power_spi_disable();  // disable spi
}


void display_wake(void) {
    // enable spi
    power_spi_enable();

    // set spi and MAX6921 control pins to output
    DDRB |= _BV(PB5) | _BV(PB3); // spi clk and mosi pins
    DDRC |= _BV(PC0) | _BV(PC3); // MAX6921 load and blank

    // setup spi
    SPCR = _BV(SPE) | _BV(MSTR) | _BV(SPR0);

    // enable and configure Timer/Counter0:
    power_timer0_enable();
    // COM0A1:0 = 10: clear OC0A on compare match; set at BOTTOM
    // WGM02:0 = 011: clear timer on compare match; TOP = OCR0A
    TCCR0A = _BV(COM0A1) | _BV(WGM00) | _BV(WGM01);
    TCCR0B = _BV(CS00);   // clock counter0 with system clock
    TIMSK0 = _BV(TOIE0);  // enable counter0 overflow interrupt

    // set OCR0A according to display brightness
    display_setbright(display.brightness);

    PORTD &= ~_BV(PD3); // MAX6921 power on (pull low)

}

void display_semitick(void) {
    static uint8_t digit_idx = 0;  
    digit_idx %= DISPLAY_SIZE;

    // select the digit to display
    uint32_t bits = (uint32_t)1 << pgm_read_byte(vfd_digit_pins + digit_idx);

    // select the segments to display
    for(uint8_t segment = 0; segment < 8; ++segment) {
	if(display.buffer[digit_idx] & _BV(segment)) {
	    bits |= (uint32_t)1 << pgm_read_byte(vfd_segment_pins + segment);
	}
    }

    // send the bits via SPI
    ATOMIC_BLOCK(ATOMIC_FORCEON) {  // disable interrupts
	for(int8_t shift = 16; shift >= 0; shift -= 8) {
	    SPDR = bits >> shift;
	    while(!(SPSR & _BV(SPIF)));
	}

	// pulse MAX6921 load pin
	PORTC |=  _BV(PC0);
	PORTC &= ~_BV(PC0);
    }

    ++digit_idx;
}


void display_clear(uint8_t idx) {
    display.buffer[idx] = DISPLAY_SPACE;
}

void display_string(const char str[]) {
    uint8_t idx = 0;

    // clear first display position
    display_clear(idx++);

    // display the string
    for(char c = str[0]; c && idx < DISPLAY_SIZE; c = str[idx++]) {
	display_char(idx, c);
    }

    // clear any remaining positions
    for(; idx < DISPLAY_SIZE; ++idx) {
	display_clear(idx);
    }
}

void display_setbright(uint8_t level) {
    // convert range 0-10 to 30-60 for OCR0A
    uint8_t new_OCR0A = 30 + 6 * level;

    // ensure display will not be too dim or too bright:
    // if too dim, low voltage may not light digit segments;
    // if too bright, excessive voltage may damage display.
    if(new_OCR0A < 30) new_OCR0A = 30;
    if(new_OCR0A > 90) new_OCR0A = 90;

    // set and save new brightness
    OCR0A = new_OCR0A;
}


void display_savebright(void) {
    eeprom_write_byte(&ee_brightness, display.brightness);
}


void display_loadbright(void) {
    display.brightness = eeprom_read_byte(&ee_brightness);
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

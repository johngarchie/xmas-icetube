// display.c  --  low-level control of VFD display
// (Display contents are controlled in mode.c)
//
//    PB5 (SCK)                      MAX6921 CLK pin
//    PB3 (MOSI)                     MAX6921 DIN pin
//    PC5*                           photoresistor pull-up
//    PC4 (ADC4)                     photoresistor voltage
//    PC3**                          MAX6921 BLANK pin
//    PC3***			     alternating current signal pin
//    PC2***			     alternating current signal pin
//    PC0                            MAX6921 LOAD pin
//    PD6                            boost transistor
//    PD3*                           vfd power transistor
//    counter/timer0                 boost (PD6) and semiticks
//    analog to digital converter    photoresistor (ADC4) voltage
//
//   * PC5 is used to directly power the photoresistor, vfd power, and
//     temperature sensor pull-up if configured for the xmas-icetube
//     hardware design.
//
//  ** PD5--not PC3--is used to control the BLANK pin if and only if
//     the IV-18 to-spec hack is enabled.
//
// *** PC3 and PC2 are used to generate an alternating current signal
//     to power the IV-18 filament if and only if the IV-18 to-spec 
//     hack is enabled.
//


#include <avr/io.h>       // for using avr register names
#include <avr/pgmspace.h> // for accessing data in program memory
#include <avr/eeprom.h>   // for accessing data in eeprom memory
#include <avr/power.h>    // for enabling/disabling chip features
#include <util/atomic.h>  // for using atomic blocks
#include <util/delay.h>   // for enabling delays


#include "display.h"
#include "usart.h"    // for debugging output
#include "system.h"   // for determining system status
#include "time.h"     // for determing current time


// extern'ed data pertaining the display
volatile display_t display;


// permanent place to store display brightness
uint8_t ee_display_status EEMEM =   DISPLAY_ANIMATED | DISPLAY_ALTNINE 
			          | DISPLAY_ALTALPHA;
uint8_t ee_display_colon_style_idx EEMEM = 0;
#ifdef AUTOMATIC_DIMMER
uint8_t ee_display_bright_min EEMEM = 0;
uint8_t ee_display_bright_max EEMEM = 8;
uint8_t ee_display_off_threshold EEMEM = 0;
#else
uint8_t ee_display_brightness EEMEM = 1;
#endif  // AUTOMATIC_DIMMER
#ifndef SEGMENT_MULTIPLEXING
uint8_t ee_display_digit_times[] EEMEM = { [0 ... DISPLAY_SIZE - 1] = 15 };
#endif  // ~SEGMENT_MULTIPLEXING

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
const uint8_t letter_segments_ada[] PROGMEM = {
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


// alternative codes for vfd letter display
const uint8_t letter_segments_alt[] PROGMEM = {
    SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G, // A
    SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,         // b
    SEG_A | SEG_D | SEG_E | SEG_F,                 // C
    SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,         // d
    SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,         // E
    SEG_A | SEG_E | SEG_F | SEG_G,                 // F
    SEG_A | SEG_C | SEG_D | SEG_E | SEG_F,         // G
    SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,         // H
    SEG_B | SEG_C,                                 // I
    SEG_B | SEG_C | SEG_D | SEG_E,                 // J
    SEG_A | SEG_C | SEG_E | SEG_F | SEG_G,         // K
    SEG_D | SEG_E | SEG_F,                         // L
    SEG_A | SEG_C | SEG_E | SEG_G,                 // M
    SEG_C | SEG_E | SEG_G,                         // n
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F, // O
    SEG_A | SEG_B | SEG_E | SEG_F | SEG_G,         // P
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_G,         // q
    SEG_E | SEG_G,                                 // r
    SEG_A | SEG_C | SEG_D | SEG_F | SEG_G,         // S
    SEG_D | SEG_E | SEG_F | SEG_G,                 // t
    SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,         // U
    SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,         // V
    SEG_A | SEG_C | SEG_D | SEG_E,                 // W
    SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,         // X
    SEG_B | SEG_C | SEG_D | SEG_F | SEG_G,         // y
    SEG_A | SEG_B | SEG_D | SEG_E | SEG_G,         // Z
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

// the following macro encodes the various colon frames
// delay is time to display the segment (in ~70 ms units)
// prevdec is true if the decimal of the previous digit should be lit
// segs is the colon character to display
#define COLON_FRAME(delay, prevdec, segs) ((delay << 9) | (prevdec ? 0x0100 : 0x0000) | segs)

// COLON_END is the terminator for a set of colon frames
#define COLON_FRAME_END COLON_FRAME(0, 0, 0)

// the following macros extract the various fields of a colon frame
#define COLON_DELAY(frame) ((frame >> 3) & 0xFE00)
#define COLON_PREVDEC(frame) (frame & 0x0100)
#define COLON_SEGS(frame) (frame & 0x00FF)

// codes for rolling colon time separators
const uint16_t colon_space[] PROGMEM = {
    COLON_FRAME(8, FALSE, 0),
    COLON_FRAME_END,
};

// codes for rolling colon time separators
const uint16_t colon_figure_eight[] PROGMEM = {
    COLON_FRAME(8, FALSE, SEG_A),
    COLON_FRAME(8, FALSE, SEG_B),
    COLON_FRAME(8, FALSE, SEG_G),
    COLON_FRAME(8, FALSE, SEG_E),
    COLON_FRAME(8, FALSE, SEG_D),
    COLON_FRAME(8, FALSE, SEG_C),
    COLON_FRAME(8, FALSE, SEG_G),
    COLON_FRAME(8, FALSE, SEG_F),
    COLON_FRAME_END,
};

// codes for rolling colon time separators
const uint16_t colon_lower_circle_roll[] PROGMEM = {
    COLON_FRAME(8, FALSE, SEG_G),
    COLON_FRAME(8, FALSE, SEG_C),
    COLON_FRAME(8, FALSE, SEG_D),
    COLON_FRAME(8, FALSE, SEG_E),
    COLON_FRAME_END,
};

// codes for rolling colon time separators
const uint16_t colon_2segment_circle_roll[] PROGMEM = {
    COLON_FRAME(8, FALSE, SEG_A | SEG_D),
    COLON_FRAME(8, FALSE, SEG_B | SEG_E),
    COLON_FRAME(8, FALSE, SEG_C | SEG_F),
    COLON_FRAME_END,
};

// codes for rolling colon time separators
const uint16_t colon_vertical_bounce[] PROGMEM = {
    COLON_FRAME(8, FALSE, SEG_A),
    COLON_FRAME(8, FALSE, SEG_G),
    COLON_FRAME(8, FALSE, SEG_D),
    COLON_FRAME(8, FALSE, SEG_G),
    COLON_FRAME_END,
};

// program pointers to rolling colon styles
#define COLON_SEQUENCES_SIZE 5
const PROGMEM uint16_t* const colon_styles[] PROGMEM = {
    colon_space,
    colon_figure_eight,
    colon_lower_circle_roll,
    colon_2segment_circle_roll,
    colon_vertical_bounce,
};


#ifdef VFD_TO_SPEC
#ifndef OCR0B_PWM_DISABLE
// magic values for converting brightness to OCR2B values
#define OCR0B_GRADIENT_MAX 80
#ifdef OCR0A_VALUE
// floor(exp(2.0201 + 0.3437 * 2:82/8))
const uint8_t ocr0b_gradient[] PROGMEM =
  { 8, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 16, 17, 17,
    18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 31, 32, 33, 35, 36, 38,
    40, 42, 43, 45, 47, 49, 52, 54, 56, 59, 61, 64, 67, 70, 73, 76, 80, 83,
    87, 91, 95, 99, 103, 108, 112, 117, 123, 128, 134, 139, 146, 152, 159,
    166, 173, 181, 189, 197, 206, 215, 224, 234, 244, 255 };
#else  // ~OCR0A_VALUE
// floor(exp(2.0201 + 0.3437 * seq(2,82/8, length.out=81)))
const uint8_t ocr0b_gradient[] PROGMEM =
  { 14, 15, 16, 16, 17, 17, 18, 19, 19, 20, 21, 22, 22, 23, 24, 25, 26, 27,
    28, 29, 30, 31, 32, 33, 35, 36, 37, 39, 40, 41, 43, 44, 46, 48, 50, 51,
    53, 55, 57, 59, 61, 64, 66, 68, 71, 73, 76, 79, 82, 85, 88, 91, 94, 98,
    101, 105, 109, 113, 117, 121, 125, 130, 134, 139, 144, 150, 155, 161,
    166, 172, 179, 185, 192, 199, 206, 213, 221, 229, 237, 246, 255 };
#endif  // OCR0A_VALUE
#endif  // ~OCR0B_PWM_DISABLE
#endif  // VFD_TO_SPEC


// initialize display after system reset
void display_init(void) {
    // if any timer is disabled during sleep, the system locks up sporadically
    // and nondeterministically, so enable timer0 in PRR and leave it alone!
    power_timer0_enable();

    // disable boost and vfd
#ifndef XMAS_DESIGN
    DDRD  |= _BV(PD3); // enable boost fet and vfd transistor
    PORTD |= _BV(PD3); // MAX6921 power off (push high)
#endif  // !XMAS_DESIGN
    DDRD  |=  _BV(PD6); // enable boost fet and vfd transistor
    PORTD &= ~_BV(PD6); // boost fet off (pull low)

#ifdef VFD_TO_SPEC
    // configure MAX6921 load and blank pins
    DDRC  |=  _BV(PC0); // set blank pin to output
    PORTC &= ~_BV(PC0); // clamp blank pin to ground
    DDRD  |=  _BV(PD5); // set load pin to output
    PORTD &= ~_BV(PD5); // clamp load pin to ground
#else
    // configure MAX6921 load and blank pins
    DDRC  |=  _BV(PC0) |  _BV(PC3); // set to output
    PORTC &= ~_BV(PC0) & ~_BV(PC3); // clamp to ground
#endif  // VFD_TO_SPEC

    // configure spi sck and mosi pins as floating inputs
    // (these pins seem to use less power when configured
    // as inputs *without* pull-ups!?!?)
    DDRB  &= ~_BV(PB5) & ~_BV(PB3);  // set to input

    // configure photoresistor pull-up pin
    DDRC  |=  _BV(PC5);  // set pin as output
    PORTC &= ~_BV(PC5);  // pull to ground

    // disable digital circuitry on photoresistor pins
    DIDR0 |= _BV(ADC5D) | _BV(ADC4D);

    display.multiplex_div = 1;  // multiplexing divider

#ifdef VFD_TO_SPEC
    display.filament_div   = 1;  // ac-frquency divider
    display.filament_timer = 1;  // ac-generation timer
#endif  // VFD_TO_SPEC

#ifdef AUTOMATIC_DIMMER
    // set initial photo_avg to maximum ambient light
    display.photo_avg = UINT16_MAX;

    // load the display-off threshold
    display_loadphotooff();
#endif  // AUTOMATIC_DIMMER

    // load the digit display times
#ifndef SEGMENT_MULTIPLEXING
    display_loaddigittimes();
#endif  // ~SEGMENT_MULTIPLEXING

    // load the auto off settings
    display_loadofftime();
    display_loadoffdays();
    display_loadondays();

    // set the initial transition type to none
    display.trans_type = DISPLAY_TRANS_NONE;

    // load display status
    display_loadstatus();

#ifdef VFD_TO_SPEC
    // configure AC signal pins
    DDRC  |=  _BV(PC2) |  _BV(PC3);  // set as output
    PORTC &= ~_BV(PC2) & ~_BV(PC3);  // pull to ground
#endif  // VFD_TO_SPEC

    display_loadcolonstyle();
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


#ifdef VFD_TO_SPEC
#ifdef OCR0B_PWM_DISABLE
    // configure and start Timer/Counter0
    // COM0A1:0 = 10: clear OC0A on compare match; set at BOTTOM
    // WGM02:0 = 011: clear timer on compare match; TOP = 0xFF
    TCCR0A = _BV(COM0A1) | _BV(WGM00) | _BV(WGM01);
    TCCR0B = _BV(CS00);   // clock counter0 with system clock
    TIMSK0 = _BV(TOIE0);  // enable counter0 overflow interrupt
#else  // ~OCR0B_PWM_DISABLE
    // configure and start Timer/Counter0
    // COM0A1:0 = 10: clear OC0A on compare match; set at BOTTOM
    // COM0B1:0 = 11: clear OC0B at bottom; set on compare match
    // WGM02:0 = 011: clear timer on compare match; TOP = 0xFF
    TCCR0A = _BV(COM0A1) | _BV(COM0B0) | _BV(COM0B1) | _BV(WGM00) | _BV(WGM01);
    TCCR0B = _BV(CS00);   // clock counter0 with system clock
    TIMSK0 = _BV(TOIE0);  // enable counter0 overflow interrupt
#endif // OCR0B_PWM_DISABLE
#else  // ~VFD_TO_SPEC
    // configure and start Timer/Counter0
    // COM0A1:0 = 10: clear OC0A on compare match; set at BOTTOM
    // WGM02:0 = 011: clear timer on compare match; TOP = 0xFF
    TCCR0A = _BV(COM0A1) | _BV(WGM00) | _BV(WGM01);
    TCCR0B = _BV(CS00);   // clock counter0 with system clock
    TIMSK0 = _BV(TOIE0);  // enable counter0 overflow interrupt
#endif  // VFD_TO_SPEC

    // set OCR0A for desired display brightness
    display_loadbright();

    // MAX6921 power on (pull low)
#ifndef XMAS_DESIGN
    PORTD &= ~_BV(PD3);
#endif  // !XMAS_DESIGN

    display.off_timer = DISPLAY_OFF_TIMEOUT;
    display_on();
}


// disable display for low-power mode
void display_sleep(void) {
    // disable Timer/Counter0
    TCCR0A = TCCR0B = 0;

    PORTD &= ~_BV(PD6); // boost fet off (pull low)
#ifndef XMAS_DESIGN
    PORTD |=  _BV(PD3); // MAX6921 power off (pull high)
#endif  // !XMAS_DESIGN

    // disable external and internal photoresistor pull-ups
    PORTC &= ~_BV(PC5) & ~_BV(PC4);  // pull to ground, disable pull-up

    // disable analog to digital converter
    ADCSRA = 0;  // disable ADC before power_adc_disable()
    power_adc_disable();

#ifdef VFD_TO_SPEC
    // configure MAX6921 LOAD and BLANK pins
    PORTC &= ~_BV(PC0); // clamp to ground
    PORTD &= ~_BV(PD5); // clamp to ground

    // disable VFD cathode (heater fillament)
    PORTC &= ~_BV(PC2) & ~_BV(PC3);  // pull to ground
#else
    // configure MAX6921 LOAD and BLANK pins
    PORTC &= ~_BV(PC0) & ~_BV(PC3); // clamp to ground
#endif  // VFD_TO_SPEC

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
    if((display.photo_avg >> 7) + display.off_threshold > 0x0200) {
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
    if(!(display.on_days & dowflag) && !(display.off_hour & DISPLAY_NOOFF)) {
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
    if((display.photo_avg >> 7) + display.off_threshold < 0x0200
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
#ifndef XMAS_DESIGN
	    PORTD |=  _BV(PD3);  // MAX6921 power off (pull high)
#endif  // !XMAS_DESIGN
#ifdef VFD_TO_SPEC
	    // disable VFD cathode (heater fillament)
	    PORTC &= ~_BV(PC2) & ~_BV(PC3);  // pull to ground
#endif  // VFD_TO_SPEC
	}
    }
}


// enable display
void display_on(void) {
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
	if(!(system.status & SYSTEM_SLEEP)
		&& (display.status & DISPLAY_DISABLED)) {
	    display.status &= ~DISPLAY_DISABLED;
#ifdef VFD_TO_SPEC
	    // enable boost and blank pwm
#ifdef OCR0B_PWM_DISABLE
	    TCCR0A =   _BV(COM0A1) | _BV(WGM00) | _BV(WGM01);
#else  // ~OCR0B_PWM_DISABLE
	    TCCR0A =   _BV(COM0A1) | _BV(COM0B0) | _BV(COM0B1)
		     | _BV(WGM00)  | _BV(WGM01);
#endif  // OCR0B_PWM_DISABLE

	    // power VFD cathode (heater fillament)
	    PORTC |=  _BV(PC2);
	    PORTC &= ~_BV(PC3);
#else
	    TCCR0A = _BV(COM0A1) | _BV(WGM00) | _BV(WGM01);
#endif  // VFD_TO_SPEC
#ifndef XMAS_DESIGN
	    PORTD &= ~_BV(PD3);  // MAX6921 power on (pull low)
#endif  // !XMAS_DESIGN
	}
    }
}


#ifndef SEGMENT_MULTIPLEXING
// utility function for display_varsemitick();
// combines two characters for the scroll-left transition
inline uint8_t display_combineLR(uint8_t a, uint8_t b) {
    uint8_t c = 0;

    if(a & SEG_B) c |= SEG_F;
    if(a & SEG_E) c |= SEG_E;
    if(b & SEG_F) c |= SEG_B;
    if(b & SEG_E) c |= SEG_C;

    return c;
}


// utility function for display_varsemitick();
// shifts the given digit up by one
inline uint8_t display_shiftU1(uint8_t digit) {
    uint8_t shifted = 0;

    if(digit & SEG_G) shifted |= SEG_A;
    if(digit & SEG_E) shifted |= SEG_F;
    if(digit & SEG_C) shifted |= SEG_B;
    if(digit & SEG_D) shifted |= SEG_G;

    return shifted;
}


// utility function for display_varsemitick();
// shifts the given digit up by two
inline uint8_t display_shiftU2(uint8_t digit) {
    uint8_t shifted = 0;

    if(digit & SEG_D) shifted |= SEG_A;

    return shifted;
}


// utility function for display_varsemitick();
// shifts the given digit down by one
inline uint8_t display_shiftD1(uint8_t digit) {
    uint8_t shifted = 0;

    if(digit & SEG_A) shifted |= SEG_G;
    if(digit & SEG_F) shifted |= SEG_E;
    if(digit & SEG_B) shifted |= SEG_C;
    if(digit & SEG_G) shifted |= SEG_D;

    return shifted;
}


// utility function for display_varsemitick();
// shifts the given digit down by two
inline uint8_t display_shiftD2(uint8_t digit) {
    uint8_t shifted = 0;

    if(digit & SEG_A) shifted |= SEG_D;

    return shifted;
}


// called periodically to to control the VFD via the MAX6921
// returns time (in 32us units) to display current digit
uint8_t display_varsemitick(void) {
    static uint8_t digit_idx = DISPLAY_SIZE - 1;

#ifdef SUBDIGIT_MULTIPLEXING
    static uint8_t digit_side = 1;

    if(digit_side) {
	digit_side = 0;
	if(++digit_idx >= DISPLAY_SIZE) digit_idx  = 0;
    } else {
	digit_side = 1;
    }
#else
    if(++digit_idx >= DISPLAY_SIZE) digit_idx = 0;
#endif  // SUBDIGIT_MULTIPLEXING

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
#ifdef SUBDIGIT_MULTIPLEXING
	    switch(_BV(segment)) {
		case SEG_B:
		case SEG_C:
		case SEG_H:
		    if(digit_side && (digit & _BV(segment))) {
			bitidx = pgm_read_byte(&(vfd_segment_pins[segment]));
			bits[bitidx >> 3] |= _BV(bitidx & 0x7);
		    }
		    break;
		default:
		    if(!digit_side && (digit & _BV(segment))) {
			bitidx = pgm_read_byte(&(vfd_segment_pins[segment]));
			bits[bitidx >> 3] |= _BV(bitidx & 0x7);
		    }
		    break;
	    }
#else
	    if(digit & _BV(segment)) {
		bitidx = pgm_read_byte(&(vfd_segment_pins[segment]));
		bits[bitidx >> 3] |= _BV(bitidx & 0x7);
	    }
#endif  // SUBDIGIT_MULTIPLEXING
	}

	// blank display to prevent ghosting
#ifdef VFD_TO_SPEC
	// disable pwm on blank pin
#ifndef OCR0B_PWM_DISABLE
	TCCR0A = _BV(COM0A1) | _BV(WGM00) | _BV(WGM01);
#endif  // ~OCR0B_PWM_DISABLE
	PORTD |= _BV(PD5);  // push MAX6921 BLANK pin high
#else
	PORTC |= _BV(PC3);  // push MAX6921 BLANK pin high
#endif  // VFD_TO_SPEC
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

    if(!(display.status & DISPLAY_DISABLED)) {
	// unblank display to prevent ghosting
#ifdef VFD_TO_SPEC
	// enable pwm on blank pin
#ifdef OCR0B_PWM_DISABLE
	PORTD &= ~_BV(PD5);  // pull MAX6921 BLANK pin low
#else  // ~OCR0B_PWM_DISABLE
	OCR0B  = display.OCR0B_value;
	TCCR0A = _BV(COM0A1) | _BV(COM0B0) | _BV(COM0B1) |
	         _BV(WGM00)  | _BV(WGM01);
	TCNT0  = 0xFF;  // set counter to max
#endif  // OCR0B_PWM_DISABLE
#else
	PORTC &= ~_BV(PC3);  // pull MAX6921 BLANK pin low
#endif  // VFD_TO_SPEC
    }

    // return time to display current digit
    return (display.digit_times[digit_idx] >> display.digit_time_shift) + 1;
}
#endif  // ~SEGMENT_MULTIPLEXING


#ifdef SEGMENT_MULTIPLEXING
// utility function for display_varsemitick();
// shifts digits up by one
inline void display_shiftU1(uint8_t bits[], volatile uint8_t buf[],
	                    uint8_t segment) {
    switch(segment) {
      case SEG_A:
	  segment = SEG_G;
	  break;
      case SEG_F:
	  segment = SEG_E;
	  break;
      case SEG_B:
	  segment = SEG_C;
	  break;
      case SEG_G:
	  segment = SEG_D;
	  break;
      default:
	  return;
    }

    for(uint8_t digit_idx = 1; digit_idx < DISPLAY_SIZE; ++digit_idx) {
	if(buf[digit_idx] & segment) {
	    // select the digit position to display
	    uint8_t bitidx = pgm_read_byte(&(vfd_digit_pins[digit_idx]));
	    bits[bitidx >> 3] |= _BV(bitidx & 0x7);
	}
    }
}


// utility function for display_varsemitick();
// shifts digits up by two
inline void display_shiftU2(uint8_t bits[], volatile uint8_t buf[],
	                    uint8_t segment) {
    if(segment == SEG_A) {
	for(uint8_t digit_idx = 1; digit_idx < DISPLAY_SIZE; ++digit_idx) {
	    if(buf[digit_idx] & SEG_D) {
		uint8_t bitidx = pgm_read_byte(&(vfd_digit_pins[digit_idx]));
		bits[bitidx >> 3] |= _BV(bitidx & 0x7);
	    }
	}
    }
}


// utility function for display_varsemitick();
// shifts digits down by one
inline void display_shiftD1(uint8_t bits[], volatile uint8_t buf[],
	                    uint8_t segment) {
    switch(segment) {
      case SEG_G:
	  segment = SEG_A;
	  break;
      case SEG_E:
	  segment = SEG_F;
	  break;
      case SEG_C:
	  segment = SEG_B;
	  break;
      case SEG_D:
	  segment = SEG_G;
	  break;
      default:
	  return;
    }

    for(uint8_t digit_idx = 1; digit_idx < DISPLAY_SIZE; ++digit_idx) {
	if(buf[digit_idx] & segment) {
	    uint8_t bitidx = pgm_read_byte(&(vfd_digit_pins[digit_idx]));
	    bits[bitidx >> 3] |= _BV(bitidx & 0x7);
	}
    }
}


// utility function for display_varsemitick();
// shifts digits down by two
inline void display_shiftD2(uint8_t bits[], volatile uint8_t buf[],
	                    uint8_t segment) {
    if(segment == SEG_D) {
	for(uint8_t digit_idx = 1; digit_idx < DISPLAY_SIZE; ++digit_idx) {
	    if(buf[digit_idx] & SEG_A) {
		uint8_t bitidx = pgm_read_byte(&(vfd_digit_pins[digit_idx]));
		bits[bitidx >> 3] |= _BV(bitidx & 0x7);
	    }
	}
    }
}


// utility function for display_varsemitick();
// combines two characters for the scroll-left transition
inline void display_shiftL(uint8_t bits[], uint8_t segment) {
    uint8_t digit_idx = 0;
    uint8_t trans_idx = DISPLAY_SIZE - (display.trans_timer >> 1);

    if(display.trans_timer & 0x01) {
	switch(segment) {
	    case SEG_B:
		segment = SEG_F;
		break;
	    case SEG_C:
		segment = SEG_E;
		break;
	    case SEG_E:
		--trans_idx;
		segment = SEG_C;
		break;
	    case SEG_F:
		--trans_idx;
		segment = SEG_B;
		break;
	    default:
		return;
	}
    }

    while(++digit_idx < DISPLAY_SIZE) {
	++trans_idx;

	uint8_t digit;

	if(trans_idx < DISPLAY_SIZE) {
	    if(trans_idx == 0) continue;
	    digit = display.postbuf[trans_idx];
	} else {
	    if(trans_idx == DISPLAY_SIZE) continue;
	    digit = display.prebuf[trans_idx - DISPLAY_SIZE];
	}

	if(digit & segment) {
	    uint8_t bitidx = pgm_read_byte(&(vfd_digit_pins[digit_idx]));
	    bits[bitidx >> 3] |= _BV(bitidx & 0x7);
	}
    }
}


// utility function for display_varsemitick();
// sets bit for given segment
void display_noshift(uint8_t bits[], volatile uint8_t buf[],
		     uint8_t segment) {
    for(uint8_t digit_idx = 0; digit_idx < DISPLAY_SIZE; ++digit_idx) {
	if(buf[digit_idx] & segment) {
	    uint8_t bitidx=pgm_read_byte(&(vfd_digit_pins[digit_idx]));
	    bits[bitidx >> 3] |= _BV(bitidx & 0x7);
	}
    }
}


// called periodically to to control the VFD via the MAX6921
// returns time (in 32us units) to display current digit
uint8_t display_varsemitick(void) {
    static uint8_t segment_idx = SEGMENT_COUNT - 1;

    if(++segment_idx >= SEGMENT_COUNT) segment_idx = 0;

    uint8_t segment = _BV(segment_idx);

    // bits to send MAX6921 (vfd driver chip)
    uint8_t bits[3] = {0, 0, 0};

    // select the segment to be displayed
    uint8_t bitidx = pgm_read_byte(&(vfd_segment_pins[segment_idx]));
    bits[bitidx >> 3] |= _BV(bitidx & 0x7);

    switch(display.trans_type) {
	case DISPLAY_TRANS_UP:
	    switch(display.trans_timer) {
		case 5:
		    display_noshift(bits, display.postbuf, segment);
		    break;

		case 4:
		    display_shiftU1(bits, display.postbuf, segment);
		    break;

		case 3:
		    display_shiftU2(bits, display.postbuf, segment);
		    break;

		case 2:
		    display_shiftD2(bits, display.prebuf, segment);
		    break;

		case 1:
		    display_shiftD1(bits, display.prebuf, segment);
		    break;

		default:
		    display_noshift(bits, display.prebuf, segment);
		    break;
	    }
	    break;

	case DISPLAY_TRANS_DOWN:
	    switch(display.trans_timer) {
		case 5:
		    display_noshift(bits, display.postbuf, segment);
		    break;

		case 4:
		    display_shiftD1(bits, display.postbuf, segment);
		    break;

		case 3:
		    display_shiftD2(bits, display.postbuf, segment);
		    break;

		case 2:
		    display_shiftU2(bits, display.prebuf, segment);
		    break;

		case 1:
		    display_shiftU1(bits, display.prebuf, segment);
		    break;

		default:
		    display_noshift(bits, display.prebuf, segment);
		    break;
	    }
	    break;

	case DISPLAY_TRANS_LEFT:
	    if(display.trans_timer >= 2 * DISPLAY_SIZE) {
		display_noshift(bits, display.postbuf, segment);
	    } else if(display.trans_timer) {
		display_shiftL(bits, segment);
	    } else if(display.trans_timer < 2 * DISPLAY_SIZE) {
		display_noshift(bits, display.prebuf, segment);
	    }
	    break;

	default:
	    display_noshift(bits, display.postbuf, segment);
	    break;
    }


    // create the sequence of bits for the calculated digit
    if(!(display.status & DISPLAY_DISABLED)) {
	// blank display to prevent ghosting
#ifdef VFD_TO_SPEC
	// disable pwm on blank pin
#ifndef OCR0B_PWM_DISABLE
	TCCR0A = _BV(COM0A1) | _BV(WGM00) | _BV(WGM01);
#endif  // ~OCR0B_PWM_DISABLE
	PORTD |= _BV(PD5);  // push MAX6921 BLANK pin high
#else
	PORTC |= _BV(PC3);  // push MAX6921 BLANK pin high
#endif  // VFD_TO_SPEC
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

    if(!(display.status & DISPLAY_DISABLED)) {
	// unblank display to prevent ghosting
#ifdef VFD_TO_SPEC
#ifdef OCR0B_PWM_DISABLE
	PORTD &= ~_BV(PD5);  // pull MAX6921 BLANK pin low
#else  // ~OCR0B_PWM_DISABLE
	// enable pwm on blank pin
	OCR0B = display.OCR0B_value;
	TCCR0A = _BV(COM0A1) | _BV(COM0B0) | _BV(COM0B1) |
	         _BV(WGM00)  | _BV(WGM01);
	TCNT0  = 0xFF;  // set counter to max
#endif  // OCR0B_PWM_DISABLE
#else
	PORTC &= ~_BV(PC3);  // pull MAX6921 BLANK pin low
#endif  // VFD_TO_SPEC
    }

    // return time to display current digit
    return 16;
}
#endif  // SEGMENT_MULTIPLEXING


// called every semisecond; updates ambient brightness running average
void display_semitick(void) {
    // Update the display transition variables as time passes:
    // During a transition, display_varsemitick() calculates the segments
    // to display on-the-fly from the transition variables.

    static uint16_t trans_delay_timer = 0;

    // calculate timer values for scrolling display
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
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
		    for(uint8_t i = 0; i < DISPLAY_SIZE; ++i) {
			display.postbuf[i] = display.prebuf[i];
		    }

		    display.dot_postbuf   = display.dot_prebuf;
		    display.colon_postbuf = display.colon_prebuf;

		    display.trans_type = DISPLAY_TRANS_NONE;
		}
	    }
	}
    }


#ifdef AUTOMATIC_DIMMER
    // get ambient lighting from photosensor every 16 semiseconds,
    // and update running average of photosensor values; note that
    // the running average has a range of [0, 0xFFFF]
    static uint8_t photo_timer = DISPLAY_ADC_DELAY;

    if(!--photo_timer) {
	// repeat in 16 semiseconds
        photo_timer = DISPLAY_ADC_DELAY;

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

	    static uint8_t grad_idx = 0;

	    if(display.status & DISPLAY_PULSE_DOWN) {
		if(grad_idx == 0x00) {
		    display.status &= ~DISPLAY_PULSE_DOWN;
		} else {
		    display_setbrightness(--grad_idx);
		}
	    } else {
		if(grad_idx == 80) {
		    display.status |=  DISPLAY_PULSE_DOWN;
		} else {
		    display_setbrightness(++grad_idx);
		}
	    }
	}
    }

    // handle the animated colons
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
	if(display.trans_type == DISPLAY_TRANS_NONE) {
	    if(++display.colon_timer >= COLON_DELAY(display.colon_frame)) {
		display.colon_timer = 0;
		NONATOMIC_BLOCK(NONATOMIC_FORCEOFF) {
		    display_nextcolonframe();
		}
	    }
	}
    }

    // handle blinking dot separators
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
	if(display.trans_type == DISPLAY_TRANS_NONE) {
	    if(    ((time.timeformat_flags & TIME_TIMEFORMAT_DOTFLASH_SLOW)
		      && ++display.dot_timer >= DISPLAY_DOTFLASH_SLOW_TIME)
		|| ((time.timeformat_flags & TIME_TIMEFORMAT_DOTFLASH_FAST)
		      && ++display.dot_timer >= DISPLAY_DOTFLASH_FAST_TIME)) {
		display.status ^= DISPLAY_HIDEDOTS;
		display.dot_timer = 0;
		NONATOMIC_BLOCK(NONATOMIC_FORCEOFF) {
		    display_updatedots();
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


// save selected colon style
void display_savecolonstyle(void) {
    eeprom_write_byte(&ee_display_colon_style_idx, display.colon_style_idx);
}


// load selected colon style
void display_loadcolonstyle(void) {
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
	display.colon_style_idx = eeprom_read_byte(&ee_display_colon_style_idx);
	if(display.colon_style_idx >= COLON_SEQUENCES_SIZE) {
	    display.colon_style_idx = 0;
	}
	display.colon_frame_idx = 0;
	display.colon_timer = 0;
    }

    display_updatecolons();
}


// change to next colon style
void display_nextcolonstyle(void) {
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
	if(++display.colon_style_idx >= COLON_SEQUENCES_SIZE) {
	    display.colon_style_idx = 0;
	}

	display.colon_frame_idx = 0;
	display.colon_timer     = 0;

	display_loadcolonframe();
    }
}


// load colon frame data given display.colon_frame_idx
void display_loadcolonframe(void) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
	// fetch current colon style from program memory
	uint16_t* PROGMEM colon_ptr = (uint16_t* PROGMEM) pgm_read_word(
		&(colon_styles[display.colon_style_idx]));

	// fetch next colon frame from program memory
	display.colon_frame = pgm_read_word(
		&(colon_ptr[display.colon_frame_idx]));

	if(display.colon_frame == COLON_FRAME_END) {
	    display.colon_frame_idx = 0;
	    display.colon_frame = pgm_read_word(
		    &(colon_ptr[display.colon_frame_idx]));
	}
    }

    NONATOMIC_BLOCK(NONATOMIC_RESTORESTATE) {
	display_updatecolons();
    }
}


// move to next colon frame
void display_nextcolonframe(void) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
	++display.colon_frame_idx;
	display_loadcolonframe();
    }
}


// updates display for colon separators
void display_updatecolons(void) {
    for(uint8_t bit=1, idx=8; bit; bit <<= 1, --idx) {
	if(bit & display.colon_prebuf) {
	    display.prebuf[idx] = COLON_SEGS(display.colon_frame);
	    if(COLON_PREVDEC(display.colon_frame)) {
		display.prebuf[idx-1] |= SEG_H;
	    } else {
		display.prebuf[idx-1] &= ~SEG_H;
	    }
	}

	if(bit & display.colon_postbuf) {
	    display.postbuf[idx] = COLON_SEGS(display.colon_frame);
	    if(COLON_PREVDEC(display.colon_frame)) {
		display.postbuf[idx-1] |= SEG_H;
	    } else {
		display.postbuf[idx-1] &= ~SEG_H;
	    }
	}
    }
}


// switch to next dot-separator style
void display_nextdotstyle(void) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
	if(time.timeformat_flags & TIME_TIMEFORMAT_DOTFLASH_SLOW) {
	    time.timeformat_flags &= ~TIME_TIMEFORMAT_DOTFLASH_SLOW;
	    time.timeformat_flags |=  TIME_TIMEFORMAT_DOTFLASH_FAST;
	} else if(time.timeformat_flags & TIME_TIMEFORMAT_DOTFLASH_FAST) {
	    time.timeformat_flags &= ~TIME_TIMEFORMAT_DOTFLASH_SLOW;
	    time.timeformat_flags &= ~TIME_TIMEFORMAT_DOTFLASH_FAST;
	} else {
	    time.timeformat_flags |=  TIME_TIMEFORMAT_DOTFLASH_SLOW;
	    time.timeformat_flags &= ~TIME_TIMEFORMAT_DOTFLASH_FAST;
	}

	display.status &= ~DISPLAY_HIDEDOTS;
	display.dot_timer = 0;
    }
}


// updates display for colon separators
void display_updatedots(void) {
    // apply style to colon positions
    for(uint8_t bit=1, idx=7; bit; bit <<= 1, --idx) {
	if(bit & display.dot_prebuf) {
	    if(display.status & DISPLAY_HIDEDOTS) {
		display.prebuf[idx] &= ~SEG_H;
	    } else {
		display.prebuf[idx] |= SEG_H;
	    }
	}

	if(bit & display.dot_postbuf) {
	    if(display.status & DISPLAY_HIDEDOTS) {
		display.postbuf[idx] &= ~SEG_H;
	    } else {
		display.postbuf[idx] |= SEG_H;
	    }
	}
    }
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

    display.dot_prebuf   = 0;
    display.colon_prebuf = 0;
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


#ifndef SEGMENT_MULTIPLEXING
// loads the times (32 us units) to display each digit
void display_loaddigittimes(void) {
    for(uint8_t i = 0; i < DISPLAY_SIZE; ++i) {
	display.digit_times[i] = eeprom_read_byte(&(ee_display_digit_times[i]));
    }

    display_noflicker();
}


// saves the times (32 us units) to display each digit
void display_savedigittimes(void) {
    for(uint8_t i = 0; i < DISPLAY_SIZE; ++i) {
	eeprom_write_byte(&(ee_display_digit_times[i]), display.digit_times[i]);
    }
}


// calculates new digit time shift to prevent flicker
void display_noflicker(void) {
    uint16_t total_digit_time = 0;

    for(uint8_t i = 0; i < DISPLAY_SIZE; ++i) {
	total_digit_time += display.digit_times[i];
    }

    display.digit_time_shift = 0;

    while( (total_digit_time >> display.digit_time_shift)
	    > DISPLAY_NOFLICKER_TIME ) {
	++display.digit_time_shift;
    }
}
#endif //  ~SEGMENT_MULTIPLEXING


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


// set display brightness using photoresistor or specified level
void display_autodim(void) {
#ifdef AUTOMATIC_DIMMER
    // convert photoresistor value to [0-80] for display_setbrightness()
    int16_t grad_idx = (display.bright_max << 3) - (((display.photo_avg >> 8)
		      * (display.bright_max - display.bright_min)) >> 5);

    // add a 1-index lag to grad_idx to prevent
    // rapid cycling between brightness levels
    if(grad_idx < display.photo_idx) ++grad_idx;
    if(grad_idx > display.photo_idx) --grad_idx;
    display.photo_idx = grad_idx;
#else  // ~AUTOMATIC_DIMMER
    int16_t grad_idx = (display.brightness < 0 ? 0 : display.brightness << 3);
#endif  // AUTOMATIC_DIMMER

    // limit grad_idx to [0, 80]
    if(grad_idx > 80) grad_idx = 80;
    if(grad_idx < 0)  grad_idx = 0;

    display_setbrightness(grad_idx);
}


// set the display brightness where level is in the range [0-80]
void display_setbrightness(int8_t level) {
#ifdef VFD_TO_SPEC
#ifndef OCR0B_PWM_DISABLE
    // force level in appropriate bounds
    if(level < 0 ) level = 0;
    if(level > OCR0B_GRADIENT_MAX) level = OCR0B_GRADIENT_MAX;

    display.OCR0B_value = pgm_read_byte(&(ocr0b_gradient[level]));
#endif  // OCR0B_PWM_DISABLE
#endif  // VFD_TO_SPEC

#ifdef OCR0A_VALUE
    OCR0A = OCR0A_VALUE;  // set fixed boost value
#else  // ~OCR0A_VALUE
    int16_t new_OCR0A = OCR0A_MIN + ((OCR0A_SCALE * level) >> 3);

    // ensure display will not be too dim or too bright:
    // if too dim, low voltage may not light digit segments;
    // if too bright, excessive voltage may damage display.
    if(new_OCR0A < OCR0A_MIN) new_OCR0A = OCR0A_MIN;
    if(new_OCR0A > OCR0A_MAX) new_OCR0A = OCR0A_MAX;

    // set new brightness
    OCR0A = new_OCR0A;
#endif  // OCR0A_VALUE
}


// display digit (n) on display position (idx)
void display_digit(uint8_t idx, uint8_t n) {
    // clear blinking dot, if any
    display.dot_prebuf &= ~_BV(7 - idx);

    // clear colon char, if any
    display.colon_prebuf &= ~_BV(8 - idx);

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
    // clear blinking dot, if any
    display.dot_prebuf &= ~_BV(7 - idx);

    // process colon
    if(c == ':') {
	display.colon_prebuf |= _BV(8 - idx);
	display.prebuf[idx] = COLON_SEGS(display.colon_frame);
	return;
    } else {
	display.colon_prebuf &= ~_BV(8 - idx);
    }

    if('a' <= c && c <= 'z') {
	if(display.status & DISPLAY_ALTALPHA) {
	    display.prebuf[idx] = pgm_read_byte(&(letter_segments_alt[c-'a']));
	} else {
	    display.prebuf[idx] = pgm_read_byte(&(letter_segments_ada[c-'a']));
	}
    } else if('A' <= c && c <= 'Z') {
	if(display.status & DISPLAY_ALTALPHA) {
	    display.prebuf[idx] = pgm_read_byte(&(letter_segments_alt[c-'A']));
	} else {
	    display.prebuf[idx] = pgm_read_byte(&(letter_segments_ada[c-'A']));
	}
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


// shows or hides dot at given index
// if show is true, displays dot at specified display position (idx)
// if show is false, clears dot at specified display position (idx)
void display_dot(uint8_t idx, uint8_t show) {
    if(show) {
	display.prebuf[idx] |= DISPLAY_DOT;
    } else {
	display.prebuf[idx] &= ~DISPLAY_DOT;
    }
}


// shows or hides dot separator at given index (a dot separator can flash)
// if show is true, displays dot separator at specified display position (idx)
// if show is false, clears dot separator at specified display position (idx)
void display_dotsep(uint8_t idx, uint8_t show) {
    if(show) {
	display.dot_prebuf |= _BV(7 - idx);

	if(display.status & DISPLAY_HIDEDOTS) {
	    display.prebuf[idx] &= ~DISPLAY_DOT;
	} else {
	    display.prebuf[idx] |= DISPLAY_DOT;
	}
    } else {
	if(display.dot_prebuf & _BV(7 - idx)) {
	    display.prebuf[idx] &= ~DISPLAY_DOT;
	}

	display.dot_prebuf &= ~_BV(7 - idx);
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
		display.trans_timer = 2 * DISPLAY_SIZE;
		break;

	    case DISPLAY_TRANS_INSTANT:
		    for(uint8_t i = 0; i < DISPLAY_SIZE; ++i) {
			display.postbuf[i] = display.prebuf[i];
		    }

		    display.dot_postbuf   = display.dot_prebuf;
		    display.colon_postbuf = display.colon_prebuf;

		    display.trans_type = DISPLAY_TRANS_NONE;
		    break;

	    default:
		break;
	}
    }
}

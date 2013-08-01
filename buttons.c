// buttons.c  --  button press detection
// (button presses are processed in mode.c)
//
// Note that PD5 is the default menu button pin, but PC4 is used
// instead when the anode-cathode to-spec hack is enabled.
// 
//    PD5*   menu button
//    PD4    plus button
//    PB0    set button
//
// * PB4--not PD5--is used to control the BLANK pin if and only if
//   the anode-grid to-spec hack is enabled.
//


#include <avr/io.h>  // for using avr register names


#include "buttons.h"
#include "piezo.h"  // for clicking on button presses
#include "usart.h"  // for debugging output


// menu button bit and registers
#ifdef VFD_TO_SPEC
#define MENU_BIT  PB4
#define MENU_PORT PORTB
#define MENU_DDR  DDRB
#define MENU_PIN  PINB
#else
#define MENU_BIT  PD5
#define MENU_PORT PORTD
#define MENU_DDR  DDRD
#define MENU_PIN  PIND
#endif  // VFD_TO_SPEC

// set button bit and registers
#define SET_BIT  PB0
#define SET_PORT PORTB
#define SET_DDR  DDRB
#define SET_PIN  PINB

// plus button bit and registers
#define PLUS_BIT  PD4
#define PLUS_PORT PORTD
#define PLUS_DDR  DDRD
#define PLUS_PIN  PIND


// extern'ed button input data
volatile buttons_t buttons;


// set initial state after system reset
void buttons_init(void) {
    buttons.state = buttons.pressed = 0;
    buttons_sleep(); // clamp button pins to ground
}


// clamp button pins to ground
void buttons_sleep(void) {
    // disable pull-up resistors
    MENU_PORT &= ~_BV(MENU_BIT);
    SET_PORT  &= ~_BV(SET_BIT);
    PLUS_PORT &= ~_BV(PLUS_BIT);

    // set to output (clamp to ground)
    MENU_DDR |= _BV(MENU_BIT);
    SET_DDR  |= _BV(SET_DDR);
    PLUS_DDR |= _BV(PLUS_DDR);
}


// enable button pins as inputs with pull-ups enabled
void buttons_wake(void) {
    // set to input
    MENU_DDR &= ~_BV(MENU_BIT);
    SET_DDR  &= ~_BV(SET_BIT);
    PLUS_DDR &= ~_BV(PLUS_BIT);

    // enable pull-up resistors
    MENU_PORT |= _BV(MENU_BIT);
    SET_PORT  |= _BV(SET_BIT);
    PLUS_PORT |= _BV(PLUS_BIT);
}


// check for button presses every semisecond
void buttons_semitick(void) {
    uint8_t sensed = 0;  // which buttons are pressed?

    // check the menu button (button one)
    if(!(MENU_PIN & _BV(MENU_BIT))) sensed |= BUTTONS_MENU;

    // check the plus button (button three)
    if(!(PLUS_PIN & _BV(PLUS_BIT))) sensed |= BUTTONS_PLUS;

    // check the set button (button two)
    if(!(SET_PIN & _BV(SET_BIT))) sensed |= BUTTONS_SET;

    // timers for button debouncing and repeating
    static uint8_t debounce_timer = 0;
    static uint16_t pressed_timer = 0;

    // debounce before storing the currently pressed buttons
    if(buttons.pressed != sensed && (buttons.state & 0x0F) == sensed) {
	if(++debounce_timer >= BUTTONS_DEBOUNCE_TIME) {
	    buttons.pressed = sensed;
	    buttons.state &= 0x0F;  // clear process and repeating flags
	    pressed_timer = 0;
	}
    } else {
	buttons.state &= 0xF0;   // clear button flags
	buttons.state |= sensed; // set new button flags
	debounce_timer = 0;
    }

    // if any buttons are pressed, periodically clear processed flag
    // to allow for press-and-hold repeating
    if(buttons.pressed) {
	++pressed_timer;

	if(buttons.state & BUTTONS_REPEATING) {
	    if(pressed_timer >= BUTTONS_REPEAT_RATE) {
		buttons.state &= ~BUTTONS_PROCESSED;
		pressed_timer = 0;
	    }
	} else {
	    if(pressed_timer >= BUTTONS_REPEAT_AFTER) {
		buttons.state |= BUTTONS_REPEATING;
	    }
	}
    }
}


// process a button press: if a button is pressed and not
// previously processed, return it.  otherwise return zero.
uint8_t buttons_process(void) {
    // return nothing if there is no button pressed
    // or if any buttons pressed have already been processed
    if(buttons.state & BUTTONS_PROCESSED || !buttons.pressed) return 0;

    // mark this button press as processed
    buttons.state |= BUTTONS_PROCESSED;

    // make a nice, satisfying click with processed button press
    piezo_click();

    // return the pressed buttons
    return buttons.pressed;
}

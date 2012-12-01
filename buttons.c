// buttons.c  --  button press detection
// (button presses are processed in mode.c)
// 
//    PD5    menu button
//    PD4    plus button
//    PB0    set button
// 


#include <avr/io.h>  // for using avr register names


#include "buttons.h"
#include "pizo.h"   // for clicking on button presses
#include "usart.h"  // for debugging output


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
    PORTD &= ~_BV(PD5) & ~_BV(PD4);
    PORTB &= ~_BV(PB0);

    // set to output (clamp to ground)
    DDRD |= _BV(PD5) | _BV(PD4);
    DDRB |= _BV(PB0);
}


// enable button pins as inputs with pull-ups enabled
void buttons_wake(void) {
    // set to input
    DDRD &= ~_BV(PD5) & ~_BV(PD4);
    DDRB &= ~_BV(PB0);

    // enable pull-up resistors
    PORTD |= _BV(PD5) | _BV(PD4);
    PORTB |= _BV(PB0);
}


// check for button presses every semisecond
void buttons_semitick(void) {
    uint8_t sensed = 0;  // which buttons are pressed?

    // check the menu button (button one)
    if(!(PIND & _BV(PD5))) sensed |= BUTTONS_MENU;

    // check the set button (button two)
    if(!(PINB & _BV(PB0))) sensed |= BUTTONS_SET;

    // check the plus button (button three)
    if(!(PIND & _BV(PD4))) sensed |= BUTTONS_PLUS;

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
    pizo_click();

    // return the pressed buttons
    return buttons.pressed;
}

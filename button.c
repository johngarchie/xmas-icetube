#include <avr/io.h>  // for using avr register names

#include "button.h"
#include "alarm.h"


// extern'ed button input data
volatile button_t button;


// set initial state after system reset
void button_init(void) {
    button.state = button.pressed = 0;
    button_sleep(); // clamp button pins to ground
}


// clamp button pins to ground
void button_sleep(void) {
    // disable pull-up resistors
    PORTD &= ~_BV(PD5) & ~_BV(PD4);
    PORTB &= ~_BV(PB0);

    // set to output (clamp to ground)
    DDRD |= _BV(PD5) | _BV(PD4);
    DDRB |= _BV(PB0);
}


// enable button pins as inputs with pull-ups enabled
void button_wake(void) {
    // set to input
    DDRD &= ~_BV(PD5) & ~_BV(PD4);
    DDRB &= ~_BV(PB0);

    // enable pull-up resistors
    PORTD |= _BV(PD5) | _BV(PD4);
    PORTB |= _BV(PB0);
}


// check for button presses every semisecond
void button_semitick(void) {
    uint8_t sensed = 0;  // which buttons are pressed?

    // check the menu button (button one)
    if(!(PIND & _BV(PD5))) sensed |= BUTTON_MENU;

    // check the set button (button two)
    if(!(PINB & _BV(PB0))) sensed |= BUTTON_SET;

    // check the plus button (button three)
    if(!(PIND & _BV(PD4))) sensed |= BUTTON_PLUS;

    // timers for button debouncing and repeating
    static uint8_t debounce_timer = 0;
    static uint16_t pressed_timer = 0;

    // debounce before storing the currently pressed buttons
    if(button.pressed != sensed && (button.state & 0x0F) == sensed) {
	if(++debounce_timer >= BUTTON_DEBOUNCE_TIME) {
	    button.pressed = sensed;
	    button.state &= 0x0F;  // clear process and repeating flags
	    pressed_timer = 0;
	}
    } else {
	button.state &= 0xF0;   // clear button flags
	button.state |= sensed; // set new button flags
	debounce_timer = 0;
    }

    // if any buttons are pressed, periodically clear processed flag
    // to allow for press-and-hold repeating
    if(button.pressed) {
	++pressed_timer;

	if(button.state & BUTTON_REPEATING) {
	    if(pressed_timer >= BUTTON_REPEAT_RATE) {
		button.state &= ~BUTTON_PROCESSED;
		pressed_timer = 0;
	    }
	} else {
	    if(pressed_timer >= BUTTON_REPEAT_AFTER) {
		button.state |= BUTTON_REPEATING;
	    }
	}
    }
}


// process a button press: if a button is pressed and not
// previously processed, return it.  otherwise return zero.
uint8_t button_process(void) {
    // return nothing if there is no button pressed
    // or if any buttons pressed have already been processed
    if(button.state & BUTTON_PROCESSED || !button.pressed) return 0;

    // mark this button press as processed
    button.state |= BUTTON_PROCESSED;

    // make a nice, satisfying click with processed button press
    alarm_click();

    // return the pressed buttons
    return button.pressed;
}

// icetube.c  --  handles system initialization and interrupts
//
// The code here is minimal and, for the most part, simply invokes
// functionality contained within other files:
//
//    system.c     system management (idle and sleep loops)
//    time.c       date and time keeping
//    alarm.c      alarm functionality
//    pizo.c       pizo element control (music, beeps, clicks)
//    buttons.c    button inputs (menu, set, and plus)
//    display.c    boost, MAX6921, and VFD
//    mode.c       clock mode (displayed time, menus, etc.)
//    usart.c      serial communication
//    gps.c        time-from-GPS functionality
//


#include <stdint.h>         // for using standard integer types
#include <avr/io.h>	    // for defining fuse settings
#include <avr/interrupt.h>  // for defining interrupt handlers
#include <avr/power.h>      // for controlling system clock speed
#include <avr/wdt.h>        // for using the watchdog timer
#include <util/delay.h>     // for the _delay_ms() function


// headers for this project
#include "config.h"
#include "system.h"
#include "time.h"
#include "alarm.h"
#include "pizo.h"
#include "buttons.h"
#include "display.h"
#include "mode.h"
#include "usart.h"
#include "gps.h"


// define ATmega328p lock bits
LOCKBITS = 0xFF;  // no restrictions on memory access

// define ATmega328p fuse bits
FUSES = {
    .low      = 0x62,
    .high     = 0xD1,
#ifdef EXTERNAL_CLOCK
    .extended = 0xFD,  // bod at 2.7 volts, when DS32kHz fails
#else
    .extended = 0xFE,  // bod at 1.8 volts, when battery dead
#endif
};


// set to 1 every ~1 millisecond or so
uint8_t semitick_successful = 1;


// start everything for the first time
int main(void) {
    cli();  // disable interrupts until system initialized

    // initialize the system: each init function leaves
    // the system in a low-power configuration
    system_init();
    usart_init();
    time_init();
    buttons_init();
    alarm_init();
    pizo_init();
    display_init();
    mode_init();
    gps_init();

    // if the system is on battery power, sleep until power restored
    if(system_power() == SYSTEM_BATTERY) {
	system_sleep_loop();
    }

    // on normal power, the 8 MHz clock is safe
    clock_prescale_set(clock_div_1);

    sei(); // enable interrupts

    // wakey, wakey
    usart_wake();
    time_wake();
    buttons_wake();
    alarm_wake();
    pizo_wake();
    display_wake();
    mode_wake();
    gps_wake();
    
    // half-second beep on system reset
    pizo_setvolume(3, 0);
    pizo_beep(500);

    // clock function is entirelly interrupt-driven after
    // this point, so let the system idle indefinetly
    system_idle_loop();
}


// counter0 compare match interrupt
// triggered every second
// counter0 is clocked by the clock crystal
ISR(TIMER2_COMPA_vect) {
    sei();  // allow nested interrupts

    if(system.status & SYSTEM_SLEEP) {
	time_tick();
	alarm_tick();
	pizo_tick();
	wdt_reset();
    } else {
	time_tick();
	buttons_tick();
	alarm_tick();
	pizo_tick();
	mode_tick();
	display_tick();
	gps_tick();
	if(semitick_successful) wdt_reset();
	semitick_successful = 0;
    }
}


// timer0 overflow interrupt
// triggered every 32 microseconds (32.25 khz);
// pwm output from timer0 controls boost power
ISR(TIMER0_OVF_vect) {
    cli();  // deferring interrupts prevents display flicker

    static uint8_t varcounter = 1, semicounter = 1;

    if(! --varcounter) varcounter = display_varsemitick();

    sei();  // allow nested interrupts

    // interupt just returns 31 out of 32 times
    if(! --semicounter) {
	semicounter = 32;

	// code below runs every "semisecond" or
	// every 0.99 microseconds (1.01 khz)
	time_semitick();
	buttons_semitick();
	alarm_semitick();
	pizo_semitick();
	mode_semitick();
	display_semitick();
	gps_semitick();

	semitick_successful = 1;
    }
}


// analog comparator interrupt
// triggered when voltage at AIN1 falls below internal
// bandgap (~1.1v), indicating external power failure
ISR(ANALOG_COMP_vect) {
    cli();  // prevent nested interrupts

    // if the system is already sleeping, do nothing
    if(system.status & SYSTEM_SLEEP) return;

    // if power is good, do nothing
    if(system_power() == SYSTEM_ADAPTOR) return;

    display_sleep();  // stop boost timer and disable display
    alarm_sleep();    // disable alarm-switch pull-up resistor
    buttons_sleep();  // disable button pull-up resistors
    time_sleep();     // save current time
    mode_sleep();     // does nothing
    gps_sleep();      // disable usart rx interrupt
    usart_sleep();    // disable usart
    pizo_sleep();     // adjust buzzer for slower clock

    // the bod settings allow the clock to run a battery down to 1.7 - 2.0v.
    // An 8 or 4 MHz clock is unstable at 1.7v, but a 2 MHz clock is okay:
    clock_prescale_set(clock_div_4);

    system_sleep_loop();  // sleep until power restored

    time_wake();  // save current time

    clock_prescale_set(clock_div_1);  // 8 MHz system clock

    sei();  // allow interrupts

    pizo_wake();     // adjust buzzer for faster clock
    mode_wake();     // display time after waking
    buttons_wake();  // enable button pull-ups
    alarm_wake();    // enable alarm switch pull-up
    usart_wake();    // enable and configure usart
    gps_wake();      // enable usart rx interrupt
    display_wake();  // start boost timer and enable display
}

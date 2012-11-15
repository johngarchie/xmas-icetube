// icetube.c
//
// This is the main file for my Ice Tube Clock firmware.  The code here is
// minimal and mostly calls functions in other files.
//
// power.c   - power management
// time.c    - date and time keeping
// alarm.c   - alarm and buzzer
// button.c  - button inputs (menu, set, and plus)
// display.c - boost, MAX6921, and VFD
// mode.c    - clock mode (displayed time, menus, etc.)


#include <stdint.h>        // for using standard integer types
#include <avr/interrupt.h> // for defining interrupt handlers
#include <avr/power.h>     // for controlling system clock speed
#include <avr/wdt.h>       // for using the watchdog timer


// headers for this project
#include "power.h"
#include "time.h"
#include "alarm.h"
#include "button.h"
#include "display.h"
#include "mode.h"


// set to 1 every ~1 millisecond or so
uint8_t semitick_successful = 0;


// start everything for the first time
int main(void) {
    cli(); // disable interrupts until sleep or idle loop

    power_init(); // setup power manager

    // initialize the clock; each init function leaves
    // the system in a low-power configuration
    usart_init();
    time_init();
    button_init();
    alarm_init();
    display_init();
    mode_init();

    // if the system is on low power, sleep until power restored
    if(power_source() == POWER_BATTERY) power_sleep_loop();

    sei();  // allow interupts

    // with normal power, the mcu can safely run at 8 MHz
    clock_prescale_set(clock_div_1);

    // wake everything up
    usart_wake();
    time_wake();
    button_wake();
    alarm_wake();
    display_wake();
    mode_wake();

    // beep for one second on first powered startup
    alarm_beep(1000);

    // clock function is entirelly interrupt-driven after
    // this point, so let the system idle indefinetly
    power_idle_loop();
}


// counter0 compare match interrupt
// triggered every second
// counter0 is clocked by the clock crystal
ISR(TIMER2_COMPA_vect) {
    sei();  // allow nested interrupts

    if(power.status & POWER_SLEEP) {
	time_tick();
	alarm_tick();
	wdt_reset();
    } else {
	time_tick();
	button_tick();
	alarm_tick();
	mode_tick();
	display_tick();
	if(semitick_successful) wdt_reset();
	semitick_successful = 0;
    }
}


// timer0 overflow interrupt
// triggered every 32 microseconds (32.25 khz);
// pwm output from timer0 controls boost power
ISR(TIMER0_OVF_vect) {
    sei();  // allow nested interrupts

    // interupt just returns 31 out of 32 times
    static uint8_t skip_count = 0;
    if(++skip_count < 32) return;
    skip_count = 0;

    // code below runs every "semisecond" or
    // every 0.99 microseconds (1.01 khz)
    time_semitick();
    button_semitick();
    alarm_semitick();
    mode_semitick();
    display_semitick();

    semitick_successful = 1;
}


// analog comparator interrupt
// triggered when voltage at AIN1 falls below internal
// bandgap (~1.1v), indicating external power failure
ISR(ANALOG_COMP_vect) {
    // if the system is already sleeping, do nothing
    if(power.status & POWER_SLEEP) return;

    // if power is good, do nothing
    if(power_source() == POWER_ADAPTOR) return;

    display_sleep();  // stop boost timer and disable display
    usart_sleep();    // disable usart
    time_sleep();     // save current time
    alarm_sleep();    // disable alarm switch pull-up resistor
    button_sleep();   // disable button pull-up resistors

    // the bod settings allow the clock to run a battery down to 1.7 - 2.0v.
    // An 8 or 4 MHz clock is unstable at 1.7v, but a 2 MHz clock is okay:
    clock_prescale_set(clock_div_4);

    power_sleep_loop(); // sleep until power restored

    time_savetime();  // save current time in case of disaster
    sei();            // allow interrupts
    clock_prescale_set(clock_div_1); // restore normal clock speed

    button_wake();   // enable button pull-ups
    alarm_wake();    // enable alarm switch pull-up
    time_wake();     // does nothing (empty function)
    usart_sleep();   // enable and configure usart
    display_wake();  // start boost timer and enable display
}

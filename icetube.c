#include <stdint.h>        // for using standard integer types
#include <avr/interrupt.h> // for defining interrupt handlers
#include <avr/power.h>     // for controlling system clock speed
#include <avr/wdt.h>       // for using the watchdog timer

// include headers for all submodules
#include "power.h"
#include "time.h"
#include "alarm.h"
#include "button.h"
#include "display.h"
#include "mode.h"


// start ice tube clock
int main(void) {
    // disable interrupts until sleep loop or idle loop
    cli();
    wdt_disable();

    power_init();

    // initialize the clock; each init function leaves
    // the system in a low-power configuration
    time_init();
    button_init();
    alarm_init();
    display_init();
    mode_init();

    // if the system is on low power
    if(power_source() == POWER_BATTERY) {
	power_sleep_loop();
    }

    // wake the system up
    time_wake();
    button_wake();
    alarm_wake();
    display_wake();
    mode_wake();

    // beep for one second on first powered startup
    alarm_beep(1000);

    // functions are entirelly interrupt-driven after
    // this point, so let the system idle indefinetly
    power_idle_loop();
}


// counter0 compare match interrupt;
// triggered every second
// counter0 is clocked by the clock crystal
ISR(TIMER2_COMPA_vect) {
    sei();  // allow nested interrupts

    if(power.status == POWER_SLEEP) {
	time_tick();
    } else {
	time_tick();
	button_tick();
	alarm_tick();
	mode_tick();
	display_tick();
    }
}


// timer0 overflow interrupt;
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
}


// analog comparator interrupt,
// triggered when voltage at AIN1 falls below internal
// bandgap (~1.1v), indicating external power failure
ISR(ANALOG_COMP_vect) {
    // if the system is already sleeping, do nothing
    if(power.status == POWER_SLEEP) return;

    // the bod settings allow the clock to run a battery down to 1.7 - 2.0v.
    // An 8 or 4 MHz clock is unstable at 1.7v, but a 2 MHz clock is okay:
    clock_prescale_set(clock_div_4);

    display_sleep();  // stop boost timer and disable display
    time_sleep();     // save current time
    alarm_sleep();    // disable alarm switch pull-up resistor
    button_sleep();   // disable button pull-up resistors

    power_sleep_loop(); // sleep until power restored

    clock_prescale_set(clock_div_1); // restore normal clock speed
    
    button_wake();    // enable button pull-ups
    alarm_wake();     // enable alarm switch pull-up
    time_wake();      // does nothing (empty function)
    display_wake();   // start boost timer and enable display
}

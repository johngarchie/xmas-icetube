// icetube.c  --  handles system initialization and interrupts
//
// The code here is minimal and, for the most part, simply invokes
// functionality contained within other files:
//
//    alarm.c      alarm functionality
//    buttons.c    button inputs (menu, set, and plus)
//    display.c    boost, MAX6921, and VFD
//    gps.c        time-from-GPS functionality
//    mode.c       clock mode (displayed time, menus, etc.)
//    piezo.c      piezo element control (music, beeps, clicks)
//    system.c     system management (idle and sleep loops)
//    temp.c       temperature sensing
//    time.c       date and time keeping
//    usart.c      serial communication
//


#include <stdint.h>         // for using standard integer types
#include <avr/io.h>	    // for defining fuse settings
#include <avr/interrupt.h>  // for defining interrupt handlers
#include <avr/power.h>      // for controlling system clock speed
#include <avr/wdt.h>        // for using the watchdog timer
#include <util/atomic.h>    // for noninterruptable code blocks
#include <avr/eeprom.h>     // for storing data in eeprom memory


// headers for this project
#include "config.h"
#include "system.h"
#include "time.h"
#include "alarm.h"
#include "piezo.h"
#include "buttons.h"
#include "display.h"
#include "mode.h"
#include "usart.h"
#include "gps.h"
#include "temp.h"


// define ATmega328p lock bits
#ifdef __AVR_ATmega328P__
// disable self-programming to prevent flash corruption
LOCKBITS = BLB0_MODE_2 & BLB1_MODE_2;
#else
#error LOCKBITS not defined for MCU
#endif  // __AVR_ATmega328P__

// define fuse bits for the ATmega168 and ATmega328p
#ifdef __AVR_ATmega328P__
FUSES = {
    .low      = 0x62,
    .high     = 0xD1,
    .extended = 0xFE,
};
#else
#error FUSES not defined for MCU
#endif  // __AVR_ATmega328P__


// according to some reports, the first byte of EEPROM memory is
// unreliable, so allocate the first byte and never use it
uint8_t ee_unreliable_byte EEMEM = 0;


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
    piezo_init();
    display_init();
    mode_init();
    gps_init();
    temp_init();

    // if on battery power, sleep until external power restored
    if(system_power() == SYSTEM_BATTERY) {
	system_sleep_loop();
    }

    // on external power, the 8 MHz clock is safe
    clock_prescale_set(clock_div_1);

    sei(); // enable interrupts

    // start all system functions
    system_wake();
    usart_wake();
    time_wake();
    buttons_wake();
    alarm_wake();
    piezo_wake();
    display_wake();
    mode_wake();
    gps_wake();
    
    // half-second beep on system reset
    piezo_setvolume(3, 0);
    piezo_beep(500);

    // clock function is entirelly interrupt-driven after
    // this point, so let the system idle indefinetly
    system_idle_loop();
}


// counter0 compare match interrupt
// triggered every second
// counter0 is clocked by the clock crystal
ISR(TIMER2_COMPB_vect) {
    if(system.status & SYSTEM_SLEEP) {
	system_tick();
	time_tick();
	alarm_tick();
	piezo_tick();
	temp_tick();
	wdt_reset();
    } else {
	if(semitick_successful) wdt_reset();
	semitick_successful = 0;

	system_tick();
	time_tick();
	buttons_tick();
	alarm_tick();
	piezo_tick();
	mode_tick();
	display_tick();
	gps_tick();
	usart_tick();
	temp_tick();
    }
}


// timer0 overflow interrupt
// triggered every 32 microseconds (31.25 khz);
// pwm output from timer0 controls boost power
ISR(TIMER0_OVF_vect) {
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
	static uint8_t varcounter = 1;
	if(varcounter && !--varcounter) {
	    varcounter = display_varsemitick();
	}
#ifdef TEMPERATURE_SENSOR
	while(temp.missed_ovf) {
	    --temp.missed_ovf;
	    if(varcounter && !--varcounter) {
		varcounter = display_varsemitick();
	    }
	}
#endif  // TEMPERATURE_SENSOR
    }

    ATOMIC_BLOCK(ATOMIC_FORCEON) {
	display_semisemitick();

	// interupt just returns 31 out of 32 times
	static uint8_t semicounter = 1;
	if(semicounter && !--semicounter) {
	    NONATOMIC_BLOCK(NONATOMIC_FORCEOFF) {
		// code below runs every "semisecond" or
		// every 1.02 microseconds (0.98 khz)
		system_semitick();
		time_semitick();
		buttons_semitick();
		alarm_semitick();
		piezo_semitick();
		mode_semitick();
		display_semitick();
		gps_semitick();
		usart_semitick();
		temp_semitick();

		semitick_successful = 1;
		semicounter = 32;
	    }
	}
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
    piezo_sleep();    // adjust buzzer timer for slower clock
    temp_sleep();     // disable temperature sensor
    system_sleep();   // does nothing

    // the bod settings allow the clock to run a battery down to 1.7 - 2.0v.
    // An 8 or 4 MHz clock is unstable at 1.7v, but a 2 MHz clock is okay:
    clock_prescale_set(clock_div_4);

    system_sleep_loop();  // sleep until power restored

    time_wake();  // save current time

    clock_prescale_set(clock_div_1);  // 8 MHz system clock

    sei();  // allow interrupts

    system_wake();   // does nothing
    temp_wake();     // enable temperature sensor
    piezo_wake();    // adjust buzzer for faster clock
    mode_wake();     // update display contents when necessary
    buttons_wake();  // enable button pull-ups
    alarm_wake();    // enable alarm switch pull-up
    usart_wake();    // enable and configure usart
    gps_wake();      // enable usart rx interrupt
    display_wake();  // start boost timer and enable display
}

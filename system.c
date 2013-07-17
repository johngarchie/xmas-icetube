// system.c  --  system functions (idle, sleep, interrupts)
//
//    PB4 (MISO)           unused pin (unless anode-grid to-spec hack)
//    PC2                  unused pin
//    PC1                  power from voltage regulator or unused pin
//    AIN1 (PD7)           divided system voltage
//    analog comparator    detects low voltage (AIN1)
//
// system_init() disables all modules in PRR register: TWI, timer2, timer1,
// timer0, SPI, USART, and ADC.  These modules are and disabled as-needed in
// the various *_wake() functions in other *.c files.
//


#include <avr/interrupt.h> // for enabling and disabling interrupts
#include <avr/power.h>     // for disabling microcontroller modules
#include <avr/sleep.h>     // for entering low-power modes
#include <avr/wdt.h>       // for using the watchdog timer
#include <util/atomic.h>   // for non-interruptable blocks
#include <util/delay.h>    // for enabling delays


#include "system.h"
#include "usart.h"  // for debugging output


// extern'ed system status data
volatile system_t system;


// enable low-power detection and disables microcontroller modules
void system_init(void) {
    system.initial_mcusr = MCUSR;  // save MCUSR
    MCUSR = 0;  // clear any watchdog timer flags
    wdt_enable(WDTO_8S);  // enable eight-second watchdog timer

    system.status &= ~SYSTEM_SLEEP;

    // enable pull-up resistors on unused pins to ensure a defined value
#ifndef VFD_ANODE_GRID_TO_SPEC
    PORTB |= _BV(PB4);
#endif  // ~VFD_ANODE_GRID_TO_SPEC
    // leave PC1 tri-stated in case clock is wired
    // for the depricated exteneded battery hack
#ifndef VFD_CATHODE_TO_SPEC
    PORTC |= _BV(PC2);
#endif  // ~VFD_CATHODE_TO_SPEC

    // use internal bandgap as reference for analog comparator
    // and enable analog comparator interrupt on falling edge
    // of AIN1 (interrupt triggers when adaptor power fails)
    ACSR = _BV(ACBG) | _BV(ACIE) | _BV(ACI);

    // disable digital input on analog comparator input, AIN1
    DIDR1 = _BV(AIN1D);

    // disable all features for power savings; individual features are enabled
    // one-by-one in appropriate submodules
    power_all_disable();
}


// repeatedly enter idle mode forevermore
void system_idle_loop(void) {
    sleep_enable();
    for(;;) {
	cli();
	set_sleep_mode(SLEEP_MODE_IDLE);
	sei();
	sleep_cpu();
    }
}


// repeatedly enter power save mode until power restored
void system_sleep_loop(void) {
    sleep_enable();                 // permit sleep mode
    system.status |= SYSTEM_SLEEP;  // set sleep flag
    wdt_disable();                  // disable watchdog
    ACSR = _BV(ACD) | _BV(ACI);     // disable analog comparator

    do {
	do {
	    // if the alarm buzzer is going, remain in idle mode
	    // to keep buzzer active for the next second
	    if(system.status & SYSTEM_ALARM_SOUNDING) {
		set_sleep_mode(SLEEP_MODE_IDLE);
	    } else {
		set_sleep_mode(SLEEP_MODE_PWR_SAVE);
	    }

	    // wait until asynchronous updates are complete
	    // or system might fail to wake from sleep
	    while(ASSR & (  _BV(TCN2UB) 
			  | _BV(OCR2AUB) | _BV(OCR2BUB)
			  | _BV(TCR2AUB) | _BV(TCR2BUB) ));

	    // disable analog comparator
	    ACSR = _BV(ACD);

	    // save power during sleep:
	    // disable analog comparator and bod which
	    // indirectly disables the internal bandgap
	    sleep_bod_disable();

	    // enter sleep mode
	    sei();
	    sleep_cpu();
	    cli();

	    // analog comparator will have already been enabled
	    // in the TIMER2_COMPB_vect interrupt (icetube.c)
	} while(system_power() == SYSTEM_BATTERY);

	// debounce power-restored signal; delay is actually 100 ms
	_delay_ms(25);  // because system clock is divided by four
    } while(system_power() == SYSTEM_BATTERY);

    wdt_enable(WDTO_8S);
    wdt_reset();

    // enable analog comparator interrupt
    ACSR = _BV(ACBG) | _BV(ACIE) | _BV(ACI);

    system.status &= ~SYSTEM_SLEEP; // clear sleep flag
}


// checks the analog comparator and returns current power source
uint8_t system_power(void) {
    if(ACSR & _BV(ACO)) {
	return SYSTEM_BATTERY;
    } else {
	return SYSTEM_ADAPTOR;
    }
}

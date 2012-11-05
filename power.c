#include <avr/interrupt.h> // for enabling and disabling interrupts
#include <avr/power.h>     // for disabling all mcu systems
#include <avr/sleep.h>     // for entering low-power modes
#include <util/delay.h>    // for debounding delay

#include "power.h"

volatile power_t power;

void power_init(void) {
    power.status = POWER_WAKE;

    // use internal bandgap as reference for analog comparator
    // and enable analog comparator interrupt on falling edge
    // of AIN1 (interrupt triggers when adaptor power fails)
    ACSR = _BV(ACBG) | _BV(ACIE) | _BV(ACIS1);

    // disable digital input on analog comparator input, AIN1
    DIDR1 = _BV(AIN1D);

    // disable all features for power savings; individual features are enabled
    // one-by-one in appropriate submodules
    power_all_disable();
}


// repeatedly enter idle mode forevermore
void power_idle_loop(void) {
    sleep_enable();
    for(;;) {
	cli();
	set_sleep_mode(SLEEP_MODE_IDLE);
	sei();
	sleep_cpu();
    }
}


// repeatedly enter power save mode until power restored
void power_sleep_loop(void) {
    power.status = POWER_SLEEP; // record current power mode

    // configure sleep mode
    sleep_enable();
    set_sleep_mode(SLEEP_MODE_PWR_SAVE);

    do {
	do {
	    cli();
#ifdef sleep_bod_disable
	    // if bod can be disabled, the bandgap voltage reference
	    // will also be disabled during sleep as long as the analog
	    // comparator is detached from the bandgap

	    ACSR = 0; // detach comparator from bandgap
	    sleep_bod_disable();
#endif
	    sei();
	    sleep_cpu();
	    cli();
#ifdef sleep_bod_disable
	    ACSR = _BV(ACBG); // link analog comparator to bandgap

	    _NOP(); // stall for 2 cycles (analog comparator
	    _NOP(); // requires 1-2 cycles to update AC0)
#endif
	} while(ACSR & _BV(ACO));
	_delay_ms(10);  // debounce power-restored signal

    } while(ACSR & _BV(ACO));

    // re-enable analog comparator interrupt to detect power failure
    ACSR = _BV(ACBG) | _BV(ACIE) | _BV(ACIS1);

    power.status = POWER_WAKE; // record current power mode
}


uint8_t power_source(void) {
    if(ACSR & _BV(ACO)) {
	return POWER_BATTERY;
    } else {
	return POWER_ADAPTOR;
    }
}

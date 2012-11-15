#include <avr/interrupt.h> // for enabling and disabling interrupts
#include <avr/power.h>     // for disabling microcontroller modules
#include <avr/sleep.h>     // for entering low-power modes
#include <avr/wdt.h>       // for using the watchdog timer
#include <util/delay.h>    // for debouncing delay
#include <avr/cpufunc.h>

#include "power.h"


// extern'ed power status data
volatile power_t power;


// enable low-power detection and disables microcontroller modules
void power_init(void) {
    power.initial_mcusr = MCUSR;  // save MCUSR
    MCUSR = 0;  // clear any watchdog timer flags
    wdt_reset();  // reset default watchdog timer
    wdt_disable();
    //wdt_enable(WDTO_2S);  // enable watchdog timer

    power.status &= ~POWER_SLEEP;

    // enable pull-up resistors on unused pins to ensure a defined value
    PORTC |= _BV(PC5) | _BV(PC4) | _BV(PC2) | _BV(PC1) | _BV(PC0);
    PORTB |= _BV(PB4); // msoi

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
    sleep_enable();  // permit sleep mode

    power.status |= POWER_SLEEP; // set sleep flag

    do {
	do {
	    // if the alarm buzzer is going, remain in idle mode to keep buzzer
	    // active for the next second
	    if(power.status & POWER_ALARMON) {
		set_sleep_mode(SLEEP_MODE_IDLE);
	    } else {
		set_sleep_mode(SLEEP_MODE_PWR_SAVE);
	    }

	    sei();
	    sleep_cpu();
	    cli();
	} while(power_source() == POWER_BATTERY);
	// debounce power-restored signal; delay is actually 100 ms
	_delay_ms(25);  // because clock is divided by four
    } while(power_source() == POWER_BATTERY);

    power.status &= ~POWER_SLEEP; // clear sleep flag
}


// checks the analog comparator and returns current power source
uint8_t power_source(void) {
    if(ACSR & _BV(ACO)) {
	return POWER_BATTERY;
    } else {
	return POWER_ADAPTOR;
    }
}

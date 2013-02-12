// system.c  --  system functions (idle, sleep, interrupts)
//
//    PB4 (MISO)           unused pin
//    PC2                  unused pin
//    PC1                  power from voltage regulator
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
    PORTB |= _BV(PB4);
#ifdef PICO_POWER
    PORTC |= _BV(PC2);
#else
    PORTC |= _BV(PC2) | _BV(PC1);
#endif  // PICO_POWER

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
    sleep_enable();  // permit sleep mode

    system.status |= SYSTEM_SLEEP; // set sleep flag

#ifdef PICO_POWER
    wdt_disable();
#endif  // PICO_POWER

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

	    sei();

	    sleep_cpu();

	    cli();

#ifdef PICO_POWER
	    // disable analog comparator if comparator enabled
	    // and voltage absent at voltage regulator
	    if(!(ACSR & _BV(ACD)) && !(PINC & _BV(PC1))) {
		// disable analog comparator
		ACSR = _BV(ACD) | _BV(ACI);
	    }

	    // enable analog comparator if comparator disabled
	    // and digital input on comparator pin is true
	    if((ACSR & _BV(ACD)) && (PINC & _BV(PC1))) {
		// enable analog comparator to bandgap
		ACSR = _BV(ACBG) | _BV(ACIE) | _BV(ACI);

		_delay_us(100);  // bandgap startup time

		// clear analog comparator interrupt
		ACSR = _BV(ACBG) | _BV(ACIE) | _BV(ACI);
	    }
	} while((ACSR & _BV(ACD)) || system_power() == SYSTEM_BATTERY);
#else
	} while(system_power() == SYSTEM_BATTERY);
#endif  // PICO_POWER
	// debounce power-restored signal; delay is actually 100 ms
	_delay_ms(25);  // because system clock is divided by four
    } while(system_power() == SYSTEM_BATTERY);

#ifdef PICO_POWER
    wdt_enable(WDTO_8S);
    wdt_reset();
#endif  // PICO_POWER

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

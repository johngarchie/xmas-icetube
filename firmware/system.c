// system.c  --  system functions (idle, sleep, interrupts)
//
//    PB4 (MISO)           unused pin (unless anode-grid to-spec hack)
//    PC2*                 unused pin
//    PC1                  power from voltage regulator or unused pin
//    AIN1 (PD7)           divided system voltage
//    analog comparator    detects low voltage (AIN1)
//
// * PC2 is unused and configured with the pull-up resistor unless the
//   IV-18 to-spec hack has been configured.
//
// system_init() disables all modules in PRR register: TWI, timer2, timer1,
// timer0, SPI, USART, and ADC.  These modules are and disabled as-needed in
// the various *_wake() functions in other *.c files.
//


#include <avr/interrupt.h> // for enabling and disabling interrupts
#include <avr/power.h>     // for disabling microcontroller modules
#include <avr/sleep.h>     // for entering low-power modes
#include <avr/wdt.h>       // for using the watchdog timer
#include <util/atomic.h>   // for non-interruptible blocks
#include <util/delay.h>    // for enabling delays


#include "system.h"
#include "usart.h"  // for debugging output
#include "mode.h"   // to refresh time when clearing low battery warning


// extern'ed system status data
volatile system_t system;


// private function declarations
void system_check_battery(void);


// enable low-power detection and disables microcontroller modules
void system_init(void) {
    system.initial_mcusr = MCUSR;  // save MCUSR
    MCUSR = 0;  // clear any watchdog timer flags
    wdt_enable(WDTO_8S);  // enable eight-second watchdog timer

    system.status &= ~SYSTEM_SLEEP;

    // enable pull-up resistors on unused pins to ensure a defined value
#if !defined(VFD_TO_SPEC) || defined(XMAS_DESIGN)
    PORTB |= _BV(PB4);
#endif  // ~VFD_TO_SPEC || XMAS_DESIGN

    // leave PC1 tri-stated in case clock is still wired
    // for the now depricated exteneded battery hack

#ifndef VFD_TO_SPEC
    PORTC |= _BV(PC2);
#endif  // ~VFD_TO_SPEC

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


// when clock goes to sleep, restart sleep timer
void system_sleep(void) {
    system.sleep_timer = 0;
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
    ACSR = _BV(ACD) | _BV(ACI);     // disable analog comparator and
    				    // clear analog comparator interrupt
    do {
	do {
	    // disable watchdog after five seconds to ensure
	    // quartz crystal is running reasonably well
	    // (otherwise, the clock will fail to wake from sleep)
	    if(system.sleep_timer == SYSTEM_WDT_DISABLE_DELAY) {
		wdt_disable();
	    }

	    // check battery status after specified delay
	    if(system.sleep_timer == SYSTEM_BATTERY_CHECK_DELAY) {
		system_check_battery();
	    }

	    // disable analog comparator to save power; analog comparator
	    // will be enabled in system_tick() or system_wake()
	    ACSR = _BV(ACD);

	    // wait until asynchronous updates are complete
	    // or system might fail to wake from sleep
	    while(ASSR & (  _BV(TCN2UB) 
			  | _BV(OCR2AUB) | _BV(OCR2BUB)
			  | _BV(TCR2AUB) | _BV(TCR2BUB) ));

	    if(system.status & SYSTEM_ALARM_SOUNDING) {
		// if the alarm buzzer is active, remain in idle mode
		// so buzzer continues sounding for next second
		set_sleep_mode(SLEEP_MODE_IDLE);
	    } else {
		// otherwise, sleep with BOD disabled to save power
		set_sleep_mode(SLEEP_MODE_PWR_SAVE);
		sleep_bod_disable();
	    }

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


// check battery voltage during sleep
void system_check_battery(void) {
    // enable analog to digital converter
    power_adc_enable();

    // select bandgap as analog to digital input
    //   REFS1:0 =   00:  VREF pin as voltage reference
    //   MUX3:0  = 1110:  1.1v bandgap as input
    ADMUX = _BV(MUX3) | _BV(MUX2) | _BV(MUX1);

    // configure analog to digital converter
    // ADEN    =   1:  enable analog to digital converter
    // ADSC    =   1:  start ADC conversion now
    // ADPS2:0 = 100:  system clock / 16  (4 MHz / 4 = 125 kHz)
    ADCSRA = _BV(ADEN) | _BV(ADSC) | _BV(ADPS2);

    // wait for conversion to complete
    while(ADCSRA & _BV(ADSC));

    uint16_t adc_curr = 0, adc_prev  = 0;
    int16_t  adc_err  = 0;
    uint8_t  adc_good = 0, adc_count = 0;

    // the analog-to-digital converter may take a while to converge
    while(adc_good < SYSTEM_BATTERY_GOOD_CONV
	    && adc_count++ < SYSTEM_BATTERY_MAX_CONV) {
       ADCSRA |= _BV(ADSC);            // start adc conversion
       while(ADCSRA & _BV(ADSC));      // wait for result
       adc_curr = ADC;                 // save current value
       adc_err = adc_prev - adc_curr;  // calculate error--
                                       // difference from previous value

       // if negligable error, result is good
       if(-SYSTEM_BATTERY_ADC_ERROR <= adc_err
	       && adc_err <= SYSTEM_BATTERY_ADC_ERROR) {
           ++adc_good;    // count consecutative good results
       } else {
           adc_good = 0;  // otherwise reset result counter
       }

       adc_prev = adc_curr;  // save current value as previous value
    }

    // disable analog to digital converter
    ADCSRA = 0;  // disable ADC before power_adc_disable()
    power_adc_disable();

    // set or clear low battery flag as required
    if(adc_curr > 1024UL * 1100 / LOW_BATTERY_VOLTAGE) {
       system.status |= SYSTEM_LOW_BATTERY;
    } else {
       system.status &= ~_BV(SYSTEM_LOW_BATTERY);
    }
}


// return true if pressed button should clear low battery warning
uint8_t system_onbutton(void) {
    if(!(system.status & SYSTEM_SLEEP) && system.status & SYSTEM_LOW_BATTERY) {
       system.status &= ~SYSTEM_LOW_BATTERY;
       mode_tick();  // force refresh of time display
       return 1;
    }

    return 0;
}

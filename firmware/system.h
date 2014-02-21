#ifndef SYSTEM_H
#define SYSTEM_H


#include <stdint.h>  // for using standard integer types

#include "config.h"  // for configuration macros


// LOW BATTERY WARNING / MEASURING BATTERY VOLTAGE
//
// The microcontroller can measure system voltage through the
// analog-to-digital converter (ADC), so battery voltage is
// determined by measuring system voltage during sleep after
// voltage falls to the battery level.  The analog-to-digital
// converter (ADC) is unreliable when it first starts, so battery
// voltage is checked repeatedly until the ADC gives consistent
// results.  The following macros control the battery voltage
// measurement algorithm.

// how long to wait after going to sleep before measuring battery
// voltage.  with the extended battery hack, system voltage may
// take 3-5 minutes to drop to the battery level, so 600 seconds
// (10 minutes) should be a safe choice
#define SYSTEM_BATTERY_CHECK_DELAY 600  // seconds

// how many consecutive good ADC readings must be read before
// using the final reading to estimate battery voltage
#define SYSTEM_BATTERY_GOOD_CONV 3

// how close must an ADC reading be to the previous reading to be
// considered good; each unit is roughly 0.23% or 0.006v.  a
// value of 4 corresponds to a battery level that is within
// within ~1% or ~0.025v of the previous value
#define SYSTEM_BATTERY_ADC_ERROR 4

// how many ADC conversions should be attempted before giving up
// and using the final reading to estimate battery voltage
#define SYSTEM_BATTERY_MAX_CONV 16


// CRYSTAL ROBUSTNESS / PREVENTING SLEEP LOCKUPS
//
// if the crystal oscillator is not running while the system sleeps the
// system will not wake.  for robustness, the watchdog timer is only
// disabled after the following time delay
#define SYSTEM_WDT_DISABLE_DELAY 5  // seconds


// return codes for the system_power() function
enum {
    SYSTEM_ADAPTOR,
    SYSTEM_BATTERY,
};


// flags for system.status
#define SYSTEM_SLEEP          0x01
#define SYSTEM_ALARM_SOUNDING 0x02
#define SYSTEM_LOW_BATTERY    0x04


typedef struct {
    uint8_t  status;         // system status flags
    uint8_t  initial_mcusr;  // initial value of MCUSR register
    uint32_t sleep_timer;    // amount of time in sleep mode
} system_t;


extern volatile system_t system;


void system_init(void);

inline void system_wake(void) {};
void system_sleep(void);

inline void system_tick(void) { 
    // if sleeping, enable analog comparater so system will know
    // if external power has been restored;  this must be done
    // here instead of in system_sleep_loop() because the analog
    // comparator needs a few microseconds to start
    if(system.status & SYSTEM_SLEEP) {
       ACSR = _BV(ACBG);
       ++system.sleep_timer;  // and increment sleep timer
    }
};

inline void system_semitick(void) {};

void system_idle_loop(void);
void system_sleep_loop(void);

uint8_t system_power(void);
uint8_t system_onbutton(void);

#endif

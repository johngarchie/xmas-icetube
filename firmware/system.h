#ifndef SYSTEM_H
#define SYSTEM_H


#include <stdint.h>  // for using standard integer types

#include "config.h"  // for configuration macros


// time to wait after entering sleep mode before checking battery.
// system voltage takes about 3 minutes to drop to battery level
// so 600 seconds (10 minutes) is playing it safe
#define SYSTEM_BATTERY_CHECK_DELAY 600  // seconds


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

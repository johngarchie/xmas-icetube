#ifndef SYSTEM_H
#define SYSTEM_H


#include <stdint.h>  // for using standard integer types

#include "config.h"  // for configuration macros


// return codes for the system_power() function
enum {
    SYSTEM_ADAPTOR,
    SYSTEM_BATTERY,
};


// flags for system.status
#define SYSTEM_SLEEP          0x01
#define SYSTEM_ALARM_SOUNDING 0x02


typedef struct {
    uint8_t status;
    uint8_t initial_mcusr;
} system_t;


extern volatile system_t system;


void system_init(void);

inline void system_wake(void) {};
inline void system_sleep(void) {};

inline void system_tick(void) {};
inline void system_semitick(void) {};

void system_idle_loop(void);
void system_sleep_loop(void);

uint8_t system_power(void);

#endif

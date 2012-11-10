#ifndef POWER_H
#define POWER_H

#include <stdint.h>  // for using standard integer types

// return codes for the power_source() function
enum {
    POWER_ADAPTOR,
    POWER_BATTERY,
};

// flags for power.status
#define POWER_SLEEP   0x01
#define POWER_ALARMON 0x02

typedef struct {
    uint8_t status;
    uint8_t initial_mcusr;
} power_t;

extern volatile power_t power;

void power_init(void);

void power_idle_loop(void);
void power_sleep_loop(void);

uint8_t power_source(void);

#endif

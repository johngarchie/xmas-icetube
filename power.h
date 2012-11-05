#ifndef POWER_H
#define POWER_H

#include <stdint.h>

enum {
    POWER_ADAPTOR,
    POWER_BATTERY,
};

enum {
    POWER_SLEEP,
    POWER_WAKE,
};

typedef struct {
    uint8_t status;
} power_t;

extern volatile power_t power;

void power_init(void);

void power_idle_loop(void);
void power_sleep_loop(void);

uint8_t power_source(void);

#endif

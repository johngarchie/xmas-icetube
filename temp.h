#include "config.h"

#ifdef TEMPERATURE_SENSOR

#ifndef TEMP_H
#define TEMP_H

#include "stdio.h"

typedef struct {
    int16_t temp;
} temp_t;


extern volatile temp_t temp;

inline void temp_init(void) {};
inline void temp_wake(void) {};
inline void temp_sleep(void) {};

void temp_tick();
inline void temp_semitick() {};

int16_t temp_degF(void);
int16_t temp_degC(void);

#endif  // TEMP_H

#endif  // TEMPERATURE_SENSOR

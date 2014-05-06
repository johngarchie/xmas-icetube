#ifndef TEMP_H
#define TEMP_H

#include "config.h"

#ifdef TEMPERATURE_SENSOR

#include <stdint.h>  // for INT16_MAX macro

// frequency of temperature conversions (seconds)
#define TEMP_CONV_INTERVAL 10

// constant value for invalid temperature
#define TEMP_INVALID  INT16_MAX

#define TEMP_CONV_STARTED 0x01  // temperature conversion started
#define TEMP_CONV_INVALID 0x02  // temperature conversion invalid
#define TEMP_COMM_LOCK    0x04  // communicating with temp sensor

typedef struct {
     int8_t status;      // status flags
    int32_t int_timer;   // seconds between temperature readings
    int32_t conv_timer;  // seconds until next temperature reading
    uint8_t adjust;      // necessary 1/128 second adjustments
    int32_t error;       // time error [1 / (16^2 * 10^9) seconds]
    int16_t temp;        // current temperature (16 * deg C)
     int8_t missed_ovf;  // missed timer0 overflows
} temp_t;


extern volatile temp_t temp;

void temp_init(void);
inline void temp_wake(void) {};
void temp_sleep(void);

void temp_tick(void);
inline void temp_semitick(void) {};

int16_t temp_degF(void);
int16_t temp_degC(void);

#else  // TEMPERATURE_SENSOR

inline void temp_init(void)     {};
inline void temp_wake(void)     {};
inline void temp_sleep(void)    {};
inline void temp_tick(void)     {};
inline void temp_semitick(void) {};

#endif  // TEMPERATURE_SENSOR

#endif  // TEMP_H

#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>        // for using standard integer types
#include <avr/pgmspace.h>  // for accessing data in program memory


#define DISPLAY_SIZE 9


typedef struct {
    uint8_t  buffer[DISPLAY_SIZE];  // display contents
    int8_t  bright_min;             // minimum display brightness
    int8_t  bright_max;             // maximum display brightness

    // photoresistor adc result (times 2^6, running average)
    uint16_t photo_avg;

    // length of time to display each digit (32 microsecond units)
    uint8_t digit_times[DISPLAY_SIZE];

    uint8_t digit_time_shift;
} display_t;

volatile extern display_t display;


void display_init(void);
void display_wake(void);
void display_sleep(void);

inline void display_tick(void) {};
uint8_t display_varsemitick(void);
void display_semitick(void);

void display_loadbright(void);
void display_savebright(void);

void display_loaddigittimes(void);
void display_savedigittimes(void);

void display_noflicker(void);

void display_autodim(void);

void display_pstr(const uint8_t idx, PGM_P pstr);
void display_digit(uint8_t idx, uint8_t n);
void display_char(uint8_t idx, char c);
void display_clear(uint8_t idx);

void display_dotselect(uint8_t idx_start, uint8_t idx_end);
void display_dot(uint8_t idx, uint8_t show);
void display_dash(uint8_t idx, uint8_t show);

#endif

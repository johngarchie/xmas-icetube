#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

#define DISPLAY_SIZE 9

typedef struct {
    volatile uint8_t buffer[DISPLAY_SIZE];  // display contents
    volatile uint8_t brightness; // display brightness
} display_t;

volatile extern display_t display;

void display_init(void);
void display_sleep(void);
void display_wake(void);

inline void display_tick(void) {};
void display_semitick(void);

void display_setbright(uint8_t value);
void display_savebright(void);
void display_loadbright(void);

void display_string(const char str[]);
void display_digit(uint8_t idx, uint8_t n);
void display_char(uint8_t idx, char c);
void display_clear(uint8_t idx);

void display_dot (uint8_t idx, uint8_t show);
void display_dash(uint8_t idx, uint8_t show);

#endif

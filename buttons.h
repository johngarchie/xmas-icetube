#ifndef BUTTONS_H
#define BUTTONS_H


#include <stdint.h>  // for using standard integer types

#include "config.h"  // for configuration macros


// debounce and repeat settings
#define BUTTONS_DEBOUNCE_TIME   20
#define BUTTONS_REPEAT_AFTER  1000
#define BUTTONS_REPEAT_RATE    100

// button flags (for button.state and button.pressed)
#define BUTTONS_MENU      0x01
#define BUTTONS_SET       0x02
#define BUTTONS_PLUS      0x04

// state flags (for button.state)
#define BUTTONS_PROCESSED 0x10
#define BUTTONS_REPEATING 0x20


typedef struct {
    uint8_t  state;  // sensed buttons and state
    uint8_t pressed; // debounced button presses
} buttons_t;


extern volatile buttons_t buttons;


void buttons_init(void);
void buttons_sleep(void);
void buttons_wake(void);

inline void buttons_tick(void) {};
void buttons_semitick(void);

uint8_t buttons_process(void);

#endif

#ifndef BUTTON_H
#define BUTTON_H

#include <avr/pgmspace.h>

// debounce and repeat settings
#define BUTTON_DEBOUNCE_TIME   20
#define BUTTON_REPEAT_AFTER  1000
#define BUTTON_REPEAT_RATE    200

// button flags (for button.state and button.pressed)
#define BUTTON_MENU      0x01
#define BUTTON_SET       0x02
#define BUTTON_PLUS      0x04

// state flags (for button.state)
#define BUTTON_PROCESSED 0x10
#define BUTTON_REPEATING 0x20

typedef struct {
    volatile uint8_t  state;  // sensed buttons and state
    volatile uint8_t pressed; // debounced button presses
} button_t;

extern volatile button_t button;

void button_init(void);
void button_sleep(void);
void button_wake(void);

inline void button_tick(void) {};
void button_semitick(void);

uint8_t button_process(void);

#endif

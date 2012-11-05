#ifndef ALARM_H
#define ALARM_H

#include <avr/pgmspace.h>

// defalut alarm time is 10:00am
#define ALARM_DEFAULT_HOUR   10
#define ALARM_DEFAULT_MINUTE  0

// default alarm volume on lowest setting [0, 10]
#define ALARM_DEFAULT_VOLUME 0


#define ALARM_SET      0x01
#define ALARM_SOUNDING 0x02
#define ALARM_SNOOZE   0x04

#define ALARM_BEEP     0x10
#define ALARM_CLICK    0x20

// duration in semiseconds of the sound made by alarm_click()
#define ALARM_CLICKTIME 5

// turn alarm off after five minutes
#define ALARM_SOUNDING_TIMEOUT 300

// snooze time (seconds)
#define ALARM_SNOOZE_TIMEOUT 150

// debounce time for alarm switch (semiticks)
#define ALARM_DEBOUNCE_TIME 40


typedef struct {
    volatile uint8_t  status;  // off, set, or sounding
    volatile uint8_t  volume;
    volatile uint8_t  hour;
    volatile uint8_t  minute;
    volatile uint16_t alarm_timer;
    volatile uint16_t buzzer_timer;
} alarm_t;

extern volatile alarm_t alarm;

void alarm_init(void);

void alarm_sleep(void);
void alarm_wake(void);

void alarm_settime(uint8_t hour, uint8_t minute);
void alarm_load(void);

void alarm_savevolume(void);

void alarm_tick(void);
void alarm_semitick(void);

void alarm_click(void);
void alarm_beep(uint16_t);

void alarm_buzzeron(void);
void alarm_buzzeroff(void);

#endif

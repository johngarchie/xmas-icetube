#ifndef ALARM_H
#define ALARM_H

#include <stdint.h>  // for using standard integer types


// alarm triggers at 10:00 am
#define ALARM_DEFAULT_HOUR   10  // (hours past midnight)
#define ALARM_DEFAULT_MINUTE  0  // (minutes past midnight)

// alarm volume ramps from min to max during alarm
#define ALARM_DEFAULT_VOLUME_MIN 3  // range [0, 10]
#define ALARM_DEFAULT_VOLUME_MAX 7  // range [0, 10]

// during alarm, this value gives the amount of time
// required to move from the minimum to maximum volume
#define ALARM_DEFAULT_RAMP_TIME 1  // minutes

// snooze time (minutes), range is [0, 30], 0=off
#define ALARM_DEFAULT_SNOOZE_TIME 9  // minutes


// turn alarm off after five minutes
#define ALARM_SOUNDING_TIMEOUT 300  // seconds

// duration in semiseconds of the sound made by alarm_click()
#define ALARM_CLICKTIME 8  // semiseconds

// debounce time for alarm switch
#define ALARM_DEBOUNCE_TIME 40  // semiseconds


// flags for alarm.status
#define ALARM_SET      0x01
#define ALARM_SOUNDING 0x02
#define ALARM_SNOOZE   0x04

#define ALARM_BEEP     0x10
#define ALARM_CLICK    0x20


typedef struct {
    uint8_t  status;       // status flags
    uint8_t  volume;       // current alarm sound volume
    uint8_t  volume_min;   // minimum sound volume of buzzer
    uint8_t  volume_max;   // maximum sound volume of buzzer
    uint8_t  hour;         // hour of alarm trigger
    uint8_t  minute;       // minute of alarm trigger
    uint16_t snooze_time;  // duration of snooze in seconds
    uint16_t alarm_timer;  // timer for alarm and snooze
    uint16_t buzzer_timer; // timer for a beep, see clock_beep()
    uint8_t  ramp_time;    // ramp time for progressive alarm (minutes)
    uint16_t ramp_int;     // ramp interval for progressive alarm (seconds)
} alarm_t;


extern volatile alarm_t alarm;


void alarm_init(void);

void alarm_wake(void);
void alarm_sleep(void);

void alarm_tick(void);
void alarm_semitick(void);

void alarm_savevolume(void);
void alarm_savesnooze(void);

void alarm_saveramp(void);
void alarm_newramp(void);

void alarm_settime(uint8_t hour, uint8_t minute);

void alarm_click(void);
void alarm_beep(uint16_t);

void alarm_buzzeron(void);
void alarm_buzzeroff(void);

#endif

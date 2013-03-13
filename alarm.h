#ifndef ALARM_H
#define ALARM_H

#include <stdint.h>  // for using standard integer types

#include "config.h"  // for configuration macros
#include "time.h"    // for day-of-week macros


// number of alarms to set
#define ALARM_COUNT 3

// alarm triggers at 10:00 am
#define ALARM_DEFAULT_HOUR   10  // (hours past midnight)
#define ALARM_DEFAULT_MINUTE  0  // (minutes past midnight)
#define ALARM_DEFAULT_DAYS    0  // alarm off

// snooze time (minutes), range is [0, 30], 0=off
#define ALARM_DEFAULT_SNOOZE_TIME 9  // minutes

// alarm volume ramps from min to max during alarm
#define ALARM_DEFAULT_VOLUME_MIN 0  // range [0, 10]
#define ALARM_DEFAULT_VOLUME_MAX 10 // range [0, 10]

// during alarm, this value gives the amount of time
// required to move from the minimum to maximum volume
#define ALARM_DEFAULT_RAMP_TIME 1  // minutes

// turn alarm off after five minutes
#define ALARM_SOUNDING_TIMEOUT 300  // seconds

// debounce time for alarm switch
#define ALARM_DEBOUNCE_TIME 40  // semiseconds

// maximum allowed time difference for alarm_nearalarm()
#define ALARM_NEAR_THRESHOLD 2  // seconds


// flags for alarm.status
#define ALARM_SET      0x01
#define ALARM_SOUNDING 0x02
#define ALARM_SNOOZE   0x04

#define ALARM_SOUNDING_PULSE 0x10
#define ALARM_SNOOZING_PULSE 0x20

#define ALARM_SETTINGS_MASK 0xF0

// enabled flag for alarm.days[i]
#define ALARM_ENABLED 0x80


typedef struct {
    uint8_t  status;       // status flags
    uint16_t snooze_time;  // duration of snooze in seconds
    uint16_t alarm_timer;  // time in current state (sounding or snooze)

    uint8_t  hours[ALARM_COUNT];    // hour for alarms
    uint8_t  minutes[ALARM_COUNT];  // minute for alarms
    uint8_t  days[ALARM_COUNT];     // day-of-week for alarms

    uint8_t  volume;       // current progressive alarm volume
    uint8_t  volume_min;   // minimum sound volume of buzzer
    uint8_t  volume_max;   // maximum sound volume of buzzer
    uint8_t  ramp_time;    // ramp time for progressive alarm (minutes)
    uint16_t ramp_int;     // ramp interval for progressive alarm (seconds)
} alarm_t;


extern volatile alarm_t alarm;


// function declarations
void alarm_init(void);

void alarm_wake(void);
void alarm_sleep(void);

void alarm_tick(void);
void alarm_semitick(void);

void alarm_savealarm(uint8_t idx);
void alarm_loadalarm(uint8_t idx);
void alarm_savevolume(void);
void alarm_saveramp(void);
void alarm_newramp(void);
void alarm_savesnooze(void);
void alarm_savestatus(void);

uint8_t alarm_onbutton(void);

uint8_t alarm_nearalarm(void);

#endif

#ifndef ALARM_H
#define ALARM_H

#include <stdint.h>  // for using standard integer types


// alarm triggers at 10:00 am
#define ALARM_DEFAULT_HOUR   10  // (hours past midnight)
#define ALARM_DEFAULT_MINUTE  0  // (minutes past midnight)

// snooze time (minutes), range is [0, 30], 0=off
#define ALARM_DEFAULT_SNOOZE_TIME 9  // minutes

// alarm volume ramps from min to max during alarm
#define ALARM_DEFAULT_VOLUME_MIN 0  // range [0, 10]
#define ALARM_DEFAULT_VOLUME_MAX 5  // range [0, 10]

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


typedef struct {
    uint8_t  status;       // status flags
    uint8_t  hour;         // hour of alarm trigger
    uint8_t  minute;       // minute of alarm trigger
    uint16_t snooze_time;  // duration of snooze in seconds
    uint16_t alarm_timer;  // time in current state (sounding or snooze)

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

void alarm_settime(uint8_t hour, uint8_t minute);
void alarm_savevolume(void);
void alarm_saveramp(void);
void alarm_newramp(void);
void alarm_savesnooze(void);

uint8_t alarm_onbutton(void);

uint8_t alarm_nearalarm(void);

#endif

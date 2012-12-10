#ifndef MODE_H
#define MODE_H

#include <stdint.h>  // for using standard integer types


// default menu timeout; on timeout, mode changes to time display
#define MODE_TIMEOUT 10000  // semiticks (~milliseconds)


// various clock modes; current mode given by mode.state
enum {
    MODE_TIME_DISPLAY,
        MODE_DAYOFWEEK_DISPLAY,
        MODE_MONTHDAY_DISPLAY,
        MODE_ALARMSET_DISPLAY,
        MODE_ALARMTIME_DISPLAY,
        MODE_ALARMOFF_DISPLAY,
        MODE_SNOOZEON_DISPLAY,
    MODE_MENU_SETALARM,
        MODE_SETALARM_HOUR,
        MODE_SETALARM_MINUTE,
    MODE_MENU_SETTIME,
        MODE_SETTIME_HOUR,
        MODE_SETTIME_MINUTE,
        MODE_SETTIME_SECOND,
    MODE_MENU_SETDATE,
        MODE_SETDATE_YEAR,
        MODE_SETDATE_MONTH,
        MODE_SETDATE_DAY,
    MODE_MENU_SETDST,
        MODE_SETDST_STATE,
        MODE_SETDST_ZONE,
    MODE_MENU_SETSOUND,
	MODE_SETSOUND_TYPE,
	MODE_SETSOUND_VOL,
	MODE_SETSOUND_VOL_MIN,
	MODE_SETSOUND_VOL_MAX,
	MODE_SETSOUND_TIME,
    MODE_MENU_SETBRIGHT,
	MODE_SETBRIGHT_LEVEL,
	MODE_SETBRIGHT_MIN,
	MODE_SETBRIGHT_MAX,
    MODE_MENU_SETDIGITBRIGHT,
	MODE_SETDIGITBRIGHT,
    MODE_MENU_SETSNOOZE,
	MODE_SETSNOOZE_TIME,
    MODE_MENU_SETFORMAT,
        MODE_SETTIME_FORMAT,
        MODE_SETDATE_FORMAT,
};


#define MODE_TMP_YEAR  0
#define MODE_TMP_MONTH 1
#define MODE_TMP_DAY   2

#define MODE_TMP_HOUR   0
#define MODE_TMP_MINUTE 1
#define MODE_TMP_SECOND 2

#define MODE_TMP_VOL      0
#define MODE_TMP_VOL_MIN  1
#define MODE_TMP_VOL_MAX  2

#define MODE_TMP_SET 0
#define MODE_TMP_MIN 1
#define MODE_TMP_MAX 2


typedef struct {
    uint8_t  state;  // name of current state
    uint16_t timer;  // time in current state (semiseconds)
    int8_t  tmp[3];  // place to store temporary data
} mode_t;


extern volatile mode_t mode;


void mode_init(void);

void mode_tick(void);
void mode_semitick(void);

void mode_wake(void);
inline void mode_sleep(void) {};

void mode_alarmset(void);
void mode_alarmoff(void);
void mode_snoozing(void);

#endif

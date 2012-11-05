#ifndef MODE_H
#define MODE_H

#include <stdint.h>

// default timeout for menus in semiticks (~milliseconds)
#define MODE_TIMEOUT 10000


enum {
    MODE_TIME,
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
    MODE_MENU_SETBRIGHT,
         MODE_SETBRIGHT_LEVEL,
    MODE_MENU_SETVOLUME,
         MODE_SETVOLUME_LEVEL,
};


#define MODE_TMP_YEAR   0
#define MODE_TMP_MONTH  1
#define MODE_TMP_DAY    2
#define MODE_TMP_HOUR   0
#define MODE_TMP_MINUTE 1
#define MODE_TMP_SECOND 2
#define MODE_TMP_BRIGHT 0
#define MODE_TMP_VOLUME 0


typedef struct {
    uint8_t  state;  // name of current state
    uint16_t timer;  // time in current state (semiseconds)
    uint8_t  tmp[3]; // place to store temporary data
} mode_t;

extern volatile mode_t mode;

void mode_init(void);

void mode_tick(void);
void mode_semitick(void);

inline void mode_wake(void) {};
inline void mode_sleep(void) {};

#endif

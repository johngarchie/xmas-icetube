#ifndef TIME_H
#define TIME_H

#include <stdint.h>       // for using standard integer types
#include <avr/pgmspace.h> // for PGM_P (pointer to program memory)

#include "config.h"  // for configuration macros


// month numbers
#define TIME_JAN 1
#define TIME_FEB 2
#define TIME_MAR 3
#define TIME_APR 4
#define TIME_MAY 5
#define TIME_JUN 6
#define TIME_JUL 7
#define TIME_AUG 8
#define TIME_SEP 9
#define TIME_OCT 10
#define TIME_NOV 11
#define TIME_DEC 12

// day-of-week numbers
#define TIME_SUN   0
#define TIME_MON   1
#define TIME_TUE   2
#define TIME_WED   3
#define TIME_THU   4
#define TIME_FRI   5
#define TIME_SAT   6
#define TIME_NODAY 7

#define TIME_NODAYS 0

#define TIME_ALLDAYS   _BV(TIME_SUN) | _BV(TIME_MON) | _BV(TIME_TUE) | _BV(TIME_WED) | _BV(TIME_THU) | _BV(TIME_FRI) | _BV(TIME_SAT)

#define TIME_WEEKDAYS   _BV(TIME_MON) | _BV(TIME_TUE) | _BV(TIME_WED) | _BV(TIME_THU) | _BV(TIME_FRI)

#define TIME_WEEKENDS _BV(TIME_SAT) | _BV(TIME_SUN)

// return states for time_isdst_usa()
#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif

// drift correction table size
#define TIME_DRIFT_TABLE_SIZE 7    // number of estimated drift corrections
#define TIME_MIN_DRIFT_ADJUST 39   // drift less than ~200 ppm
#define TIME_MAX_DRIFT_TIME   1200 // seconds (20 min)
#define TIME_MIN_DRIFT_TIME   15   // seconds
#define TIME_DRIFT_SAVE_DELAY 600  // seconds (10 min)

// flags for time.status
#define TIME_UNSET       0x01
#define TIME_DST         0x02

// top nibble indicates DST
#define TIME_AUTODST_MASK   0xF0
#define TIME_AUTODST_NONE   0x00
#define TIME_AUTODST_EU_GMT 0x10
#define TIME_AUTODST_EU_CET 0x20
#define TIME_AUTODST_EU_EET 0x30
#define TIME_AUTODST_USA    0x40


// date format flags for date.dateformat
#define TIME_DATEFORMAT_SHOWWDAY  0x80
#define TIME_DATEFORMAT_SHOWYEAR  0x40
#define TIME_DATEFORMAT_SHOWTEMP  0x20

#define TIME_DATEFORMAT_MASK 0x0F

enum {
    TIME_DATEFORMAT_DOTNUM_ISO,   // YYYY.MM.DD  (e.g. "2010.01.30")
    TIME_DATEFORMAT_DOTNUM_EU,    // DD.MM.YYYY  (e.g. "30.01.2010")
    TIME_DATEFORMAT_DOTNUM_USA,   // MM.DD.YYYY  (e.g. "01.30.2011")
    TIME_DATEFORMAT_DASHNUM_EU,   // DD-MM-YYYY  (e.g. "30-01-2011")
    TIME_DATEFORMAT_DASHNUM_USA,  // MM-DD-YYYY  (e.g. "01-30-2011")
    TIME_DATEFORMAT_TEXT_EU,      // Mmm DD      (e.g. " Jan 30 ")
    TIME_DATEFORMAT_TEXT_USA,     // DD Mmm      (e.g. " 30 Jan ")
};


// time format flags for time.timeformat
#define TIME_TIMEFORMAT_12HOUR   0x80
#define TIME_TIMEFORMAT_SHOWAMPM 0x40
#define TIME_TIMEFORMAT_SHOWDST  0x20
#ifdef GPS_TIMEKEEPING
#define TIME_TIMEFORMAT_SHOWGPS   0x10
#endif  // GPS_TIMEKEEPING

#define TIME_TIMEFORMAT_MASK 0x0F

enum {
    TIME_TIMEFORMAT_HH_MM_SS,
    TIME_TIMEFORMAT_HH_MM_dial,
    TIME_TIMEFORMAT_HH_MM,
    TIME_TIMEFORMAT_HH_MM_PM,
    TIME_TIMEFORMAT_HHMMSSPM,
};


typedef struct {
    uint8_t status;  // status flags
    uint8_t dateformat;  // date format
    uint8_t timeformat;  // time format

    uint8_t year;    // years past 2000 (0 during year 2000)
    uint8_t month;   // month (1 during january)
    uint8_t day;     // day of month (1 on the first)
    uint8_t hour;    // hours past midnight (0 at midnight)
    uint8_t minute;  // minutes past hour   (0 at midnight)
    uint8_t second;  // seconds past minute (0 at midnight)

    int16_t drift_adjust; // current drift adjustment; abs(drift_adjust) is
    // the number of seconds that pass before time should be adjusted by 1/128
    // seconds; positive values indicate the clock is fast; negative values,
    // slow; 0 indicates no adjustment need be made

    uint16_t drift_adjust_timer; // incremented every second; when
    // drift_adjust_timer equals abs(drift_adjust), time is adjusted by
    // 1/128 seconds and the timer is reset to zero

    int32_t drift_delta_seconds; // when clock is set, the difference between
    // the old and new time accumulates here; drift_delta_seconds is reset
    // to zero when drift_total_seconds is reset

    int32_t drift_total_seconds;  // total number of seconds between clock
    // sets, so if no drift adjustment is currently being made, drift would be
    // [drift (ppm)] = 1000000 * [drift_delta_seconds] / [drift_delta_timer]

    uint16_t drift_delay_timer;  // seconds until system computes a new
    // drift adjustment using drift_delta_seconds and drift_total_seconds

    uint8_t drift_frac_seconds;  // monitors fractional seconds from time sets
} time_t;


extern volatile time_t time;


void time_init(void);

void time_wake(void);
void time_sleep(void);

void time_tick(void);
inline void time_semitick(void) {};

void time_savetime(void);
void time_savedate(void);

void time_savestatus(void);
void time_loadstatus(void);

void time_savedateformat(void);
void time_loaddateformat(void);

void time_savetimeformat(void);
void time_loadtimeformat(void);

void time_settime(const uint8_t hour, const uint8_t minute, const uint8_t second);
void time_setdate(uint8_t year, uint8_t month, uint8_t day);

uint8_t time_dayofweek(uint8_t year, uint8_t month, uint8_t day);
uint8_t time_daysinmonth(uint8_t year, uint8_t month);

PGM_P time_wday2pstr(uint8_t wday);
PGM_P time_month2pstr(uint8_t month);

void time_autodst(uint8_t);
void time_dston(uint8_t adj_time);
void time_dstoff(uint8_t adj_time);
void time_springforward(void);
void time_fallback(void);
uint8_t time_isdst_eu(int8_t rel_gmt);
uint8_t time_isdst_usa(void);

void time_autodrift(void);
void time_newdrift(void);
void time_loaddriftmedian(void);

#endif

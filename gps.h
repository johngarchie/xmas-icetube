#ifndef GPS_H
#define GPS_H

#include <stdint.h>  // for standard integer types

#include "config.h"  // for configuration macros

#ifdef GPS_TIMEKEEPING

// various flags for gps.status
#define GPS_INVALID_RMC        0x01
#define GPS_PARSED_TIME        0x02
#define GPS_PARSED_STATUS_CODE 0x04
#define GPS_PARSED_DATE        0x08
#define GPS_PARSED_CHECKSUM    0x10
#define GPS_INVALID_CHECKSUM   0x20
#define GPS_SIGNAL_GOOD        0x40

// standard definitions for TRUE and FALSE
#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif


// timeout values for gps.data_timer and gps.warn_timer
#define GPS_DATA_TIMEOUT  15  // (seconds)
#define GPS_WARN_TIMEOUT 180  // (seconds)

// the maximum and minimum hour offset from utc/gmt
#define GPS_HOUR_OFFSET_MIN -12
#define GPS_HOUR_OFFSET_MAX  14


typedef struct {
    uint8_t status;    // rmc parse status flags
    uint8_t checksum;  // rmc checksum
    uint8_t field;     // current rmc field
    uint8_t idx;       // character index within current field

    // data parsed from rmc line; time from gps is utc/gmt
    int8_t  hour;
    int8_t  minute;
    int8_t  second;
    int8_t  day;
    int8_t  month;
    int8_t  year;
    char    status_code;  // 'A' for active; 'V' for warning

    // local time offset relative to gmt/utc
    int8_t rel_utc_hour;
    int8_t rel_utc_minute;

    // gps data-received timers to determine if gps present with good signal
    uint8_t data_timer;  // nonzero if gps data is being received
    uint8_t warn_timer;  // nonzero if gps has signal (status_code == 'A')
} gps_t;


extern volatile gps_t gps;


void gps_init(void);

void gps_wake(void);
void gps_sleep(void);

void gps_tick(void);
inline void gps_semitick(void) {};

void gps_loadrelutc(void);
void gps_saverelutc(void);

void gps_settime(void);

#else  // GPS_TIMEKEEPING

inline void gps_init(void) {};

inline void gps_wake(void) {};
inline void gps_sleep(void) {};

inline void gps_tick(void) {};
inline void gps_semitick(void) {};

#endif  // GPS_TIMEKEEPING

#endif  // GPS_H

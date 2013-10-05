// gps.c  --  parses gps output from usart and sets time accordingly
//
//


#include "config.h"
#ifdef GPS_TIMEKEEPING

#include <avr/io.h>         // for using avr register names
#include <avr/eeprom.h>     // for accessing data in eeprom memory
#include <avr/interrupt.h>  // for defining usart rx interrupt
#include <util/atomic.h>    // for non-interruptable blocks

#include "gps.h"
#include "time.h"
#include "usart.h"
#include "alarm.h"


#define FIELD_RECORD_START                  0
#define FIELD_RMC_CODE                      1
#define FIELD_UTC_TIME                      2
#define FIELD_STATUS_CODE                   3
#define FIELD_LATITUDE_VALUE                4
#define FIELD_LATITUDE_DIRECTION            5
#define FIELD_LONGITUDE_VALUE               6
#define FIELD_LONGITUDE_DIRECTION           7
#define FIELD_TRAVEL_SPEED                  8
#define FIELD_TRAVEL_DIRECTION              9
#define FIELD_UTC_DATE                     10
#define FIELD_MAGNETIC_VARIATION_VALUE     11
#define FIELD_MAGNETIC_VARIATION_DIRECTION 12
#define FIELD_FAA_MODE_INDICATOR           13
#define FIELD_CHECKSUM                     14
#define FIELD_NEWLINE                      15


// extern'ed gps data
volatile gps_t gps;


// time offsets relative to gmt/utc 
uint8_t ee_gps_rel_utc_hour   EEMEM = 0;
uint8_t ee_gps_rel_utc_minute EEMEM = 0;


// load time offsets from gmt/utc
void gps_init(void) {
    gps_loadrelutc();
}


// enable interrupt on received data; called *after* usart_wake()
void gps_wake(void) {
    // reset parser
    gps.status  = 0;

    // enable usart rx interrupt
    UCSR0B |= _BV(RXCIE0);

    // give gps time to acquire signal before issuing "gps lost" warning
    gps.warn_timer = GPS_WARN_TIMEOUT;

    // gps needs to reacquire satellites
    gps.status &= ~GPS_SIGNAL_GOOD;
}


// disable interrupt on received data; called *before* usart_sleep()
void gps_sleep(void) {
    // disable usart rx interrupt
    UCSR0B &= ~_BV(RXCIE0);
}


// decrement gps timers
void gps_tick(void) {
    if(gps.data_timer) {
	--gps.data_timer;
    } else {
	gps.status &= ~GPS_SIGNAL_GOOD;
    }

    if(gps.warn_timer) --gps.warn_timer;
}


// load time offsets from gmt/utc from eeprom
void gps_loadrelutc(void) {
    gps.rel_utc_hour   = eeprom_read_byte(&ee_gps_rel_utc_hour  );
    gps.rel_utc_minute = eeprom_read_byte(&ee_gps_rel_utc_minute);

    if(gps.rel_utc_hour < GPS_HOUR_OFFSET_MIN
	    || gps.rel_utc_hour > GPS_HOUR_OFFSET_MAX) {
	gps.rel_utc_hour   = 0;
	gps.rel_utc_minute = 0;
    } else {
	gps.rel_utc_minute %= 60;
    }
}


// save time offsets from gmt/utc from eeprom
void gps_saverelutc(void) {
    eeprom_write_byte(&ee_gps_rel_utc_hour,   gps.rel_utc_hour  );
    eeprom_write_byte(&ee_gps_rel_utc_minute, gps.rel_utc_minute);
}


// set clock time from rmc parse (assumes successful parse)
void gps_settime(void) {
    // only set time once per rmc sentence
    gps.status &= GPS_SIGNAL_GOOD;

    gps.data_timer = GPS_DATA_TIMEOUT;

    if(gps.status_code == 'A') {
	gps.warn_timer = GPS_WARN_TIMEOUT;

	if(!(gps.status & GPS_SIGNAL_GOOD)) {
	    // don't set time on first good gps signal;
	    // on the adafruit ultimate gps, it is garbage
	    gps.status |= GPS_SIGNAL_GOOD;
	    return;
	}

	// never set time when new time could skip alarm time
	if(alarm_nearalarm()) return;

	// first, convert time to local time
	int8_t second = gps.second;
	int8_t minute = gps.minute + gps.rel_utc_minute;
	int8_t hour   = gps.hour   + gps.rel_utc_hour;
	int8_t day    = gps.day;
	int8_t month  = gps.month;
	int8_t year   = gps.year;

	if(minute < 0) {
	    minute += 60;
	    --hour;
	} else if(minute >= 60) {
	    minute -= 60;
	    ++hour;
	}

	if(time.status & TIME_DST) ++hour;

	if(hour < 0) {
	    hour += 24;
	    --day;
	} else if(hour >= 24) {
	    hour -= 24;
	    ++day;
	}

	if(day < 1) {
	    --month;
	    if(month < TIME_JAN) {
		month = TIME_DEC;
		--year;
	    }
	    day = time_daysinmonth(year, month);
	} else if(day > time_daysinmonth(year, month)) {
	    day = 1;
	    ++month;
	    if(month > TIME_DEC) {
		month = TIME_JAN;
		++year;
	    }
	}

	// calculate difference between gps time and clock time
	int8_t delta_hour   = hour   - time.hour;
	int8_t delta_minute = minute - time.minute;
	int8_t delta_second = second - time.second;

	int32_t time_diff = delta_hour;
	time_diff *= 60;  // hours to minutes
	time_diff += delta_minute;
	time_diff *= 60;  // minutes to seconds
	time_diff += delta_second;

	// correct for times spanning midnight
	if(time_diff > (int32_t)12 * 60 * 60) {
	    time_diff -= (int32_t)24 * 60 * 60;
	} else if(time_diff < (int32_t)-12 * 60 * 60) {
	    time_diff += (int32_t)24 * 60 * 60;
	}

	// if time has drifted by more than one second...
	if(time_diff < -1 || 1 < time_diff) {
	    // set time from gps
	    time_settime(hour, minute, second);

	    // ensure date is correct for new time
	    if(year != time.year || month != time.month
		    || day != time.day) {
		time_setdate(year, month, day);
	    }
	}

	// if date is incorrect and time is not near midnight
	if((day != time.day || year != time.year || month != time.month)
		&& (hour != 23 || minute != 59 || second != 59)
		&& (hour !=  0 || minute != 0
		    || (second != 0 && second != 1))) {
	    // set date from gps
	    time_setdate(year, month, day);
	}
    } else {
	gps.status &= ~GPS_SIGNAL_GOOD;
    }
}


// parse character from gps
ISR(USART_RX_vect) {
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
	char c = UDR0;

	// reset rmc parser on carriage return
	if(c == '\r') {
	    gps.status   &= GPS_SIGNAL_GOOD;
	    gps.field     = FIELD_NEWLINE;
	    gps.idx       = 1;  // because '\r' has already been processed
	    return;
	}

	// ignore invalid rmc records
	if(gps.status & GPS_INVALID_RMC) return;

	// check for field delimiter (comma)
	if(c == ',') {
	    gps.checksum ^= c;
	    ++gps.field;
	    gps.idx = 0;
	    return;
	}

	switch(gps.field) {
	    case FIELD_RECORD_START:

		if(c != '$' || gps.idx) {
		    gps.status |= GPS_INVALID_RMC;
		}

		gps.field    = FIELD_RMC_CODE;
		gps.idx      = 0;
		gps.checksum = 0;

		return;

	    case FIELD_RMC_CODE:
		gps.checksum ^= c;

		const char *gprmc_str = "GPRMC";
		if(gps.idx >= 5 || c != gprmc_str[gps.idx]) {
		    gps.status |= GPS_INVALID_RMC;
		}
		break;

	    case FIELD_STATUS_CODE:
		gps.checksum ^= c;

		if(gps.idx == 0 && (c == 'A' || c == 'V')) {
		    gps.status_code = c;
		    gps.status |= GPS_PARSED_STATUS_CODE;
		} else {
		    gps.status |= GPS_INVALID_RMC;
		}
		break;

	    case FIELD_UTC_TIME:
		gps.checksum ^= c;

		if(gps.idx != 6 || c != '.') {
		    if('0' <= c && c <= '9') {
			c -= '0';  // convert c to decimal

			switch(gps.idx) {
			    case 0:
				gps.hour = 10 * c;
				break;
			    case 1:
				gps.hour += c;
				break;
			    case 2:
				gps.minute = 10 * c;
				break;
			    case 3:
				gps.minute += c;
				break;
			    case 4:
				gps.second = 10 * c;
				break;
			    case 5:
				gps.second += c;
				break;
			    case 7:
			    case 8:
				break;
			    case 9:
				gps.status |= GPS_PARSED_TIME;
				break;
			    default:
				gps.status |= GPS_INVALID_RMC;
				break;
			}
		    } else {
			gps.status |= GPS_INVALID_RMC;
		    }
		}
		break;

	    case FIELD_UTC_DATE:
		gps.checksum ^= c;

		if('0' <= c && c <= '9') {
		    c -= '0';  // convert c to decimal

		    switch(gps.idx) {
			case 0:
			    gps.day = 10 * c;
			    break;
			case 1:
			    gps.day += c;
			    break;
			case 2:
			    gps.month = 10 * c;
			    break;
			case 3:
			    gps.month += c;
			    break;
			case 4:
			    gps.year = 10 * c;
			    break;
			case 5:
			    gps.year += c;
			    gps.status |= GPS_PARSED_DATE;
			    break;
			default:
			    gps.status |= GPS_INVALID_RMC;
			    break;
		    }
		} else {
		    gps.status |= GPS_INVALID_RMC;
		}
		break;

	    case FIELD_FAA_MODE_INDICATOR:
		if(c == '*') {
		    gps.field = FIELD_CHECKSUM;
		    gps.idx   = 1;
		    return;
		} else {
		    gps.checksum ^= c;
		}
		break;

	    case FIELD_CHECKSUM:
		if(!gps.idx) {
		    if(c == '*') {
			break;
		    } else {
			gps.status |= GPS_INVALID_RMC;
		    }
		    return;
		}


		// convert c to number
		if('0' <= c && c <= '9') {
		    c -= '0';
		} else if('A' <= c && c <= 'F') {
		    c -= 'A';
		    c += 10;
		} else {
		    gps.status |= GPS_INVALID_RMC;
		    return;
		}

		switch(gps.idx) {
		    case 1:
			if( (gps.checksum >> 4) != c ) {
			    gps.status |= GPS_INVALID_CHECKSUM;
			}
			break;
		    case 2:
			if( (gps.checksum & 0x0F) != c ) {
			    gps.status |= GPS_INVALID_CHECKSUM;
			}

			gps.status |= GPS_PARSED_CHECKSUM;

			gps.field = FIELD_NEWLINE;
			gps.idx   = 0;
			break;

		    default:
			gps.status |= GPS_INVALID_RMC;
			break;
		}
		break;

	    case FIELD_NEWLINE:
		if(gps.idx == 1 && c == '\n') {
		    gps.field = FIELD_RECORD_START;
		    gps.idx   = 0;
		    return;
		} else {
		    gps.status |= GPS_INVALID_RMC;
		}
		break;

	    default:
		gps.checksum ^= c;
		break;
	}

	// set time after successful rmc parse
	if((gps.status & (GPS_PARSED_TIME | GPS_PARSED_STATUS_CODE
			| GPS_PARSED_DATE | GPS_PARSED_CHECKSUM))
		&& !(gps.status & (GPS_INVALID_RMC | GPS_INVALID_CHECKSUM))) {
	    gps_settime();
	}

	++gps.idx;
    }
}

#endif  // GPS_TIMEKEEPING

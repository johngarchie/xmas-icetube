#include <avr/io.h>      // for using register names
#include <avr/eeprom.h>  // for storing data in eeprom memory
#include <avr/power.h>   // for enabling/disabling chip features

#include "time.h"
#include "timedef.h"


// extern'ed time and date data
volatile time_t time;

// places to store the current time in EEMEM
#if TIME_DEFAULT_DST == 0
uint8_t ee_time_status EEMEM = 0;
#else
uint8_t ee_time_status EEMEM = TIME_DST;
#endif
uint8_t ee_time_year   EEMEM = TIME_DEFAULT_YEAR;
uint8_t ee_time_month  EEMEM = TIME_DEFAULT_MONTH;
uint8_t ee_time_day    EEMEM = TIME_DEFAULT_MDAY;
uint8_t ee_time_hour   EEMEM = TIME_DEFAULT_HOUR;
uint8_t ee_time_minute EEMEM = TIME_DEFAULT_MINUTE;
uint8_t ee_time_second EEMEM = TIME_DEFAULT_SECOND;


// load time from eeprom, setup counter2 with clock crystal
void time_init(void) {
    // eeprom could be uninitialized or corrupted,
    // so force reasonable values for restored data
    time.year   = eeprom_read_byte(&ee_time_year  ) % 100;
    time.month  = eeprom_read_byte(&ee_time_month ) % 13;
    time.day    = eeprom_read_byte(&ee_time_day   ) % 32;
    time.hour   = eeprom_read_byte(&ee_time_hour  ) % 24;
    time.minute = eeprom_read_byte(&ee_time_minute) % 60;
    time.second = eeprom_read_byte(&ee_time_second) % 60;

    // for month and day, zero is an invalid value
    if(time.month == 0) time.month = 1;
    if(time.day   == 0) time.day   = 1;

    time.status = eeprom_read_byte(&ee_time_status);
    time.status |= TIME_UNSET;

    power_timer2_enable();  // enable counter 2

    // setup counter2 for timekeeping with clock crystal
    ASSR   |= _BV(AS2); // clock counter with crystal oscillator
    TCCR2A  = _BV(WGM21);  // clear counter on compare match
    TCCR2B  = _BV(CS22) | _BV(CS21); // divide clock by 256

    // clock crystal resonates at 32.768 kHz and
    // 32,786 Hz / 256 = 128, so set compare match to
    OCR2A = 127;  // 172 (128 values, including zero)

    // call interrupt on compare match (once per second)
    TIMSK2 = _BV(OCIE2A);
}


// save current time to eeprom
void time_sleep(void) {
    // saving time to eeprom doesn't hurt. if the backup battery is dead, power
    // stored in capacitor should be sufficient to save current time.
    // if the power outage is brief, time will be restored from eeprom and
    // the clock will still have a semi-reasonable time.
    time_savetime();
}


// save time to eeprom
void time_savedate(void) {
    eeprom_write_byte(&ee_time_year,  time.year );
    eeprom_write_byte(&ee_time_month, time.month);
    eeprom_write_byte(&ee_time_day,   time.day  );
}


// save date to eeprom
void time_savetime(void) {
    eeprom_write_byte(&ee_time_hour,   time.hour  );
    eeprom_write_byte(&ee_time_minute, time.minute);
    eeprom_write_byte(&ee_time_second, time.second);
}


// save status to eeprom
void time_savestatus(void) {
    eeprom_write_byte(&ee_time_status, time.status);
}


// set current time
void time_settime(uint8_t hour, uint8_t minute, uint8_t second) {
    time.status &= ~TIME_UNSET;

    time.hour   = hour;
    time.minute = minute;
    time.second = second;
}


// set current date
void time_setdate(uint8_t year, uint8_t month, uint8_t day) {
    time.status &= ~TIME_UNSET;

    time.year   = year;
    time.month  = month;
    time.day    = day;
}


// add one second to current time
void time_tick(void) {
    ++time.second;

    if(time.second >= 60) {
	time.second = 0;
	++time.minute;
	if(time.minute >= 60) {
	    time.minute = 0;
	    ++time.hour;
	    if(time.hour >= 24) {
		time.hour = 0;
		++time.day;
		eeprom_write_byte(&ee_time_day, time.day);
		if(time.day > time_daysinmonth(time.year, time.month)) {
		    time.day = 1;
		    ++time.month;
		    eeprom_write_byte(&ee_time_month, time.month);
		    if(time.month > 12) {
			time.month = 1;
			++time.year;
			eeprom_write_byte(&ee_time_year, time.year);
		    }
		}
	    }
	}
    }

    time_autodst(TRUE);
}


// return the number of days in the current month
uint8_t time_daysinmonth(uint8_t year, uint8_t month) {
    // Thirty days hath September,
    // April, June, and November.
    if(month == TIME_SEP || month == TIME_APR
	    || month == TIME_JUN || month == TIME_NOV) {
	return 30;
    }

    // All the rest have thirty-one,
    // Excepting February alone,
    if(month != TIME_FEB) {
	return 31;
    }

    // And that has twenty-eight days clear,
    // And twenty-nine in each leap year.
    if(year % 4) {
	return 28;
    } else {
	return 29;
    }
}

// returns the day of week for today;
// works for days in years 2000 to 2099
uint8_t time_dayofweek(uint8_t year, uint8_t month, uint8_t day) {
    // first, calculate days since 2000
    uint16_t total_days = 0;

    // days from prior years, minus leap days
    total_days += 365 * year;

    // leap day from year 2000, if prior year
    if(year > 0) ++total_days;

    // leap days from prior years
    total_days += (year - 1) / 4;

    // days in prior months this year
    for(uint8_t i = TIME_JAN; i < month; ++i) {
	total_days += time_daysinmonth(year, i);
    }

    // days in this month
    total_days += day;

    // let 0 be sun, 1 be mon; ...; and 6 be sat.
    // dec 31, 1999 was 5 (fri); so day of week is
    return (5 + total_days) % 7;
}


// if autodst is enabled, set dst accordingly; if adj_time is
// true and the dst state is changed, adjust time accordingly
void time_autodst(uint8_t adj_time) {
    if(time.status & TIME_AUTODST_USA) {
	if(time_isdst_usa()) {
	    time_dston(adj_time);
	} else {
	    time_dstoff(adj_time);
	}
    }
}


// enable dst; if adj_time is true and dst is
// not already enabled, adjust time accordingly
void time_dston(uint8_t adj_time) {
    if(adj_time && !(time.status & TIME_DST)) {
	time_springforward();
    }

    time.status |= TIME_DST;
}


// disable dst; if adj_time is true and dst is
// not already enabled, adjust time accordingly
void time_dstoff(uint8_t adj_time) {
    if(adj_time && time.status & TIME_DST) {
	time_fallback();
    }

    time.status &= ~TIME_DST;
}


// adds one hour from the current time
// (in the spring, clocks "spring forward")
void time_springforward(void) {
    ++time.hour;
    if(time.hour < 24) return;
    time.hour = 0;

    ++time.day;
    if(time.day <= time_daysinmonth(time.year, time.month)) return;
    time.day = 1;

    if(++time.month > 12) {
	time.month = 1;
	++time.year;
    }
}


// subtracts one hour from the current time
// (in the fall, clocks "fall back")
void time_fallback(void) {
    // if time.hour is 0, underflow will make it 255
    --time.hour;

    if(time.hour < 24) return;
    time.hour = 23;

    --time.day;
    if(time.day > 0) return;
    --time.month;

    if(time.month < TIME_JAN) {
	time.month = TIME_DEC;
	--time.year;
    }

    time.day = time_daysinmonth(time.year, time.month);
}


// returns TRUE if currently observing DST, FALSE otherwise
uint8_t time_isdst_usa(void) {
    uint8_t first_day, dst_day;

    switch(time.month) {
	case TIME_MAR:
	    // dst begins on the second sunday in march
	    first_day = time_dayofweek(time.year, time.month, 1);
	    if(first_day == TIME_SUN) {
		dst_day = 8;
	    } else {
		dst_day = 15 - first_day;
	    }

	    // before that day, dst is not in effect;
	    // after that day, dst is in effect
	    if(time.day < dst_day) return FALSE;
	    if(time.day > dst_day) return TRUE;

	    // at 2:00, time jumps forward to 3:00, so
	    // 2:00 to 2:59 is an invalid time range
	    // a time in this range probably means dst should
	    // be enabled, but is not yet, so return true
	    if(time.hour <  2) {
		return FALSE;
	    } else {
		return TRUE;
	    }

	    break;
	case TIME_NOV:
	    // dst ends on the first sunday in november
	    first_day = time_dayofweek(time.year, time.month, 1);
	    if(first_day == TIME_SUN) {
		dst_day = 1;
	    } else {
		dst_day = 8 - first_day;
	    }

	    // before that day, dst is in effect;
	    // after that day, dst is not in effect
	    if(time.day < dst_day) return TRUE;
	    if(time.day > dst_day) return FALSE;

	    // at 2:00, time falls back to 1:00, so
	    // 1:00 to 1:59 is an ambiguous time range
	    if(time.hour <  1) return TRUE;
	    if(time.hour >= 2) return FALSE;

	    // if time is ambiguous, return current dst state
	    if(time.status & TIME_DST) {
		return TRUE;
	    } else {
		return FALSE;
	    }

	    break;
	default:
	    if(TIME_MAR < time.month && time.month < TIME_NOV) {
		// between march and november,
	        // daylight saving time is in effect
		return TRUE;
	    } else {
		// before march and after november,
	        // daylight saving time is not in effect
		return FALSE;
	    }

	    break;
    }
}

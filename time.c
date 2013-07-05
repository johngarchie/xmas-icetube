// time.c  --  maintains current date and time
//
//    timer/counter2    clock timer
//


#include <avr/eeprom.h>   // for storing data in eeprom memory
#include <avr/pgmspace.h> // for accessing data in program memory
#include <avr/power.h>    // for enabling/disabling chip features
#include <util/atomic.h>  // for non-interruptable blocks


#include "time.h"
#include "usart.h"   // for debugging output
#include "system.h"  // for determining power source


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

// places to store the time and date display format
uint8_t ee_time_timeformat EEMEM = 0;
uint8_t ee_time_dateformat EEMEM =   TIME_DATEFORMAT_SHOWWDAY
				   | TIME_DATEFORMAT_SHOWYEAR
				   | TIME_DATEFORMAT_TEXT_EU;

// drift adjustment data
uint8_t ee_time_drift_count EEMEM = 0;
uint8_t ee_time_drift_idx   EEMEM = 0;
int16_t ee_time_drift_table[TIME_DRIFT_TABLE_SIZE] EEMEM;


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

    // explicitly initialize drift variables
    time.drift_adjust_timer  = 0;
    time.drift_delay_timer   = 0;
    time.drift_total_seconds = 0;
    time.drift_delta_seconds = 0;

    // load drift_adjust
    time_loaddriftmedian();

    time_loadstatus();
    time_loaddateformat();
    time_loadtimeformat();

    time.status |= TIME_UNSET;

    power_timer2_enable(); // enable timer2

    // setup timer2 for timekeeping with clock crystal
#ifdef EXTERNAL_CLOCK
    ASSR |= _BV(AS2) | _BV(EXCLK); // clock with external clock
#else
    ASSR |= _BV(AS2); // clock with crystal oscillator
#endif
    TCCR2A  = _BV(WGM21);  // clear counter on compare match
    TCCR2B  = _BV(CS22) | _BV(CS21); // divide clock by 256

    // clock crystal resonates at 32.768 kHz and
    // 32,786 Hz / 256 = 128, so set compare match to
    OCR2A = 127;  // 172 (128 values, including zero)

    // run the once-per-second interrupt when TCNT2 is zero
    OCR2B = 0;

    // call interrupt on compare match (once per second)
    TIMSK2 = _BV(OCIE2B);
}


// save current time to eeprom
void time_wake(void) {
    // saving time to eeprom doesn't hurt. if something goes wrong, during
    // waking, the watchdog timer will reset the system and the system will
    // (hopefully) load the correct time after reset
    time_savetime();
}


// save current time to eeprom
void time_sleep(void) {
    // saving time to eeprom doesn't hurt. if the backup battery is dead, power
    // stored in capacitor should be sufficient to save current time.  if the
    // power outage is brief, time will be restored from eeprom and the clock
    // will still have a semi-reasonable time.
    time_savetime();
}


// save time to eeprom
void time_savedate(void) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
	eeprom_write_byte(&ee_time_year,  time.year );
	eeprom_write_byte(&ee_time_month, time.month);
	eeprom_write_byte(&ee_time_day,   time.day  );
    }
}


// save date to eeprom
void time_savetime(void) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
	eeprom_write_byte(&ee_time_hour,   time.hour  );
	eeprom_write_byte(&ee_time_minute, time.minute);
	eeprom_write_byte(&ee_time_second, time.second);
    }
}


// save status to eeprom
void time_savestatus(void) {
    eeprom_write_byte(&ee_time_status, time.status);
}


// load status from eeprom
void time_loadstatus(void) {
    time.status = eeprom_read_byte(&ee_time_status);
}


// save date format
void time_savedateformat(void) {
    eeprom_write_byte(&ee_time_dateformat, time.dateformat);
}


// load date format
void time_loaddateformat(void) {
    time.dateformat = eeprom_read_byte(&ee_time_dateformat);
}


// save time format
void time_savetimeformat(void) {
    eeprom_write_byte(&ee_time_timeformat, time.timeformat);
}


// load time format
void time_loadtimeformat(void) {
    time.timeformat = eeprom_read_byte(&ee_time_timeformat);
}


// set current time
void time_settime(uint8_t hour, uint8_t minute, uint8_t second) {
    // pause clock while mucking with clock timer
    // to prevent race conditions
    TCCR2B = 0;

    // delay until TCCR2B has been written
    while(ASSR & _BV(TCR2BUB));

    // stage time change for later drift correction
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
	// set fractional seconds to zero
	uint8_t TCNT2_old = TCNT2;
	TCNT2 = 0;

	// restart timer (clock with quartz oscillator divided by 256)
	TCCR2B  = _BV(CS22) | _BV(CS21);

	// determine if clock drift estimate should be computed
	if(time.status & TIME_UNSET) {
	    // reset drift monitoring variables
	    time.drift_total_seconds = 0;
	    time.drift_delta_seconds = 0;
	} else {
	    // compute the time difference between old and new times
	    int8_t delta_hour   = hour   - time.hour;
	    int8_t delta_minute = minute - time.minute;
	    int8_t delta_second = second - time.second;

	    int32_t total_delta = delta_hour;
	    total_delta *= 60;            // hours to minutes
	    total_delta += delta_minute;  // add minutes
	    total_delta *= 60;            // minutes to seconds
	    total_delta += delta_second;  // add seconds

	    // if time reset within drift delay period,
	    // update the number of seconds of clock drift
	    time.drift_delta_seconds += total_delta;

	    // adjust drift_delta_seconds for clock wrap-around
	    if(time.drift_delta_seconds > (int32_t)12 * 60 * 60) {
		time.drift_delta_seconds -= (int32_t)24 * 60 * 60;
	    }

	    if(time.drift_delta_seconds < (int32_t)-12 * 60 * 60) {
		time.drift_delta_seconds += (int32_t)24 * 60 * 60;
	    }

	    // defer setting drift adjustment to allow
	    // correction of an incorrectly set time
	    time.drift_delay_timer = TIME_DRIFT_SAVE_DELAY;

	    // tally fractional seconds
	    time.drift_frac_seconds += TCNT2_old;

	    // when fractional seconds make one full second, process
	    // the missed second with the drift correction code
	    if(time.drift_frac_seconds >= 127) {
		time.drift_frac_seconds -= 127;
		time_autodrift();
	    }
	}

	// set the new time
	time.hour   = hour;
	time.minute = minute;
	time.second = second;

	time.status &= ~TIME_UNSET;

	// ensure TCNT2 and TCR2B write completes
	while(ASSR & (_BV(TCN2UB) | _BV(TCR2BUB)));

	// ensure no pending OCR2A compare match interrupt
	TIFR2 = _BV(OCF2A);
    }
}


// set current date
void time_setdate(uint8_t year, uint8_t month, uint8_t day) {
    time.status &= ~TIME_UNSET;

    ATOMIC_BLOCK(ATOMIC_FORCEON) {
	time.year   = year;
	time.month  = month;
	time.day    = day;
    }
}


// add one second to current time
void time_tick(void) {
    //alarm_click();
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
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
    }

    // run autodst each minute
    if(!time.second) time_autodst(TRUE);

    // run drift correction
    time_autodrift();
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

    if(year > 0) {
	// leap day from year 2000
	++total_days;

	// leap days from other years since 2000
	total_days += (year - 1) >> 2;
    }

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


// returns given day-of-week as a program memory string
PGM_P time_wday2pstr(uint8_t wday) {
    switch(wday) {
	case TIME_SUN:
	    return PSTR(" sunday");
	case TIME_MON:
	    return PSTR(" monday");
	case TIME_TUE:
	    return PSTR("tuesday");
	case TIME_WED:
	    return PSTR("wednsday");
	case TIME_THU:
	    return PSTR("thursday");
	case TIME_FRI:
	    return PSTR(" friday");
	case TIME_SAT:
	    return PSTR("saturday");
	default:
	    return PSTR("-error-");
    }
}


// returns given month as program memory string
PGM_P time_month2pstr(uint8_t month) {
    switch(month) {
	case TIME_JAN:
	    return PSTR("jan");
	case TIME_FEB:
	    return PSTR("feb");
	case TIME_MAR:
	    return PSTR("mar");
	case TIME_APR:
	    return PSTR("apr");
	case TIME_MAY:
	    return PSTR("may");
	case TIME_JUN:
	    return PSTR("jun");
	case TIME_JUL:
	    return PSTR("jul");
	case TIME_AUG:
	    return PSTR("aug");
	case TIME_SEP:
	    return PSTR("sep");
	case TIME_OCT:
	    return PSTR("oct");
	case TIME_NOV:
	    return PSTR("nov");
	case TIME_DEC:
	    return PSTR("dec");
	default:
	    return PSTR("-error-");
    }
}


// if autodst is enabled, set dst accordingly; if adj_time is
// true and the dst state is changed, adjust time accordingly
void time_autodst(uint8_t adj_time) {
    uint8_t is_dst = time.status & TIME_DST;

    switch(time.status & TIME_AUTODST_MASK) {
	case TIME_AUTODST_USA:
	    is_dst = time_isdst_usa();
	    break;
	case TIME_AUTODST_EU_GMT:
	    is_dst = time_isdst_eu(0); // gmt + 0
	    break;
	case TIME_AUTODST_EU_CET:
	    is_dst = time_isdst_eu(1); // gmt + 1
	    break;
	case TIME_AUTODST_EU_EET:
	    is_dst = time_isdst_eu(2); // gmt + 2
	    break;
	default:
	    break;
    }

    if(is_dst) {
	time_dston(adj_time);
    } else {
	time_dstoff(adj_time);
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
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
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
}


// subtracts one hour from the current time
// (in the fall, clocks "fall back")
void time_fallback(void) {
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
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
}


// returns TRUE if currently observing DST, FALSE otherwise
uint8_t time_isdst_eu(int8_t rel_gmt) {
    uint8_t dst_day;

    // time changes at 1:00 GMT
    int8_t dst_hour = 1 + rel_gmt;

    switch(time.month) {
	case TIME_MAR:
	    // dst begins on the last sunday in march
	    dst_day = 31 - time_dayofweek(time.year, time.month, 31);

	    // before that day, dst is not in effect;
	    // after that day, dst is in effect
	    if(time.day < dst_day) return FALSE;
	    if(time.day > dst_day) return TRUE;

	    // at dst_hour, time jumps forward to dst_hour + 1,
	    // so the time between is an invalid range a time in
	    // this range probably means dst should be enabled,
	    // but is not yet, so return true
	    if(time.hour <  dst_hour) {
		return FALSE;
	    } else {
		return TRUE;
	    }

	    break;
	case TIME_OCT:
	    // dst ends on the last sunday in october
	    dst_day = 31 - time_dayofweek(time.year, time.month, 31);

	    // before that day, dst is in effect;
	    // after that day, dst is not in effect
	    if(time.day < dst_day) return TRUE;
	    if(time.day > dst_day) return FALSE;

	    // at dst_hour, time falls back to dst_hour - 1, so
	    // the time in between is an ambiguous time range
	    if(time.hour + 1 <  dst_hour) return TRUE;
	    if(time.hour     >= dst_hour) return FALSE;

	    // if time is ambiguous, return current dst state
	    if(time.status & TIME_DST) {
		return TRUE;
	    } else {
		return FALSE;
	    }

	    break;
	default:
	    if(TIME_MAR < time.month && time.month < TIME_OCT) {
		// between march and october,
	        // daylight saving time is in effect
		return TRUE;
	    } else {
		// before march and after october,
	        // daylight saving time is not in effect
		return FALSE;
	    }

	    break;
    }
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


// manages drift correction
void time_autodrift(void) {
    // adjust timekeeping according to current drift_adjust
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
	++time.drift_total_seconds;  // seconds since clock last set
    }

    ATOMIC_BLOCK(ATOMIC_FORCEON) {
	if(time.drift_adjust_timer) {
	    // seconds until next drift correction
	    --time.drift_adjust_timer;
	}
    }

    ATOMIC_BLOCK(ATOMIC_FORCEON) {
	// set timer2 top value to adjust duration of next second
	if(!time.drift_adjust_timer && time.drift_adjust > 0) {
	    // reset drift correction timer
	    time.drift_adjust_timer = time.drift_adjust;

	    // clock is slow: make next "second" faster
	    OCR2A = 126;  // 127 values, including zero
	} else if(!time.drift_adjust_timer && time.drift_adjust < 0) {
	    // reset drift correction timer
	    time.drift_adjust_timer = -time.drift_adjust;

	    // clock is fast: make next "second" longer
	    OCR2A = 128;  // 129 values, including zero
	} else {
	    // make next "second" of normal duration
	    OCR2A = 127; // 128 values, including zero
	}
    }

    if(system.status & SYSTEM_SLEEP) {
	// if drift adjustment calculation is pending,
	// defer it until external power restored
	return;
    }

    ATOMIC_BLOCK(ATOMIC_FORCEON) {
	// if drift adjustment calculation deferred and timer expires,
	// calculate and store new adjustment and update drift_adjust
	if(time.drift_delay_timer) {
	    --time.drift_delay_timer;
	    if(!time.drift_delay_timer) {
		// calculate and save new drift adjustment
		time_newdrift();

		NONATOMIC_BLOCK(NONATOMIC_FORCEOFF) {
		    // load new median adjustment
		    time_loaddriftmedian();
		}
	    }
	}
    }
}


// calculates and saves new drift value
// ***interrupts must be disabled while calling this function***
void time_newdrift(void) {
    int32_t new_adj;  // new drift adjustment value

    // disregard monitored drift data if time change too large
    // (maybe user mixed up timezones, mixed up am and pm, etc.)
    if(time.drift_delta_seconds < -TIME_MAX_DRIFT_TIME
	    || time.drift_delta_seconds > TIME_MAX_DRIFT_TIME) {
	// reset drift monitor variables
	time.drift_total_seconds = 0;
	time.drift_frac_seconds  = 0;
	time.drift_delta_seconds = 0;
	return;
    }
    
    // defer calculation of new adjustment if time change too small
    // (maybe user accidently entered set time mode, hit "set" three times,
    //  and only changed current time by a small amount; also, recording only
    //  larger adjustments gives more accurate results)
    if(-TIME_MIN_DRIFT_TIME < time.drift_delta_seconds
	    && time.drift_delta_seconds < TIME_MIN_DRIFT_TIME) {
	return;
    }

    // subtract effect of current drift adjustment, if any
    if(time.drift_adjust) {
	int32_t adj_sec = (time.drift_total_seconds / time.drift_adjust) >> 7;
	time.drift_total_seconds -= adj_sec;
	time.drift_delta_seconds += adj_sec;

	if(!time.drift_delta_seconds) return;
    }

    // calculate new drift adjustment
    new_adj = (time.drift_total_seconds / time.drift_delta_seconds) >> 7;

    // ultimately drift correction is stored as an int16,
    // so constrain new_adj to those bounds
    if(new_adj > INT16_MAX) new_adj = INT16_MAX;
    if(new_adj < INT16_MIN) new_adj = INT16_MIN;

    // reset drift monitor variables
    time.drift_total_seconds = 0;
    time.drift_delta_seconds = 0;

    // do not record if abs(new_adj) is too small; too small a value means
    // the clock is running very fast or very slow...probably a mistake...
    if(-TIME_MIN_DRIFT_ADJUST < new_adj && new_adj < TIME_MIN_DRIFT_ADJUST) {
	return;
    }

    // save drift adjustment
    uint8_t idx   = eeprom_read_byte(&ee_time_drift_idx  );
    uint8_t count = eeprom_read_byte(&ee_time_drift_count);
    eeprom_write_word((uint16_t*)&(ee_time_drift_table[idx]), new_adj);
    ++idx;
    if(count < idx) count = idx;
    idx %= TIME_DRIFT_TABLE_SIZE;
    eeprom_write_byte(&ee_time_drift_idx,   idx);
    eeprom_write_byte(&ee_time_drift_count, count);
}


// load drift correction from memory
void time_loaddriftmedian(void) {
    uint8_t table_size, half_table, min_idx;
    int16_t processed, min_val, cur_val;

    min_idx = 0;
    min_val = 0;  // default median if no data (0 means no drift correction)
    processed = 0;
    table_size = eeprom_read_byte(&ee_time_drift_count);
    half_table = table_size >> 1;
    if(table_size) ++half_table;

    // find the median value in the drift table,
    // for small n, this O(n^2) method is quick
    for(uint8_t i = 0; i < half_table; ++i) {
	min_val = 1;  // largest possible drift correction value
	for(uint8_t j = 0; j < table_size; ++j) {
	    if( !(processed & _BV(j)) ) {
		cur_val=eeprom_read_word((uint16_t*)&(ee_time_drift_table[j]));
		// Drift (ppm) is inversely proportional to the drift
		// adjustments compared here, so values need be ranked in
		// reciprocal space (e.g., ordered like -70, -90, -100, 100,
		// 90). And avr-gcc doea not handle floats efficiently:
		if( ((cur_val < 0) == (min_val < 0) ?
			    cur_val > min_val : cur_val < min_val) ) {
		    min_idx = j;
		    min_val = cur_val;
		}
	    }
	}
	processed |= _BV(min_idx);
    }

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
	time.drift_adjust = min_val;

	if(time.drift_adjust > 0) {
	    time.drift_adjust_timer =  time.drift_adjust;
	} else {
	    time.drift_adjust_timer = -time.drift_adjust;
	}
    }
}

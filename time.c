#include <avr/io.h>      // for using register names
#include <avr/eeprom.h>  // for storing data in eeprom memory
#include <avr/power.h>   // for enabling/disabling chip features

#include "time.h"
#include "timedef.h"

// current time and date
volatile time_t time;

// places to store the current time in EEMEM
uint8_t ee_time_year   EEMEM = TIME_DEFAULT_YEAR;
uint8_t ee_time_month  EEMEM = TIME_DEFAULT_MONTH;
uint8_t ee_time_day    EEMEM = TIME_DEFAULT_MDAY;
uint8_t ee_time_hour   EEMEM = TIME_DEFAULT_HOUR;
uint8_t ee_time_minute EEMEM = TIME_DEFAULT_MINUTE;
uint8_t ee_time_second EEMEM = TIME_DEFAULT_SECOND;


// load time from eeprom, setup counter2 with clock crystal
void time_init(void) {
    time_load();  // set clock with last known time

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
    time_save();
}


// save time and date to eeprom
void time_save(void) {
    eeprom_write_byte(&ee_time_year,   time.year  );
    eeprom_write_byte(&ee_time_month,  time.month );
    eeprom_write_byte(&ee_time_day,    time.day   );
    eeprom_write_byte(&ee_time_hour,   time.hour  );
    eeprom_write_byte(&ee_time_minute, time.minute);
    eeprom_write_byte(&ee_time_second, time.second);
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


// load time and date from eeprom
void time_load(void) {
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

    time.status = TIME_UNSET | TIME_MMDDYY | TIME_12HOUR | TIME_DST;
}


// add one second to current time
void time_tick(void) {
    ++time.second;

    if(time.second < 60) {
	return;
    } else {
	time.second = 0;
	++time.minute;
    }

    if(time.minute < 60) {
	return;
    } else {
	time.minute = 0;
	++time.hour;
    }

    if(time.hour < 24) {
	return;
    } else {
	time.hour = 0;
	++time.day;
    }

    if(time.day <= time_daysinmonth(time.year, time.month)) {
	return;
    } else {
	time.day = 1;
	++time.month;
    }

    if(time.month <= 12) {
	return;
    } else {
	time.month = 1;
	++time.year;
    }
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

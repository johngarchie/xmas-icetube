#include <avr/pgmspace.h> // for defining program memory strings with PSTR()
#include <util/atomic.h>  // for defining non-interruptable blocks

#include "mode.h"
#include "power.h"
#include "display.h"
#include "time.h"
#include "alarm.h"
#include "button.h"


// extern'ed clock mode data
volatile mode_t mode;


// private function declarations
void mode_update(uint8_t new_state);
void mode_time_display(uint8_t hour,   uint8_t minute,
	               uint8_t second, uint8_t dst);
void mode_alarm_display(uint8_t hour, uint8_t minute);
void mode_date_display(void);
void mode_textnum_display(PGM_P pstr, uint8_t num);
void mode_dayofweek_display(void);
void mode_monthday_display(void);


// set default startup mode after system reset
void mode_init() {
    mode_update(MODE_TIME_DISPLAY);
}


// called each second; updates current mode as required
void mode_tick(void) {
    if(mode.state == MODE_TIME_DISPLAY) {
	// update time display for each tick of the clock
	if(time.status & TIME_UNSET && time.second % 2) {
	    if(power.initial_mcusr & _BV(WDRF))  display_pstr(PSTR("wdt rset"));
	    if(power.initial_mcusr & _BV(BORF))  display_pstr(PSTR("bod rset"));
	    if(power.initial_mcusr & _BV(EXTRF)) display_pstr(PSTR("ext rset"));
	    if(power.initial_mcusr & _BV(PORF))  display_pstr(PSTR("pwr rset"));
	} else {
	    mode_update(MODE_TIME_DISPLAY);
	}
    }
}


// called each semisecond; updates current mode as required
void mode_semitick(void) {
    uint8_t btn = button_process();

    switch(mode.state) {
	case MODE_TIME_DISPLAY:
	    // display dash to indicate alarm status
	    display_dash(0, alarm.status & ALARM_SET
	                    && ( !(alarm.status & ALARM_SNOOZE)
		             || time.second % 2));

	    // check for button presses
	    switch(btn) {
		case BUTTON_MENU:
		    mode_update(MODE_MENU_SETALARM);
		    break;
		case BUTTON_PLUS:
		case BUTTON_SET:
		    mode_update(MODE_DAYOFWEEK_DISPLAY);
		    break;
		default:
		    break;
	    }
	    return;  // no timout; skip code below
	case MODE_DAYOFWEEK_DISPLAY:
	    if(btn || ++mode.timer > 1000) mode_update(MODE_MONTHDAY_DISPLAY);
	    return;  // time ourselves; skip code below
	case MODE_ALARMSET_DISPLAY:
	    if(btn || ++mode.timer > 1000) mode_update(MODE_ALARMTIME_DISPLAY);
	    return;  // time ourselves; skip code below
	case MODE_MONTHDAY_DISPLAY:
	case MODE_ALARMTIME_DISPLAY:
	case MODE_ALARMOFF_DISPLAY:
	case MODE_SNOOZEON_DISPLAY:
	    if(btn || ++mode.timer > 1000) mode_update(MODE_TIME_DISPLAY);
	    return;  // time ourselves; skip code below
	case MODE_MENU_SETALARM:
	    switch(btn) {
		case BUTTON_MENU:
		    mode_update(MODE_MENU_SETTIME);
		    break;
		case BUTTON_SET:
		    mode.tmp[MODE_TMP_HOUR]   = alarm.hour;
		    mode.tmp[MODE_TMP_MINUTE] = alarm.minute;
		    mode_update(MODE_SETALARM_HOUR);
		    break;
		case BUTTON_PLUS:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETALARM_HOUR:
	    switch(btn) {
		case BUTTON_MENU:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTON_SET:
		    mode_update(MODE_SETALARM_MINUTE);
		    break;
		case BUTTON_PLUS:
		    ++mode.tmp[MODE_TMP_HOUR];
		    mode.tmp[MODE_TMP_HOUR] %= 24;
		    mode_update(MODE_SETALARM_HOUR);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETALARM_MINUTE:
	    switch(btn) {
		case BUTTON_MENU:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTON_SET:
		    alarm_settime(mode.tmp[MODE_TMP_HOUR],
			          mode.tmp[MODE_TMP_MINUTE]);
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTON_PLUS:
		    ++mode.tmp[MODE_TMP_MINUTE];
		    mode.tmp[MODE_TMP_MINUTE] %= 60;
		    mode_update(MODE_SETALARM_MINUTE);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_MENU_SETTIME:
	    switch(btn) {
		case BUTTON_MENU:
		    mode_update(MODE_MENU_SETDATE);
		    break;
		case BUTTON_SET:
		    mode.tmp[MODE_TMP_HOUR]   = time.hour;
		    mode.tmp[MODE_TMP_MINUTE] = time.minute;
		    mode.tmp[MODE_TMP_SECOND] = time.second;

		    mode_update(MODE_SETTIME_HOUR);
		    break;
		case BUTTON_PLUS:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETTIME_HOUR:
	    switch(btn) {
		case BUTTON_MENU:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTON_SET:
		    mode_update(MODE_SETTIME_MINUTE);
		    break;
		case BUTTON_PLUS:
		    ++mode.tmp[MODE_TMP_HOUR];
		    mode.tmp[MODE_TMP_HOUR] %= 24;
		    mode_update(MODE_SETTIME_HOUR);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETTIME_MINUTE:
	    switch(btn) {
		case BUTTON_MENU:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTON_SET:
		    mode_update(MODE_SETTIME_SECOND);
		    break;
		case BUTTON_PLUS:
		    ++mode.tmp[MODE_TMP_MINUTE];
		    mode.tmp[MODE_TMP_MINUTE] %= 60;
		    mode_update(MODE_SETTIME_MINUTE);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETTIME_SECOND:
	    switch(btn) {
		case BUTTON_MENU:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTON_SET:
		    ATOMIC_BLOCK(ATOMIC_FORCEON) {
			time_settime(mode.tmp[MODE_TMP_HOUR],
				     mode.tmp[MODE_TMP_MINUTE],
				     mode.tmp[MODE_TMP_SECOND]);
			time_autodst(FALSE);
		    }
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTON_PLUS:
		    ++mode.tmp[MODE_TMP_SECOND];
		    mode.tmp[MODE_TMP_SECOND] %= 60;
		    mode_update(MODE_SETTIME_SECOND);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_MENU_SETDATE:
	    switch(btn) {
		case BUTTON_MENU:
		    mode_update(MODE_MENU_SETDST);
		    break;
		case BUTTON_SET:
		    // fetch the current date
		    mode.tmp[MODE_TMP_YEAR]  = time.year;
		    mode.tmp[MODE_TMP_MONTH] = time.month;
		    mode.tmp[MODE_TMP_DAY]   = time.day;

		    // and change to mode to set date
		    if(time.status & TIME_MMDDYY) {
			mode_update(MODE_SETDATE_MONTH);
		    } else {
			mode_update(MODE_SETDATE_DAY);
		    }
		    break;
		case BUTTON_PLUS:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETDATE_DAY:
	    switch(btn) {
		case BUTTON_MENU:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTON_SET:
		    if(time.status & TIME_MMDDYY) {
			mode_update(MODE_SETDATE_YEAR);
		    } else {
			mode_update(MODE_SETDATE_MONTH);
		    }
		    break;
		case BUTTON_PLUS:
		    ++mode.tmp[MODE_TMP_DAY];
		    if(mode.tmp[MODE_TMP_DAY] > 31) {
			mode.tmp[MODE_TMP_DAY] = 1;
		    }
		    mode_update(MODE_SETDATE_DAY);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETDATE_MONTH:
	    switch(btn) {
		case BUTTON_MENU:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTON_SET:
		    if(time.status & TIME_MMDDYY) {
			mode_update(MODE_SETDATE_DAY);
		    } else {
			mode_update(MODE_SETDATE_YEAR);
		    }
		    break;
		case BUTTON_PLUS:
		    ++mode.tmp[MODE_TMP_MONTH];
		    if(mode.tmp[MODE_TMP_MONTH] > 12) {
			mode.tmp[MODE_TMP_MONTH] = 1;
		    }
		    mode_update(MODE_SETDATE_MONTH);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETDATE_YEAR:
	    switch(btn) {
		case BUTTON_MENU:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTON_SET:
		    ATOMIC_BLOCK(ATOMIC_FORCEON) {
			time_setdate(mode.tmp[MODE_TMP_YEAR],
				     mode.tmp[MODE_TMP_MONTH],
				     mode.tmp[MODE_TMP_DAY]);
			time_autodst(FALSE);
		    }
		    time_savedate();
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTON_PLUS:
		    ++mode.tmp[MODE_TMP_YEAR];
		    mode.tmp[MODE_TMP_YEAR] %= 100;
		    mode_update(MODE_SETDATE_YEAR);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_MENU_SETDST:
	    switch(btn) {
		case BUTTON_MENU:
		    mode_update(MODE_MENU_SETPREFERENCES);
		    break;
		case BUTTON_SET:
		    *mode.tmp = time.status;
		    mode_update(MODE_SETDST_STATE);
		    break;
		case BUTTON_PLUS:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETDST_STATE:
	    switch(btn) {
		case BUTTON_MENU:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTON_SET:
		    ATOMIC_BLOCK(ATOMIC_FORCEON) {
			uint8_t autodst = *mode.tmp & TIME_AUTODST_MASK;

			if(autodst) {
			    time_autodst(FALSE);
			} else {
			    if(*mode.tmp & TIME_DST) {
				time_dston(TRUE);
			    } else {
				time_dstoff(TRUE);
			    }
			}

			time.status = *mode.tmp;
			time_savestatus();

			switch(autodst) {
			    case TIME_AUTODST_USA:
			    case TIME_AUTODST_NONE:
				mode_update(MODE_TIME_DISPLAY);
				break;
			    default:  // GMT, CET, or EET
				mode_update(MODE_SETDST_ZONE);
				break;
			}
		    }
		    break;
		case BUTTON_PLUS:
		    switch(*mode.tmp & TIME_AUTODST_MASK) {
			case TIME_AUTODST_USA:
			    // after autodst usa, go to manual
			    // dst for current dst state
			    *mode.tmp &= ~TIME_AUTODST_MASK;
			    break;
			case TIME_AUTODST_NONE:
			    if( (*mode.tmp & TIME_DST)
				    == (time.status & TIME_DST) ) {
				// after current dst state,
				// go to other dst state
				*mode.tmp ^= TIME_DST;
			    } else {
				// after other dst state,
				// go to autodst eu
				*mode.tmp ^= TIME_DST;
				switch(time.status & TIME_AUTODST_MASK) {
				    case TIME_AUTODST_EU_CET:
					*mode.tmp |= TIME_AUTODST_EU_CET;
					break;
				    case TIME_AUTODST_EU_EET:
					*mode.tmp |= TIME_AUTODST_EU_EET;
					break;
				    default:
					*mode.tmp |= TIME_AUTODST_EU_GMT;
					break;
				}
			    }
			    break;
			default:  // GMT, CET, or EET
			    // after autodst eu, go to autodst usa
			    *mode.tmp &= ~TIME_AUTODST_MASK;
			    *mode.tmp |=  TIME_AUTODST_USA;
			    break;
		    }
		    mode_update(MODE_SETDST_STATE);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETDST_ZONE:
	    switch(btn) {
		case BUTTON_MENU:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTON_SET:
		    ATOMIC_BLOCK(ATOMIC_FORCEON) {
			time.status = *mode.tmp;
			time_autodst(FALSE);
			time_savestatus();
		    }
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTON_PLUS:
		    switch(*mode.tmp & TIME_AUTODST_MASK) {
			case TIME_AUTODST_EU_CET:
			    *mode.tmp &= ~TIME_AUTODST_MASK;
			    *mode.tmp |=  TIME_AUTODST_EU_EET;
			    break;
			case TIME_AUTODST_EU_EET:
			    *mode.tmp &= ~TIME_AUTODST_MASK;
			    *mode.tmp |=  TIME_AUTODST_EU_GMT;
			    break;
			default:  // GMT
			    *mode.tmp &= ~TIME_AUTODST_MASK;
			    *mode.tmp |=  TIME_AUTODST_EU_CET;
			    break;
		    }
		    mode_update(MODE_SETDST_ZONE);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_MENU_SETPREFERENCES:
	    switch(btn) {
		case BUTTON_MENU:
		    mode_update(MODE_MENU_SETFORMAT);
		    break;
		case BUTTON_SET:
		    mode_update(MODE_MENU_SETBRIGHT);
		    break;
		case BUTTON_PLUS:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_MENU_SETBRIGHT:
	    switch(btn) {
		case BUTTON_MENU:
		    mode_update(MODE_MENU_SETVOLUME);
		    break;
		case BUTTON_SET:
		    *mode.tmp = display.brightness;
		    mode_update(MODE_SETBRIGHT_LEVEL);
		    break;
		case BUTTON_PLUS:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETBRIGHT_LEVEL:
	    switch(btn) {
		case BUTTON_MENU:
		    display_setbright(display.brightness);
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTON_SET:
		    display.brightness = *mode.tmp;
		    display_savebright();
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTON_PLUS:
		    ++(*mode.tmp);
		    *mode.tmp %= 11;
		    display_setbright(*mode.tmp);
		    mode_update(MODE_SETBRIGHT_LEVEL);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) {
			display_setbright(display.brightness);
		    }
		    break;
	    }
	    break;
	case MODE_MENU_SETVOLUME:
	    switch(btn) {
		case BUTTON_MENU:
		    mode_update(MODE_MENU_SETSNOOZE);
		    break;
		case BUTTON_SET:
		    if(alarm.volume_min != alarm.volume_max) {
			mode.tmp[MODE_TMP_VOL] = 11;
		    } else {
			mode.tmp[MODE_TMP_VOL] = alarm.volume_min;
			alarm.volume           = alarm.volume_min;
			alarm_beep(1000);
		    }
		    mode.tmp[MODE_TMP_VOL_MIN] = alarm.volume_min;
		    mode.tmp[MODE_TMP_VOL_MAX] = alarm.volume_max;
		    mode_update(MODE_SETVOLUME_LEVEL);
		    break;
		case BUTTON_PLUS:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETVOLUME_LEVEL:
	    switch(btn) {
		case BUTTON_MENU:
		    alarm.volume = alarm.volume_min;
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTON_SET:
		    if(mode.tmp[MODE_TMP_VOL] < 11) {
			alarm.volume     = mode.tmp[MODE_TMP_VOL];
			alarm.volume_min = mode.tmp[MODE_TMP_VOL];
			alarm.volume_max = mode.tmp[MODE_TMP_VOL];
			alarm_savevolume();
			mode_update(MODE_TIME_DISPLAY);
		    } else {
			alarm.volume = mode.tmp[MODE_TMP_VOL_MIN];
			alarm_beep(1000);
			mode_update(MODE_SETVOLUME_MIN);
		    }
		    break;
		case BUTTON_PLUS:
		    ++mode.tmp[MODE_TMP_VOL];
		    mode.tmp[MODE_TMP_VOL] %= 12;

		    if(mode.tmp[MODE_TMP_VOL] < 11) {
			alarm.volume = mode.tmp[MODE_TMP_VOL];
			alarm_beep(1000);
		    }

		    mode_update(MODE_SETVOLUME_LEVEL);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) {
			alarm.volume = alarm.volume_min;
		    }
		    break;
	    }
	    break;
	case MODE_SETVOLUME_MIN:
	    switch(btn) {
		case BUTTON_MENU:
		    alarm.volume = alarm.volume_min;
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTON_SET:
		    alarm.volume = mode.tmp[MODE_TMP_VOL_MAX];
		    alarm_beep(1000);
		    mode_update(MODE_SETVOLUME_MAX);
		    break;
		case BUTTON_PLUS:
		    ++mode.tmp[MODE_TMP_VOL_MIN];
		    mode.tmp[MODE_TMP_VOL_MIN] %= 10;
		    alarm.volume = mode.tmp[MODE_TMP_VOL_MIN];
		    alarm_beep(1000);
		    mode_update(MODE_SETVOLUME_MIN);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) {
			alarm.volume = alarm.volume_min;
		    }
		    break;
	    }
	    break;
	case MODE_SETVOLUME_MAX:
	    switch(btn) {
		case BUTTON_MENU:
		    alarm.volume = alarm.volume_min;
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTON_SET:
		    alarm.volume     = mode.tmp[MODE_TMP_VOL_MIN];
		    alarm.volume_min = mode.tmp[MODE_TMP_VOL_MIN];
		    alarm.volume_max = mode.tmp[MODE_TMP_VOL_MAX];
		    alarm_savevolume();
		    *mode.tmp = alarm.ramp_time;
		    mode_update(MODE_SETVOLUME_TIME);
		    break;
		case BUTTON_PLUS:
		    ++mode.tmp[MODE_TMP_VOL_MAX];
		    if(mode.tmp[MODE_TMP_VOL_MAX] > 10) {
			mode.tmp[MODE_TMP_VOL_MAX]=mode.tmp[MODE_TMP_VOL_MIN]+1;
		    }
		    alarm.volume = mode.tmp[MODE_TMP_VOL_MAX];
		    alarm_beep(1000);
		    mode_update(MODE_SETVOLUME_MAX);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) {
			alarm.volume = alarm.volume_min;
		    }
		    break;
	    }
	    break;
	case MODE_SETVOLUME_TIME:
	    switch(btn) {
		case BUTTON_MENU:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTON_SET:
		    alarm.ramp_time = *mode.tmp;
		    alarm_newramp();
		    alarm_saveramp();
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTON_PLUS:
		    if(++(*mode.tmp) > 30) *mode.tmp = 1;
		    mode_update(MODE_SETVOLUME_TIME);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_MENU_SETSNOOZE:
	    switch(btn) {
		case BUTTON_MENU:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTON_SET:
		    *mode.tmp = alarm.snooze_time / 60;
		    mode_update(MODE_SETSNOOZE_TIME);
		    break;
		case BUTTON_PLUS:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETSNOOZE_TIME:
	    switch(btn) {
		case BUTTON_MENU:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTON_SET:
		    alarm.snooze_time = *mode.tmp * 60;
		    alarm_savesnooze();
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTON_PLUS:
		    ++(*mode.tmp); *mode.tmp %= 31;
		    mode_update(MODE_SETSNOOZE_TIME);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_MENU_SETFORMAT:
	    switch(btn) {
		case BUTTON_MENU:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTON_SET:
		    *mode.tmp = time.status & TIME_12HOUR;
		    mode_update(MODE_SETTIME_FORMAT);
		    break;
		case BUTTON_PLUS:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETTIME_FORMAT:
	    switch(btn) {
		case BUTTON_MENU:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTON_SET:
		    // save the time format
		    if(*mode.tmp) {
			time.status |= TIME_12HOUR;
		    } else {
			time.status &= ~TIME_12HOUR;
		    }
		    time_savestatus();

		    // prompt for date format
		    *mode.tmp = time.status & TIME_MMDDYY;
		    mode_update(MODE_SETDATE_FORMAT);
		    break;
		case BUTTON_PLUS:
		    *mode.tmp = !*mode.tmp;
		    mode_update(MODE_SETTIME_FORMAT);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETDATE_FORMAT:
	    switch(btn) {
		case BUTTON_MENU:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTON_SET:
		    // save the time format
		    if(*mode.tmp) {
			time.status |=  TIME_MMDDYY;
		    } else {
			time.status &= ~TIME_MMDDYY;
		    }
		    time_savestatus();

		    // return to time desplay
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTON_PLUS:
		    *mode.tmp = !*mode.tmp;
		    mode_update(MODE_SETDATE_FORMAT);
		    break;
		default:
		    break;
	    }
	    break;
	default:
	    break;
    }

    if(++mode.timer > MODE_TIMEOUT) {
	mode_update(MODE_TIME_DISPLAY);
    }
}


// called when the alarm switch is turned on
void mode_alarmset(void) {
    if(mode.state == MODE_TIME_DISPLAY) {
	mode_update(MODE_ALARMSET_DISPLAY);
    }
}


// called when the alarm switch is turned off
void mode_alarmoff(void) {
    if(mode.state == MODE_TIME_DISPLAY) {
	mode_update(MODE_ALARMOFF_DISPLAY);
    }
}


// called when the snooze mode is activated
void mode_snoozing(void) {
    if(mode.state == MODE_TIME_DISPLAY) {
	mode_update(MODE_SNOOZEON_DISPLAY);
    }
}


// change mode to specified state and update display
void mode_update(uint8_t new_state) {
    mode.timer = 0;
    mode.state = new_state;

    switch(mode.state) {
	case MODE_TIME_DISPLAY:
	    mode_time_display(time.hour, time.minute,
		    	      time.second, time.status & TIME_DST);
	    break;
	case MODE_DAYOFWEEK_DISPLAY:
	    mode_dayofweek_display();
	    break;
	case MODE_MONTHDAY_DISPLAY:
	    mode_monthday_display();
	    break;
	case MODE_ALARMSET_DISPLAY:
	    display_pstr(PSTR("alar set"));
	    break;
	case MODE_ALARMTIME_DISPLAY:
	    mode_alarm_display(alarm.hour, alarm.minute);
	    break;
	case MODE_ALARMOFF_DISPLAY:
	    display_pstr(PSTR("alar off"));
	    break;
	case MODE_SNOOZEON_DISPLAY:
	    display_pstr(PSTR("snoozing"));
	    break;
	case MODE_MENU_SETALARM:
	    display_pstr(PSTR("set alar"));
	    break;
	case MODE_SETALARM_HOUR:
	    mode_alarm_display(mode.tmp[MODE_TMP_HOUR],
			       mode.tmp[MODE_TMP_MINUTE]);
	    if(time.status & TIME_12HOUR) {
		display_dotselect(1, 2);
	    } else {
		display_dotselect(2, 3);
	    }
	    break;
	case MODE_SETALARM_MINUTE:
	    mode_alarm_display(mode.tmp[MODE_TMP_HOUR],
			       mode.tmp[MODE_TMP_MINUTE]);
	    if(time.status & TIME_12HOUR) {
		display_dotselect(4, 5);
	    } else {
		display_dotselect(5, 6);
	    }
	    break;
	case MODE_MENU_SETTIME:
	    display_pstr(PSTR("set time"));
	    break;
	case MODE_SETTIME_HOUR:
	    mode_time_display(mode.tmp[MODE_TMP_HOUR],
		              mode.tmp[MODE_TMP_MINUTE],
			      mode.tmp[MODE_TMP_SECOND],
			      FALSE);
	    display_dotselect(1, 2);
	    break;
	case MODE_SETTIME_MINUTE:
	    mode_time_display(mode.tmp[MODE_TMP_HOUR],
		              mode.tmp[MODE_TMP_MINUTE],
			      mode.tmp[MODE_TMP_SECOND],
			      FALSE);
	    display_dotselect(4, 5);
	    break;
	case MODE_SETTIME_SECOND:
	    mode_time_display(mode.tmp[MODE_TMP_HOUR],
		              mode.tmp[MODE_TMP_MINUTE],
			      mode.tmp[MODE_TMP_SECOND],
			      FALSE);
	    display_dotselect(7, 8);
	    break;
	case MODE_MENU_SETDATE:
	    display_pstr(PSTR("set date"));
	    break;
	case MODE_SETDATE_DAY:
	    mode_date_display();
	    if(time.status & TIME_MMDDYY) {
		display_dotselect(4, 5);
	    } else {
		display_dotselect(1, 2);
	    }
	    break;
	case MODE_SETDATE_MONTH:
	    mode_date_display();
	    if(time.status & TIME_MMDDYY) {
		display_dotselect(1, 2);
	    } else {
		display_dotselect(4, 5);
	    }
	    break;
	case MODE_SETDATE_YEAR:
	    mode_date_display();
	    display_dotselect(7, 8);
	    break;
	case MODE_MENU_SETDST:
	    display_pstr(PSTR("set  dst"));
	    break;
	case MODE_SETDST_STATE:
	    switch(*mode.tmp & TIME_AUTODST_MASK) {
		case TIME_AUTODST_USA:
		    display_pstr(PSTR("dst  usa"));
		    display_dotselect(6, 8);
		    break;
		case TIME_AUTODST_NONE:
		    if(*mode.tmp & TIME_DST) {
			display_pstr(PSTR("dst   on"));
			display_dotselect(7, 8);
		    } else {
			display_pstr(PSTR("dst  off"));
			display_dotselect(6, 8);
		    }
		    break;
		default:  // GMT, CET, or EET
		    display_pstr(PSTR("dst   eu"));
		    display_dotselect(7, 8);
		    break;
	    }
	    break;
	case MODE_SETDST_ZONE:
	    switch(*mode.tmp & TIME_AUTODST_MASK) {
		case TIME_AUTODST_EU_CET:
		    display_pstr(PSTR("zone cet"));
		    display_dotselect(6, 8);
		    break;
		case TIME_AUTODST_EU_EET:
		    display_pstr(PSTR("zone eet"));
		    display_dotselect(6, 8);
		    break;
		default:  // GMT
		    display_pstr(PSTR("zone utc"));
		    display_dotselect(6, 8);
		    break;
	    }
	    break;
	case MODE_MENU_SETPREFERENCES:
	    display_pstr(PSTR("set pref"));
	    break;
	case MODE_MENU_SETBRIGHT:
	    display_pstr(PSTR("set brit"));
	    break;
	case MODE_SETBRIGHT_LEVEL:
	    mode_textnum_display(PSTR("brite"), *mode.tmp);
	    break;
	case MODE_MENU_SETVOLUME:
	    display_pstr(PSTR("set  vol"));
	    break;
	case MODE_SETVOLUME_LEVEL:
	    if(mode.tmp[MODE_TMP_VOL] == 11) {
		display_pstr(PSTR("vol ramp"));
		display_dotselect(5, 7);
	    } else {
		mode_textnum_display(PSTR("vol"), mode.tmp[MODE_TMP_VOL]);
	    }
	    break;
	case MODE_SETVOLUME_MIN:
	    mode_textnum_display(PSTR("v lo"), mode.tmp[MODE_TMP_VOL_MIN]);
	    break;
	case MODE_SETVOLUME_MAX:
	    mode_textnum_display(PSTR("v hi"), mode.tmp[MODE_TMP_VOL_MAX]);
	    break;
	case MODE_SETVOLUME_TIME:
	    mode_textnum_display(PSTR("time"), *mode.tmp);
	    break;
	case MODE_MENU_SETSNOOZE:
	    display_pstr(PSTR("set snoz"));
	    break;
	case MODE_SETSNOOZE_TIME:
	    if(*mode.tmp) {
		mode_textnum_display(PSTR("snoz"), *mode.tmp);
	    } else {
		display_pstr(PSTR("snoz off"));
		display_dotselect(6, 8);
	    }
	    break;
	case MODE_MENU_SETFORMAT:
	    display_pstr(PSTR("set form"));
	    break;
	case MODE_SETDATE_FORMAT:
	    if(*mode.tmp) {
		display_pstr(PSTR("mm-dd-yy"));
	    } else {
		display_pstr(PSTR("dd-mm-yy"));
	    }
	    display_dotselect(1, 8);
	    break;
	case MODE_SETTIME_FORMAT:
	    if(*mode.tmp) {
		display_pstr(PSTR("12-hour"));
	    } else {
		display_pstr(PSTR("24-hour"));
	    }
	    display_dotselect(1, 2);
	    break;
	default:
	    break;
    }

    mode.timer = 0;
    mode.state = new_state;
}


// updates the time display every second
void mode_time_display(uint8_t hour, uint8_t minute,
		       uint8_t second, uint8_t dst) {
    uint8_t hour_to_display = hour;

    if(time.status & TIME_12HOUR) {
	if(hour_to_display > 12) hour_to_display -= 12;
	if(hour_to_display == 0) hour_to_display  = 12;
    }

    // display current time
    display_digit(1, hour_to_display / 10);
    display_digit(2, hour_to_display % 10);
    display_clear(3);
    display_digit(4, minute / 10);
    display_digit(5, minute % 10);
    display_clear(6);
    display_digit(7, second / 10);
    display_digit(8, second % 10);

    // show am/pm and/or dst indicators
    if(time.status & TIME_12HOUR) {
	display_dot(0, hour >= 12);  // left-most circle if pm
	display_dot(8, dst);  // show rightmost dot if dst
    } else {
	display_dot(0, dst);  // show leftmost circle if dst
    }
}


// displays current alarm time
void mode_alarm_display(uint8_t hour, uint8_t minute) {
    uint8_t hour_to_display = hour;
    uint8_t idx = 0;

    if(time.status & TIME_12HOUR) {
	if(hour_to_display > 12) hour_to_display -= 12;
	if(hour_to_display == 0) hour_to_display  = 12;
    } else {
	display_clear(idx++);
    }

    // display current time
    display_clear(idx++);
    display_digit(idx++, hour_to_display / 10);
    display_digit(idx++, hour_to_display % 10);
    display_clear(idx++);
    display_digit(idx++, minute / 10);
    display_digit(idx++, minute % 10);
    display_clear(idx++);

    if(time.status & TIME_12HOUR) {
	if(hour < 12) {
	    display_char(idx++, 'a');
	    display_char(idx++, 'm');
	} else {
	    display_char(idx++, 'p');
	    display_char(idx++, 'm');
	}
    } else {
	display_clear(idx++);
    }
}


// display date
void mode_date_display(void) {
    if(time.status & TIME_MMDDYY) {
	display_digit(1, mode.tmp[MODE_TMP_MONTH] / 10);
	display_digit(2, mode.tmp[MODE_TMP_MONTH] % 10);

	display_char(3, '-');

	display_digit(4, mode.tmp[MODE_TMP_DAY] / 10);
	display_digit(5, mode.tmp[MODE_TMP_DAY] % 10);
    } else {
	display_digit(1, mode.tmp[MODE_TMP_DAY] / 10);
	display_digit(2, mode.tmp[MODE_TMP_DAY] % 10);

	display_char(3, '-');

	display_digit(4, mode.tmp[MODE_TMP_MONTH] / 10);
	display_digit(5, mode.tmp[MODE_TMP_MONTH] % 10);
    }

    display_char(6, '-');

    display_digit(7, mode.tmp[MODE_TMP_YEAR] / 10);
    display_digit(8, mode.tmp[MODE_TMP_YEAR] % 10);
}


// displays text with two-digit number, with number selected
void mode_textnum_display(PGM_P pstr, uint8_t num) {
    display_pstr(pstr);
    display_digit(7, num / 10);
    display_digit(8, num % 10);
    display_dotselect(7, 8);
}


// displays current day of the week
void mode_dayofweek_display(void) {
    switch(time_dayofweek(time.year, time.month, time.day)) {
	case TIME_SUN:
	    display_pstr(PSTR(" sunday"));
	    break;
	case TIME_MON:
	    display_pstr(PSTR(" monday"));
	    break;
	case TIME_TUE:
	    display_pstr(PSTR("tuesday"));
	    break;
	case TIME_WED:
	    display_pstr(PSTR("wednsday"));
	    break;
	case TIME_THU:
	    display_pstr(PSTR("thursday"));
	    break;
	case TIME_FRI:
	    display_pstr(PSTR(" friday"));
	    break;
	case TIME_SAT:
	    display_pstr(PSTR("saturday"));
	    break;
	default:
	    break;
    }
}


// displays current month and day
void mode_monthday_display(void) {
    switch(time.month) {
	case TIME_JAN:
	    display_pstr(PSTR(" jan"));
	    break;
	case TIME_FEB:
	    display_pstr(PSTR(" feb"));
	    break;
	case TIME_MAR:
	    display_pstr(PSTR(" mar"));
	    break;
	case TIME_APR:
	    display_pstr(PSTR(" apr"));
	    break;
	case TIME_MAY:
	    display_pstr(PSTR(" may"));
	    break;
	case TIME_JUN:
	    display_pstr(PSTR(" jun"));
	    break;
	case TIME_JUL:
	    display_pstr(PSTR(" jul"));
	    break;
	case TIME_AUG:
	    display_pstr(PSTR(" aug"));
	    break;
	case TIME_SEP:
	    display_pstr(PSTR(" sep"));
	    break;
	case TIME_OCT:
	    display_pstr(PSTR(" oct"));
	    break;
	case TIME_NOV:
	    display_pstr(PSTR(" nov"));
	    break;
	case TIME_DEC:
	    display_pstr(PSTR(" dec"));
	    break;
	default:
	    break;
    }

    display_digit(6, time.day / 10);
    display_digit(7, time.day % 10);
}

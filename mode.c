// mode.c  --  time display and user interaction
//
// Time display and menu configuration are implemented through various
// modes or states within a finite state machine.
//


#include <avr/pgmspace.h> // for defining program memory strings with PSTR()
#include <util/atomic.h>  // for defining non-interruptable blocks


#include "mode.h"
#include "system.h"   // for system.initial_mcusr
#include "display.h"  // for setting display contents
#include "time.h"     // for displaying and setting time and date
#include "alarm.h"    // for setting and displaying alarm status
#include "pizo.h"     // for making clicks and alarm sounds
#include "buttons.h"  // for processing button presses
#include "gps.h"      // for setting the utc offset
#include "usart.h"    // for debugging output


// extern'ed clock mode data
volatile mode_t mode;


// private function declarations
void mode_update(uint8_t new_state);
void mode_zone_display(void);
void mode_time_display(uint8_t hour, uint8_t minute, uint8_t second);
void mode_alarm_display(uint8_t hour, uint8_t minute);
void mode_textnum_display(PGM_P pstr, int8_t num);
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
	   if(system.initial_mcusr & _BV(WDRF)) {
	       display_pstr(0, PSTR("wdt rset"));
	   } else if(system.initial_mcusr & _BV(BORF)) {
	       display_pstr(0, PSTR("bod rset"));
	   } else if(system.initial_mcusr & _BV(EXTRF)) {
	       display_pstr(0, PSTR("ext rset"));
	   } else if(system.initial_mcusr & _BV(PORF)) {
	       display_pstr(0, PSTR("pwr rset"));
	   } else {
	       display_pstr(0, PSTR(""));
	   }
	} else if(gps.data_timer && !gps.warn_timer && time.second % 2) {
	    display_pstr(0, PSTR("gps lost"));
	} else {
	    mode_update(MODE_TIME_DISPLAY);
	}
    }
}


// called each semisecond; updates current mode as required
void mode_semitick(void) {
    uint8_t btn = buttons_process();

    // enable or extend snooze on button press
    if(btn) if(alarm_onbutton()) btn = 0;

    switch(mode.state) {
	case MODE_TIME_DISPLAY:
	    // check for button presses
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_MENU_SETALARM);
		    break;
		case BUTTONS_PLUS:
		case BUTTONS_SET:
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
		case BUTTONS_MENU:
		    if(gps.data_timer) {
			mode_update(MODE_MENU_SETZONE);
		    } else {
			mode_update(MODE_MENU_SETTIME);
		    }
		    break;
		case BUTTONS_SET:
		    mode.tmp[MODE_TMP_HOUR]   = alarm.hour;
		    mode.tmp[MODE_TMP_MINUTE] = alarm.minute;
		    mode_update(MODE_SETALARM_HOUR);
		    break;
		case BUTTONS_PLUS:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETALARM_HOUR:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTONS_SET:
		    mode_update(MODE_SETALARM_MINUTE);
		    break;
		case BUTTONS_PLUS:
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
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTONS_SET:
		    alarm_settime(mode.tmp[MODE_TMP_HOUR],
			          mode.tmp[MODE_TMP_MINUTE]);
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTONS_PLUS:
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
		case BUTTONS_MENU:
		    mode_update(MODE_MENU_SETDATE);
		    break;
		case BUTTONS_SET:
		    mode.tmp[MODE_TMP_HOUR]   = time.hour;
		    mode.tmp[MODE_TMP_MINUTE] = time.minute;
		    mode.tmp[MODE_TMP_SECOND] = time.second;

		    mode_update(MODE_SETTIME_HOUR);
		    break;
		case BUTTONS_PLUS:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETTIME_HOUR:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTONS_SET:
		    mode_update(MODE_SETTIME_MINUTE);
		    break;
		case BUTTONS_PLUS:
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
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTONS_SET:
		    mode_update(MODE_SETTIME_SECOND);
		    break;
		case BUTTONS_PLUS:
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
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTONS_SET:
		    ATOMIC_BLOCK(ATOMIC_FORCEON) {
			time_settime(mode.tmp[MODE_TMP_HOUR],
				     mode.tmp[MODE_TMP_MINUTE],
				     mode.tmp[MODE_TMP_SECOND]);
			time_autodst(FALSE);
		    }
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTONS_PLUS:
		    ++mode.tmp[MODE_TMP_SECOND];
		    mode.tmp[MODE_TMP_SECOND] %= 60;
		    mode_update(MODE_SETTIME_SECOND);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_MENU_SETZONE:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_MENU_SETDST);
		    break;
		case BUTTONS_SET:
		    mode.tmp[MODE_TMP_HOUR]   = gps.rel_utc_hour;
		    mode.tmp[MODE_TMP_MINUTE] = gps.rel_utc_minute;
		    mode_update(MODE_SETZONE_HOUR);
		    break;
		case BUTTONS_PLUS:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETZONE_HOUR:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTONS_SET:
		    mode_update(MODE_SETZONE_MINUTE);
		    break;
		case BUTTONS_PLUS:
		    if(mode.tmp[MODE_TMP_HOUR] >= GPS_HOUR_OFFSET_MAX) {
			mode.tmp[MODE_TMP_HOUR] = GPS_HOUR_OFFSET_MIN;
		    } else {
			++mode.tmp[MODE_TMP_HOUR];
		    }
		    mode_update(MODE_SETZONE_HOUR);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETZONE_MINUTE:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTONS_SET:
		    ATOMIC_BLOCK(ATOMIC_FORCEON) {
			gps.rel_utc_hour   = mode.tmp[MODE_TMP_HOUR  ];
			gps.rel_utc_minute = mode.tmp[MODE_TMP_MINUTE];
			gps_saverelutc();
		    }
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTONS_PLUS:
		    ++mode.tmp[MODE_TMP_MINUTE];
		    mode.tmp[MODE_TMP_MINUTE] %= 60;
		    mode_update(MODE_SETZONE_MINUTE);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_MENU_SETDATE:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_MENU_SETDST);
		    break;
		case BUTTONS_SET:
		    // fetch the current date
		    mode.tmp[MODE_TMP_YEAR]  = time.year;
		    mode.tmp[MODE_TMP_MONTH] = time.month;
		    mode.tmp[MODE_TMP_DAY]   = time.day;
		    mode_update(MODE_SETDATE_YEAR);
		    break;
		case BUTTONS_PLUS:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETDATE_DAY:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTONS_SET:
		    ATOMIC_BLOCK(ATOMIC_FORCEON) {
			time_setdate(mode.tmp[MODE_TMP_YEAR],
				     mode.tmp[MODE_TMP_MONTH],
				     mode.tmp[MODE_TMP_DAY]);
			time_autodst(FALSE);
		    }
		    time_savedate();
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTONS_PLUS:
		    if(++mode.tmp[MODE_TMP_DAY]
			    > time_daysinmonth(mode.tmp[MODE_TMP_YEAR],
					       mode.tmp[MODE_TMP_MONTH])) {
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
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTONS_SET:
		    mode_update(MODE_SETDATE_DAY);
		    break;
		case BUTTONS_PLUS:
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
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTONS_SET:
		    mode_update(MODE_SETDATE_MONTH);
		    break;
		case BUTTONS_PLUS:
		    if(++mode.tmp[MODE_TMP_YEAR] > 50) {
		        mode.tmp[MODE_TMP_YEAR] = 10;
		    }
		    mode_update(MODE_SETDATE_YEAR);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_MENU_SETDST:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_MENU_SETSOUND);
		    break;
		case BUTTONS_SET:
		    *mode.tmp = time.status;
		    mode_update(MODE_SETDST_STATE);
		    break;
		case BUTTONS_PLUS:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETDST_STATE:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTONS_SET:
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
		case BUTTONS_PLUS:
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
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTONS_SET:
		    ATOMIC_BLOCK(ATOMIC_FORCEON) {
			time.status = *mode.tmp;
			time_autodst(FALSE);
			time_savestatus();
		    }
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTONS_PLUS:
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
	case MODE_MENU_SETBRIGHT:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_MENU_SETDIGITBRIGHT);
		    break;
		case BUTTONS_SET:
		    mode.tmp[MODE_TMP_MIN] = display.bright_min;
		    mode.tmp[MODE_TMP_MAX] = display.bright_max;

		    if(display.bright_min == display.bright_max) {
			*mode.tmp = display.bright_min;
		    } else {
			*mode.tmp = 11;
		    }

		    mode_update(MODE_SETBRIGHT_LEVEL);
		    break;
		case BUTTONS_PLUS:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETBRIGHT_LEVEL:
	    switch(btn) {
		case BUTTONS_MENU:
		    display_loadbright();
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTONS_SET:
		    if(*mode.tmp == 11) {
			*mode.tmp = mode.tmp[MODE_TMP_MIN];
			mode_update(MODE_SETBRIGHT_MIN);
		    } else {
			display_savebright();
			mode_update(MODE_TIME_DISPLAY);
		    }
		    break;
		case BUTTONS_PLUS:
		    ++(*mode.tmp);
		    *mode.tmp %= 12;
		    mode_update(MODE_SETBRIGHT_LEVEL);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) {
			display_loadbright();
		    }
		    break;
	    }
	    break;
	case MODE_SETBRIGHT_MIN:
	    switch(btn) {
		case BUTTONS_MENU:
		    display_loadbright();
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTONS_SET:
		    // save brightness minimum
		    mode.tmp[MODE_TMP_MIN] = *mode.tmp;

		    // load brightness maximum
		    if(mode.tmp[MODE_TMP_MAX] > *mode.tmp) {
			*mode.tmp = mode.tmp[MODE_TMP_MAX];
		    } else {
		        if(*mode.tmp < 1) {
			    *mode.tmp = 1;
			} else {
			    ++(*mode.tmp);
			}
		    }

		    // set brightness maximum
		    mode_update(MODE_SETBRIGHT_MAX);
		    break;
		case BUTTONS_PLUS:
		    if(++(*mode.tmp) > 9) *mode.tmp = -5;
		    mode_update(MODE_SETBRIGHT_MIN);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) {
			display_loadbright();
		    }
		    break;
	    }
	    break;
	case MODE_SETBRIGHT_MAX:
	    switch(btn) {
		case BUTTONS_MENU:
		    display_loadbright();
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTONS_SET:
		    display.bright_min = mode.tmp[MODE_TMP_MIN];
		    display.bright_max = *mode.tmp;
		    display_autodim();
		    display_savebright();
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTONS_PLUS:
		    if(++(*mode.tmp) > 20) {
		        if(mode.tmp[MODE_TMP_MIN] < 1) {
			    *mode.tmp = 1;
			} else {
			    *mode.tmp = mode.tmp[MODE_TMP_MIN] + 1;
			}
		    }

		    mode_update(MODE_SETBRIGHT_MAX);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) {
			display_loadbright();
		    }
		    break;
	    }
	    break;
	case MODE_MENU_SETDIGITBRIGHT:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_MENU_SETSNOOZE);
		    break;
		case BUTTONS_SET:
		    *mode.tmp = 0;
		    mode_update(MODE_SETDIGITBRIGHT);
		    break;
		case BUTTONS_PLUS:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETDIGITBRIGHT:
	    switch(btn) {
		case BUTTONS_MENU:
		    display_loaddigittimes();
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTONS_SET:
		    if(++(*mode.tmp) < DISPLAY_SIZE) {
			mode_update(MODE_SETDIGITBRIGHT);
		    } else {
			display_savedigittimes();
			mode_update(MODE_TIME_DISPLAY);
		    }
		    break;
		case BUTTONS_PLUS:
		    if(display.digit_times[*mode.tmp] < 2 * UINT8_MAX / 3 ) {
			display.digit_times[*mode.tmp] +=
					display.digit_times[*mode.tmp] >> 1;
		    } else {
			display.digit_times[*mode.tmp] = 15;
		    }

		    display_noflicker();

		    mode_update(MODE_SETDIGITBRIGHT);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_MENU_SETSOUND:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_MENU_SETBRIGHT);
		    break;
		case BUTTONS_SET:
		    pizo_setvolume((alarm.volume_min+alarm.volume_max)>>1, 0);
		    pizo_tryalarm_start();
		    mode_update(MODE_SETSOUND_TYPE);
		    break;
		case BUTTONS_PLUS:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETSOUND_TYPE:
	    switch(btn) {
		case BUTTONS_MENU:
		    pizo_tryalarm_stop();
		    pizo_loadsound();
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTONS_SET:
		    pizo_savesound();
		    if(alarm.volume_min != alarm.volume_max) {
			*mode.tmp = 11;
			pizo_tryalarm_stop();
		    } else {
			*mode.tmp = alarm.volume_min;
			pizo_setvolume(alarm.volume_min, 0);
			pizo_tryalarm_start();
		    }
		    mode.tmp[MODE_TMP_MIN] = alarm.volume_min;
		    mode.tmp[MODE_TMP_MAX] = alarm.volume_max;
		    mode_update(MODE_SETSOUND_VOL);
		    break;
		case BUTTONS_PLUS:
		    pizo_nextsound();
		    pizo_tryalarm_start();
		    mode_update(MODE_SETSOUND_TYPE);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) {
			pizo_tryalarm_stop();
			pizo_loadsound();
		    }
		    break;
	    }
	    break;
	case MODE_SETSOUND_VOL:
	    switch(btn) {
		case BUTTONS_MENU:
		    pizo_tryalarm_stop();
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTONS_SET:
		    if(*mode.tmp < 11) {
			pizo_tryalarm_stop();
			pizo_setvolume(*mode.tmp, 0);
			alarm.volume_min = *mode.tmp;
			alarm.volume_max = *mode.tmp;
			alarm_savevolume();
			mode_update(MODE_TIME_DISPLAY);
		    } else {
			pizo_setvolume(mode.tmp[MODE_TMP_MIN], 0);
			pizo_tryalarm_start();
			mode_update(MODE_SETSOUND_VOL_MIN);
		    }
		    break;
		case BUTTONS_PLUS:
		    ++(*mode.tmp);
		    *mode.tmp %= 12;

		    if(*mode.tmp < 11) {
			pizo_setvolume(*mode.tmp, 0);
			pizo_tryalarm_start();
		    } else {
			pizo_tryalarm_stop();
		    }

		    mode_update(MODE_SETSOUND_VOL);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) pizo_tryalarm_stop();
		    break;
	    }
	    break;
	case MODE_SETSOUND_VOL_MIN:
	    switch(btn) {
		case BUTTONS_MENU:
		    pizo_tryalarm_stop();
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTONS_SET:
		    pizo_setvolume(mode.tmp[MODE_TMP_MAX], 0);
		    pizo_tryalarm_start();
		    mode_update(MODE_SETSOUND_VOL_MAX);
		    break;
		case BUTTONS_PLUS:
		    ++mode.tmp[MODE_TMP_MIN];
		    mode.tmp[MODE_TMP_MIN] %= 10;
		    pizo_setvolume(mode.tmp[MODE_TMP_MIN], 0);
		    pizo_tryalarm_start();
		    mode_update(MODE_SETSOUND_VOL_MIN);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) pizo_tryalarm_stop();
		    break;
	    }
	    break;
	case MODE_SETSOUND_VOL_MAX:
	    switch(btn) {
		case BUTTONS_MENU:
		    pizo_tryalarm_stop();
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTONS_SET:
		    pizo_tryalarm_stop();
		    alarm.volume_min = mode.tmp[MODE_TMP_MIN];
		    alarm.volume_max = mode.tmp[MODE_TMP_MAX];
		    alarm_savevolume();
		    *mode.tmp = alarm.ramp_time;
		    mode_update(MODE_SETSOUND_TIME);
		    break;
		case BUTTONS_PLUS:
		    ++mode.tmp[MODE_TMP_MAX];
		    if(mode.tmp[MODE_TMP_MAX] > 10) {
			mode.tmp[MODE_TMP_MAX] = mode.tmp[MODE_TMP_MIN] + 1;
		    }
		    pizo_setvolume(mode.tmp[MODE_TMP_MAX], 0);
		    pizo_tryalarm_start();
		    mode_update(MODE_SETSOUND_VOL_MAX);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) pizo_tryalarm_stop();
		    break;
	    }
	    break;
	case MODE_SETSOUND_TIME:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTONS_SET:
		    alarm.ramp_time = *mode.tmp;
		    alarm_newramp();  // calculate alarm.ramp_int
		    alarm_saveramp(); // save alarm.ramp_time
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTONS_PLUS:
		    if(++(*mode.tmp) > 60) *mode.tmp = 1;
		    mode_update(MODE_SETSOUND_TIME);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_MENU_SETSNOOZE:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_MENU_SETTIMEFORMAT);
		    break;
		case BUTTONS_SET:
		    *mode.tmp = alarm.snooze_time / 60;
		    mode_update(MODE_SETSNOOZE_TIME);
		    break;
		case BUTTONS_PLUS:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETSNOOZE_TIME:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTONS_SET:
		    alarm.snooze_time = *mode.tmp * 60;
		    alarm_savesnooze();
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTONS_PLUS:
		    ++(*mode.tmp); *mode.tmp %= 31;
		    mode_update(MODE_SETSNOOZE_TIME);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_MENU_SETTIMEFORMAT:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTONS_SET:
		    *mode.tmp = time.status & TIME_12HOUR;
		    mode_update(MODE_SETTIMEFORMAT_12HOUR);
		    break;
		case BUTTONS_PLUS:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETTIMEFORMAT_12HOUR:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTONS_SET:
		    time.status ^= TIME_12HOUR;
		    time_savestatus();
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTONS_PLUS:
		    *mode.tmp = !*mode.tmp;
		    mode_update(MODE_SETTIMEFORMAT_12HOUR);
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


// set default mode when waking from sleep
void mode_wake(void) {
    mode_update(MODE_TIME_DISPLAY);
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
	    // display current time
	    mode_time_display(time.hour, time.minute, time.second);

	    // show daylight savings time indicator
	    if(time.status & TIME_12HOUR) {
		// show rightmost decimal if dst
		display_dot(8, time.status & TIME_DST);
	    } else {
		// otherwise, show circle if dst
		display_dot(0, time.status & TIME_DST);
	    }

	    // show alarm status with leftmost dash
	    display_dash(0, alarm.status & ALARM_SET
	                    && ( !(alarm.status & ALARM_SNOOZE)
		             || time.second % 2));

	    break;
	case MODE_DAYOFWEEK_DISPLAY:
	    mode_dayofweek_display();
	    break;
	case MODE_MONTHDAY_DISPLAY:
	    mode_monthday_display();
	    break;
	case MODE_ALARMSET_DISPLAY:
	    display_pstr(0, PSTR("alar set"));
	    break;
	case MODE_ALARMTIME_DISPLAY:
	    mode_alarm_display(alarm.hour, alarm.minute);
	    break;
	case MODE_ALARMOFF_DISPLAY:
	    display_pstr(0, PSTR("alar off"));
	    break;
	case MODE_SNOOZEON_DISPLAY:
	    display_pstr(0, PSTR("snoozing"));
	    break;
	case MODE_MENU_SETALARM:
	    display_pstr(0, PSTR("set alar"));
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
	    display_pstr(0, PSTR("set time"));
	    break;
	case MODE_SETTIME_HOUR:
	    mode_time_display(mode.tmp[MODE_TMP_HOUR],
		              mode.tmp[MODE_TMP_MINUTE],
			      mode.tmp[MODE_TMP_SECOND]);
	    display_dotselect(1, 2);
	    break;
	case MODE_SETTIME_MINUTE:
	    mode_time_display(mode.tmp[MODE_TMP_HOUR],
		              mode.tmp[MODE_TMP_MINUTE],
			      mode.tmp[MODE_TMP_SECOND]);
	    display_dotselect(4, 5);
	    break;
	case MODE_SETTIME_SECOND:
	    mode_time_display(mode.tmp[MODE_TMP_HOUR],
		              mode.tmp[MODE_TMP_MINUTE],
			      mode.tmp[MODE_TMP_SECOND]);
	    display_dotselect(7, 8);
	    break;
	case MODE_MENU_SETZONE:
	    display_pstr(0, PSTR("set zone"));
	    break;
	case MODE_SETZONE_HOUR:
	    mode_zone_display();
	    display_dotselect(2, 3);
	    break;
	case MODE_SETZONE_MINUTE:
	    mode_zone_display();
	    display_dotselect(6, 7);
	    break;
	case MODE_MENU_SETDATE:
	    display_pstr(0, PSTR("set date"));
	    break;
	case MODE_SETDATE_DAY:
	    display_pstr(0, time_month2pstr(mode.tmp[MODE_TMP_MONTH]));
	    display_digit(5, mode.tmp[MODE_TMP_DAY] / 10);
	    display_digit(6, mode.tmp[MODE_TMP_DAY] % 10);
	    display_dotselect(5, 6);
	    break;
	case MODE_SETDATE_MONTH:
	    display_pstr(0, PSTR("20"));
	    display_digit(3, mode.tmp[MODE_TMP_YEAR] / 10);
	    display_digit(4, mode.tmp[MODE_TMP_YEAR] % 10);
	    display_pstr(6, time_month2pstr(mode.tmp[MODE_TMP_MONTH]));
	    display_dotselect(6, 8);
	    break;
	case MODE_SETDATE_YEAR:
	    display_pstr(0, PSTR("20"));
	    display_digit(3, mode.tmp[MODE_TMP_YEAR] / 10);
	    display_digit(4, mode.tmp[MODE_TMP_YEAR] % 10);
	    display_pstr(6, time_month2pstr(mode.tmp[MODE_TMP_MONTH]));
	    display_dotselect(3, 4);
	    break;
	case MODE_MENU_SETDST:
	    display_pstr(0, PSTR("set dst"));
	    break;
	case MODE_SETDST_STATE:
	    switch(*mode.tmp & TIME_AUTODST_MASK) {
		case TIME_AUTODST_USA:
		    display_pstr(0, PSTR("dst  usa"));
		    display_dotselect(6, 8);
		    break;
		case TIME_AUTODST_NONE:
		    if(*mode.tmp & TIME_DST) {
			display_pstr(0, PSTR("dst   on"));
			display_dotselect(7, 8);
		    } else {
			display_pstr(0, PSTR("dst  off"));
			display_dotselect(6, 8);
		    }
		    break;
		default:  // GMT, CET, or EET
		    display_pstr(0, PSTR("dst   eu"));
		    display_dotselect(7, 8);
		    break;
	    }
	    break;
	case MODE_SETDST_ZONE:
	    switch(*mode.tmp & TIME_AUTODST_MASK) {
		case TIME_AUTODST_EU_CET:
		    display_pstr(0, PSTR("zone cet"));
		    display_dotselect(6, 8);
		    break;
		case TIME_AUTODST_EU_EET:
		    display_pstr(0, PSTR("zone eet"));
		    display_dotselect(6, 8);
		    break;
		default:  // GMT
		    display_pstr(0, PSTR("zone utc"));
		    display_dotselect(6, 8);
		    break;
	    }
	    break;
	case MODE_MENU_SETBRIGHT:
	    display_pstr(0, PSTR("set brit"));
	    break;
	case MODE_SETBRIGHT_LEVEL:
	    if(*mode.tmp == 11) {
		display_loadbright();
		display_pstr(0, PSTR("bri auto"));
		display_dotselect(5, 8);
	    } else {
		display.bright_min = display.bright_max = *mode.tmp;
		display_autodim();
		mode_textnum_display(PSTR("bri"), *mode.tmp);
	    }
	    break;
	case MODE_SETBRIGHT_MIN:
	    if(*mode.tmp < 0) {
		// user might not be able to see lowest brightness
		display_loadbright();
	    } else {
		display.bright_min = display.bright_max = *mode.tmp;
		display_autodim();
	    }
	    mode_textnum_display(PSTR("b min"), *mode.tmp);
	    break;
	case MODE_SETBRIGHT_MAX:
	    display.bright_min = display.bright_max = *mode.tmp;
	    display_autodim();
	    mode_textnum_display(PSTR("b max"), *mode.tmp);
	    break;
	case MODE_MENU_SETDIGITBRIGHT:
	    display_pstr(0, PSTR("set digt"));
	    break;
	case MODE_SETDIGITBRIGHT:
	    display_dot(0, TRUE);

	    for(uint8_t i = 1; i < DISPLAY_SIZE; ++i) {
		display_digit(i, 8);
	    }

	    if(*mode.tmp) {
		display_dot(*mode.tmp, TRUE);
		display_dash(0, FALSE);
	    } else {
		display_dash(*mode.tmp, TRUE);
	    }

	    break;
	case MODE_MENU_SETSOUND:
	    display_pstr(0, PSTR("set alrt"));
	    break;
	case MODE_SETSOUND_TYPE:
	    switch(pizo.status & PIZO_SOUND_MASK) {
		case PIZO_SOUND_MERRY_XMAS:
		    display_pstr(0, PSTR("mery chr"));
		    display_dotselect(1, 8);
		    break;
		default:
		    display_pstr(0, PSTR(" buzzer"));
		    display_dotselect(2, 7);
		    break;
	    }
	    break;
	case MODE_SETSOUND_VOL:
	    if(*mode.tmp == 11) {
		display_pstr(0, PSTR("vol prog"));
		display_dotselect(5, 8);
	    } else {
		mode_textnum_display(PSTR("vol"), *mode.tmp);
	    }
	    break;
	case MODE_SETSOUND_VOL_MIN:
	    mode_textnum_display(PSTR("v min"), mode.tmp[MODE_TMP_MIN]);
	    break;
	case MODE_SETSOUND_VOL_MAX:
	    mode_textnum_display(PSTR("v max"), mode.tmp[MODE_TMP_MAX]);
	    break;
	case MODE_SETSOUND_TIME:
	    mode_textnum_display(PSTR("time"), *mode.tmp);
	    break;
	case MODE_MENU_SETSNOOZE:
	    display_pstr(0, PSTR("set snoz"));
	    break;
	case MODE_SETSNOOZE_TIME:
	    if(*mode.tmp) {
		mode_textnum_display(PSTR("snoz"), *mode.tmp);
	    } else {
		display_pstr(0, PSTR("snoz off"));
		display_dotselect(6, 8);
	    }
	    break;
	case MODE_MENU_SETTIMEFORMAT:
	    display_pstr(0, PSTR("set tfmt"));
	    break;
	case MODE_SETTIMEFORMAT_12HOUR:
	    if(*mode.tmp) {
		mode_textnum_display(PSTR("hours"), 12);
	    } else {
		mode_textnum_display(PSTR("hours"), 24);
	    }
	    break;
	default:
	    break;
    }

    mode.timer = 0;
    mode.state = new_state;
}


// updates the time display every second
void mode_time_display(uint8_t hour, uint8_t minute,
		       uint8_t second) {
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

    // set or clear am or pm indicator
    if(time.status & TIME_12HOUR) {
	display_dot(0, hour >= 12);  // left-most circle if pm
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


// displays current alarm time
void mode_zone_display(void) {
    int8_t hour_to_display = mode.tmp[MODE_TMP_HOUR];
    display_clear(0);
    if(hour_to_display < 0) {
	display_char(1, '-');
	hour_to_display *= -1;
    } else {
	display_clear(1);
    }
    display_digit(2, hour_to_display / 10);
    display_digit(3, hour_to_display % 10);
    display_char(4, 'h');
    display_clear(5);
    display_digit(6, mode.tmp[MODE_TMP_MINUTE] / 10);
    display_digit(7, mode.tmp[MODE_TMP_MINUTE] % 10);
    display_char(8, 'm');
}


// displays text with two-digit positive number or one-digit
// negative number, with number selected
void mode_textnum_display(PGM_P pstr, int8_t num) {
    display_pstr(0, pstr);

    if(num < 0) {
	display_char(7, '-');
	num *= -1;
    } else {
	display_digit(7, num / 10);
    }

    display_digit(8, num % 10);

    display_dotselect(7, 8);
}


// displays current day of the week
void mode_dayofweek_display(void) {
    switch(time_dayofweek(time.year, time.month, time.day)) {
	case TIME_SUN:
	    display_pstr(0, PSTR(" sunday"));
	    break;
	case TIME_MON:
	    display_pstr(0, PSTR(" monday"));
	    break;
	case TIME_TUE:
	    display_pstr(0, PSTR("tuesday"));
	    break;
	case TIME_WED:
	    display_pstr(0, PSTR("wednsday"));
	    break;
	case TIME_THU:
	    display_pstr(0, PSTR("thursday"));
	    break;
	case TIME_FRI:
	    display_pstr(0, PSTR(" friday"));
	    break;
	case TIME_SAT:
	    display_pstr(0, PSTR("saturday"));
	    break;
	default:
	    break;
    }
}


// displays current month and day
void mode_monthday_display(void) {
    display_clear(0);
    display_clear(1);
    display_pstr(2, time_month2pstr(time.month));
    display_clear(5);
    display_digit(6, time.day / 10);
    display_digit(7, time.day % 10);
}

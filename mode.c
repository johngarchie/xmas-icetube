// mode.c  --  time display and user interaction
//
// Time display and menu configuration are implemented through various
// modes or states within a finite state machine.
//


#include <avr/pgmspace.h> // for defining program memory strings with PSTR()
#include <util/atomic.h>  // for defining non-interruptable blocks
#include <stdio.h>        // for using the NULL pointer macro

#include "mode.h"
#include "system.h"   // for system.initial_mcusr
#include "display.h"  // for setting display contents
#include "time.h"     // for displaying and setting time and date
#include "alarm.h"    // for setting and displaying alarm status
#include "piezo.h"    // for making clicks and alarm sounds
#include "buttons.h"  // for processing button presses
#include "gps.h"      // for setting the utc offset
#include "usart.h"    // for debugging output


// extern'ed clock mode data
volatile mode_t mode;


// private function declarations
void mode_update(uint8_t new_state, uint8_t disp_trans);
void mode_zone_display(void);
void mode_time_display(void);
void mode_settime_display(uint8_t hour, uint8_t minute, uint8_t second);
void mode_alarm_display(uint8_t hour, uint8_t minute);
void mode_textnum_display(PGM_P pstr, int8_t num);
void mode_texttext_display(PGM_P txt, PGM_P opt);
void mode_monthday_display(void);
void mode_daysofweek_display(uint8_t days);
void mode_menu_process_button(uint8_t up, uint8_t next, uint8_t down,
			      void (*init_func)(void), uint8_t btn,
			      uint8_t next_is_up);


// set default startup mode after system reset
void mode_init() {
    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_INSTANT);
}


// called each second; updates current mode as required
void mode_tick(void) {
    if(mode.state == MODE_TIME_DISPLAY) {
	// update time display for each tick of the clock
	if(time.status & TIME_UNSET && time.second % 2) {
	   if(system.initial_mcusr & _BV(WDRF)) {
	       display_pstr(0, PSTR("wdt rset"));
	   } else if(system.initial_mcusr & _BV(EXTRF)) {
	       display_pstr(0, PSTR("ext rset"));
	   } else if(system.initial_mcusr & _BV(PORF)) {
	       display_pstr(0, PSTR("pwr rset"));
	   } else if(system.initial_mcusr & _BV(BORF)) {
	       display_pstr(0, PSTR("bod rset"));
	   } else {
	       display_pstr(0, PSTR("oth rset"));
	   }
	   display_transition(DISPLAY_TRANS_INSTANT);
#ifdef GPS_TIMEKEEPING
	} else if(gps.data_timer && !gps.warn_timer && time.second % 2) {
	    display_pstr(0, PSTR("gps lost"));
	    display_transition(DISPLAY_TRANS_INSTANT);
#endif  // GPS_TIMEKEEPING
	} else {
	    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_INSTANT);
	}
    }
}


// called each semisecond; updates current mode as required
void mode_semitick(void) {
    // ignore buttons unless transition is finished
    if(display.trans_type != DISPLAY_TRANS_NONE) return;

    uint8_t btn = buttons_process();

    // enable snooze and ensure visible display on button press
    if(btn) {
	uint8_t a = display_onbutton();
	uint8_t b = alarm_onbutton();
	if(a || b) btn = 0;
    }

    switch(mode.state) {
	case MODE_TIME_DISPLAY:
	    // check for button presses
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_SETALARM_MENU, DISPLAY_TRANS_UP);
		    break;
		case BUTTONS_PLUS:
		case BUTTONS_SET:
		    if(time.dateformat & TIME_DATEFORMAT_SHOWWDAY) {
			mode_update(MODE_DAYOFWEEK_DISPLAY, DISPLAY_TRANS_DOWN);
		    } else {
			mode_update(MODE_MONTHDAY_DISPLAY, DISPLAY_TRANS_DOWN);
		    }
		    break;
		default:
		    break;
	    }
	    return;  // no timout; skip code below
	case MODE_DAYOFWEEK_DISPLAY:
	    if(btn || ++mode.timer > 1250) {
		mode_update(MODE_MONTHDAY_DISPLAY, DISPLAY_TRANS_LEFT);
	    }
	    return;  // time ourselves; skip code below
	case MODE_MONTHDAY_DISPLAY:
	    if(btn || ++mode.timer > 1250) {
		if(time.dateformat & TIME_DATEFORMAT_SHOWYEAR) {
		    mode_update(MODE_YEAR_DISPLAY, DISPLAY_TRANS_LEFT);
		} else {
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
		}
	    }
	    return;  // time ourselves; skip code below
	case MODE_YEAR_DISPLAY:
	    if(btn || ++mode.timer > 1250) {
		mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
	    }
	    return;  // time ourselves; skip code below
	case MODE_ALARMSET_DISPLAY:
	    if(btn || ++mode.timer > 1250) {
		*mode.tmp = 0;
		mode_update(MODE_ALARMIDX_DISPLAY, DISPLAY_TRANS_LEFT);
	    }
	    return;  // time ourselves; skip code below
	case MODE_ALARMIDX_DISPLAY:
	    if(btn || ++mode.timer > 1250) {
		mode_update(MODE_ALARMTIME_DISPLAY, DISPLAY_TRANS_LEFT);
	    }
	    return;  // time ourselves; skip code below
	case MODE_ALARMTIME_DISPLAY:
	    if(btn || ++mode.timer > 1250) {
		if(alarm.days[*mode.tmp] & ALARM_ENABLED) {
		    mode_update(MODE_ALARMDAYS_DISPLAY, DISPLAY_TRANS_LEFT);
		} else if(++(*mode.tmp) < ALARM_COUNT) {
		    mode_update(MODE_ALARMIDX_DISPLAY, DISPLAY_TRANS_LEFT);
		} else {
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
		}
	    }
	    return;  // time ourselves; skip code below
	case MODE_ALARMDAYS_DISPLAY:
	    if(btn || ++mode.timer > 1250) {
		if(++(*mode.tmp) < ALARM_COUNT) {
		    mode_update(MODE_ALARMIDX_DISPLAY, DISPLAY_TRANS_LEFT);
		} else {
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
		}
	    }
	    return;  // time ourselves; skip code below
	case MODE_ALARMOFF_DISPLAY:
	    if(btn || ++mode.timer > 1250) {
		mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
	    }
	    return;  // time ourselves; skip code below
	case MODE_SNOOZEON_DISPLAY:
	    if(btn || ++mode.timer > 5250) {
		mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
	    }
	    return;  // time ourselves; skip code below
	case MODE_SETALARM_MENU: ;
	    void menu_setalarm_init(void) { *mode.tmp = 0; }

	    mode_menu_process_button(
		MODE_TIME_DISPLAY, 
#ifdef GPS_TIMEKEEPING
		(gps.status & GPS_SIGNAL_GOOD ? MODE_CFGALARM_MENU
		 			      : MODE_SETTIME_MENU),
#else
		MODE_SETTIME_MENU,
#endif  // GPS_TIMEKEEPING
		MODE_SETALARM_IDX,
	        menu_setalarm_init, btn, FALSE);
	    break;
	case MODE_SETALARM_IDX:
	    switch(btn) {
#ifdef ADAFRUIT_BUTTONS
		case BUTTONS_PLUS:
#else
		case BUTTONS_MENU:
#endif
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    mode_update(MODE_SETALARM_ENABLE, DISPLAY_TRANS_UP);
		    break;
#ifdef ADAFRUIT_BUTTONS
		case BUTTONS_MENU:
		    if(*mode.tmp == ALARM_COUNT - 1) {
			mode_update(MODE_SETALARM_MENU, DISPLAY_TRANS_DOWN);
			break;
		    }
#else
		case BUTTONS_PLUS:
#endif
		    ++(*mode.tmp);
		    *mode.tmp %= ALARM_COUNT;
		    mode_update(MODE_SETALARM_IDX, DISPLAY_TRANS_LEFT);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETALARM_ENABLE:
	    switch(btn) {
		case BUTTONS_MENU:
		    alarm_loadalarm(*mode.tmp);
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    if(alarm.days[*mode.tmp] & ALARM_ENABLED) {
			mode_update(MODE_SETALARM_HOUR, DISPLAY_TRANS_UP);
		    } else {
			alarm_savealarm(*mode.tmp);
			mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
		    }
		    break;
		case BUTTONS_PLUS:
		    if(alarm.days[*mode.tmp] & ALARM_ENABLED) {
			alarm.days[*mode.tmp] &= ~ALARM_ENABLED;
		    } else {
			alarm.days[*mode.tmp] |= ALARM_ENABLED;
		    }
		    mode_update(MODE_SETALARM_ENABLE, DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) {
			alarm_loadalarm(*mode.tmp);
		    }
		    break;
	    }
	    break;
	case MODE_SETALARM_HOUR:
	    switch(btn) {
		case BUTTONS_MENU:
		    alarm_loadalarm(*mode.tmp);
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    mode_update(MODE_SETALARM_MINUTE, DISPLAY_TRANS_INSTANT);
		    break;
		case BUTTONS_PLUS:
		    ++alarm.hours[*mode.tmp];
		    alarm.hours[*mode.tmp] %= 24;
		    mode_update(MODE_SETALARM_HOUR, DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) {
			alarm_loadalarm(*mode.tmp);
		    }
		    break;
	    }
	    break;
	case MODE_SETALARM_MINUTE:
	    switch(btn) {
		case BUTTONS_MENU:
		    alarm_loadalarm(*mode.tmp);
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    if(alarm.days[*mode.tmp] == ALARM_ENABLED) {
			mode.tmp[MODE_TMP_DAYS] = TIME_ALLDAYS | ALARM_ENABLED;
		    } else {
			mode.tmp[MODE_TMP_DAYS] = alarm.days[*mode.tmp];
		    }
		    mode_update(MODE_SETALARM_DAYS_OPTIONS, DISPLAY_TRANS_UP);
		    break;
		case BUTTONS_PLUS:
		    ++alarm.minutes[*mode.tmp];
		    alarm.minutes[*mode.tmp] %= 60;
		    mode_update(MODE_SETALARM_MINUTE, DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) {
			alarm_loadalarm(*mode.tmp);
		    }
		    break;
	    }
	    break;
	case MODE_SETALARM_DAYS_OPTIONS:
	    switch(btn) {
		case BUTTONS_MENU:
		    alarm_loadalarm(*mode.tmp);
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    switch((uint8_t)mode.tmp[MODE_TMP_DAYS]) {
			case TIME_ALLDAYS | ALARM_ENABLED:
			case TIME_WEEKDAYS | ALARM_ENABLED:
			case TIME_WEEKENDS | ALARM_ENABLED:
			    alarm.days[*mode.tmp] = mode.tmp[MODE_TMP_DAYS];
			    alarm_savealarm(*mode.tmp);
			    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
			    break;
			case ALARM_ENABLED:
			    mode.tmp[MODE_TMP_DAYS]
				= TIME_ALLDAYS | ALARM_ENABLED;
			    mode.tmp[MODE_TMP_IDX] = 0;
			    mode_update(MODE_SETALARM_DAYS_CUSTOM,
				        DISPLAY_TRANS_UP);
			    break;
			default:
			    mode.tmp[MODE_TMP_DAYS] = alarm.days[*mode.tmp];
			    mode.tmp[MODE_TMP_IDX] = 0;
			    mode_update(MODE_SETALARM_DAYS_CUSTOM,
				        DISPLAY_TRANS_UP);
			    break;
		    }
		    break;
		case BUTTONS_PLUS:
		    switch((uint8_t)mode.tmp[MODE_TMP_DAYS]) {
			case TIME_ALLDAYS | ALARM_ENABLED:
		            mode.tmp[MODE_TMP_DAYS]
				= TIME_WEEKDAYS | ALARM_ENABLED;
			    break;
			case TIME_WEEKDAYS | ALARM_ENABLED:
		            mode.tmp[MODE_TMP_DAYS]
				= TIME_WEEKENDS | ALARM_ENABLED;
			    break;
			case TIME_WEEKENDS | ALARM_ENABLED:
		            mode.tmp[MODE_TMP_DAYS] = ALARM_ENABLED;
			    break;
			default:
		            mode.tmp[MODE_TMP_DAYS]
				= TIME_ALLDAYS | ALARM_ENABLED;
			    break;
		    }
		    mode_update(MODE_SETALARM_DAYS_OPTIONS,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) {
			alarm_loadalarm(*mode.tmp);
		    }
		    break;
	    }
	    break;
	case MODE_SETALARM_DAYS_CUSTOM:
	    switch(btn) {
		case BUTTONS_MENU:
		    alarm_loadalarm(*mode.tmp);
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    if(mode.tmp[MODE_TMP_IDX] < TIME_SAT) {
			++mode.tmp[MODE_TMP_IDX];
			mode_update(MODE_SETALARM_DAYS_CUSTOM,
				    DISPLAY_TRANS_INSTANT);
		    } else {
			alarm.days[*mode.tmp] = mode.tmp[MODE_TMP_DAYS];
			alarm_savealarm(*mode.tmp);
			mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
		    }
		    break;
		case BUTTONS_PLUS:
		    mode.tmp[MODE_TMP_DAYS] ^= _BV(mode.tmp[MODE_TMP_IDX]);
		    mode_update(MODE_SETALARM_DAYS_CUSTOM,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) {
			alarm_loadalarm(*mode.tmp);
		    }
		    break;
	    }
	    break;
	case MODE_SETTIME_MENU: ;
	    void menu_settime_init(void) {
		mode.tmp[MODE_TMP_HOUR]   = time.hour;
		mode.tmp[MODE_TMP_MINUTE] = time.minute;
		mode.tmp[MODE_TMP_SECOND] = time.second;
	    }

	    mode_menu_process_button(
		    MODE_TIME_DISPLAY,
		    MODE_SETDATE_MENU,
		    MODE_SETTIME_HOUR,
		    menu_settime_init,
		    btn, FALSE);
	    break;
	case MODE_SETTIME_HOUR:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    mode_update(MODE_SETTIME_MINUTE, DISPLAY_TRANS_INSTANT);
		    break;
		case BUTTONS_PLUS:
		    ++mode.tmp[MODE_TMP_HOUR];
		    mode.tmp[MODE_TMP_HOUR] %= 24;
		    mode_update(MODE_SETTIME_HOUR, DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETTIME_MINUTE:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    mode_update(MODE_SETTIME_SECOND, DISPLAY_TRANS_INSTANT);
		    break;
		case BUTTONS_PLUS:
		    ++mode.tmp[MODE_TMP_MINUTE];
		    mode.tmp[MODE_TMP_MINUTE] %= 60;
		    mode_update(MODE_SETTIME_MINUTE, DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETTIME_SECOND:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    ATOMIC_BLOCK(ATOMIC_FORCEON) {
			time_settime(mode.tmp[MODE_TMP_HOUR],
				     mode.tmp[MODE_TMP_MINUTE],
				     mode.tmp[MODE_TMP_SECOND]);
			time_autodst(FALSE);
		    }
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
		    break;
		case BUTTONS_PLUS:
		    ++mode.tmp[MODE_TMP_SECOND];
		    mode.tmp[MODE_TMP_SECOND] %= 60;
		    mode_update(MODE_SETTIME_SECOND, DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETDATE_MENU: ;
	    void menu_setdate_init(void) {
		mode.tmp[MODE_TMP_YEAR]  = time.year;
		mode.tmp[MODE_TMP_MONTH] = time.month;
		mode.tmp[MODE_TMP_DAY]   = time.day;
	    }

	    mode_menu_process_button(
		    MODE_TIME_DISPLAY,
		    MODE_CFGALARM_MENU,
		    MODE_SETDATE_YEAR,
		    menu_setdate_init,
		    btn, FALSE);
	    break;
	case MODE_SETDATE_YEAR:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    mode_update(MODE_SETDATE_MONTH, DISPLAY_TRANS_INSTANT);
		    break;
		case BUTTONS_PLUS:
		    if(++mode.tmp[MODE_TMP_YEAR] > 50) {
		        mode.tmp[MODE_TMP_YEAR] = 10;
		    }
		    mode_update(MODE_SETDATE_YEAR, DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETDATE_MONTH:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    mode_update(MODE_SETDATE_DAY, DISPLAY_TRANS_LEFT);
		    break;
		case BUTTONS_PLUS:
		    ++mode.tmp[MODE_TMP_MONTH];
		    if(mode.tmp[MODE_TMP_MONTH] > 12) {
			mode.tmp[MODE_TMP_MONTH] = 1;
		    }
		    mode_update(MODE_SETDATE_MONTH, DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETDATE_DAY:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    ATOMIC_BLOCK(ATOMIC_FORCEON) {
			time_setdate(mode.tmp[MODE_TMP_YEAR],
				     mode.tmp[MODE_TMP_MONTH],
				     mode.tmp[MODE_TMP_DAY]);
			time_autodst(FALSE);
		    }
		    time_savedate();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
		    break;
		case BUTTONS_PLUS:
		    if(++mode.tmp[MODE_TMP_DAY]
			    > time_daysinmonth(mode.tmp[MODE_TMP_YEAR],
					       mode.tmp[MODE_TMP_MONTH])) {
			mode.tmp[MODE_TMP_DAY] = 1;
		    }
		    mode_update(MODE_SETDATE_DAY, DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_CFGALARM_MENU:
	    mode_menu_process_button(
		    MODE_TIME_DISPLAY,
		    MODE_CFGDISP_MENU,
		    MODE_CFGALARM_SETSOUND_MENU,
		    NULL,
		    btn, FALSE);
	    break;
	case MODE_CFGALARM_SETSOUND_MENU: ;
	    void menu_cfgalarm_setsound_init(void) {
		piezo_setvolume((alarm.volume_min + alarm.volume_max) >> 1, 0);
		piezo_tryalarm_start();
	    }

	    mode_menu_process_button(
		    MODE_TIME_DISPLAY,
		    MODE_CFGALARM_SETVOL_MENU,
		    MODE_CFGALARM_SETSOUND,
		    menu_cfgalarm_setsound_init,
		    btn, FALSE);
	    break;
	case MODE_CFGALARM_SETSOUND:
	    switch(btn) {
		case BUTTONS_MENU:
		    piezo_tryalarm_stop();
		    piezo_loadsound();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    piezo_savesound();
		    piezo_tryalarm_stop();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
		    break;
		case BUTTONS_PLUS:
		    piezo_nextsound();
		    piezo_tryalarm_start();
		    mode_update(MODE_CFGALARM_SETSOUND, DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) {
			piezo_tryalarm_stop();
			piezo_loadsound();
		    }
		    break;
	    }
	    break;
	case MODE_CFGALARM_SETVOL_MENU: ;
	    void menu_cfgalarm_setvol_init(void) {
		if(alarm.volume_min != alarm.volume_max) {
		    *mode.tmp = 11;
		} else {
		    *mode.tmp = alarm.volume_min;
		    piezo_setvolume(alarm.volume_min, 0);
		    piezo_tryalarm_start();
		}
		mode.tmp[MODE_TMP_MIN] = alarm.volume_min;
		mode.tmp[MODE_TMP_MAX] = alarm.volume_max;
	    }

	    mode_menu_process_button(
		    MODE_TIME_DISPLAY,
		    MODE_CFGALARM_SETSNOOZE_MENU,
		    MODE_CFGALARM_SETVOL,
		    menu_cfgalarm_setvol_init,
		    btn, FALSE);
	    break;
	case MODE_CFGALARM_SETVOL:
	    switch(btn) {
		case BUTTONS_MENU:
		    piezo_tryalarm_stop();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    if(*mode.tmp < 11) {
			piezo_tryalarm_stop();
			piezo_setvolume(*mode.tmp, 0);
			alarm.volume_min = *mode.tmp;
			alarm.volume_max = *mode.tmp;
			alarm_savevolume();
			mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
		    } else {
			piezo_setvolume(mode.tmp[MODE_TMP_MIN], 0);
			piezo_tryalarm_start();
			mode_update(MODE_CFGALARM_SETVOL_MIN, DISPLAY_TRANS_UP);
		    }
		    break;
		case BUTTONS_PLUS:
		    ++(*mode.tmp);
		    *mode.tmp %= 12;

		    if(*mode.tmp < 11) {
			piezo_setvolume(*mode.tmp, 0);
			piezo_tryalarm_start();
		    } else {
			piezo_tryalarm_stop();
		    }

		    mode_update(MODE_CFGALARM_SETVOL, DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) piezo_tryalarm_stop();
		    break;
	    }
	    break;
	case MODE_CFGALARM_SETVOL_MIN:
	    switch(btn) {
		case BUTTONS_MENU:
		    piezo_tryalarm_stop();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    piezo_setvolume(mode.tmp[MODE_TMP_MAX], 0);
		    piezo_tryalarm_start();
		    mode_update(MODE_CFGALARM_SETVOL_MAX, DISPLAY_TRANS_UP);
		    break;
		case BUTTONS_PLUS:
		    ++mode.tmp[MODE_TMP_MIN];
		    mode.tmp[MODE_TMP_MIN] %= 10;
		    piezo_setvolume(mode.tmp[MODE_TMP_MIN], 0);
		    piezo_tryalarm_start();
		    mode_update(MODE_CFGALARM_SETVOL_MIN,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) piezo_tryalarm_stop();
		    break;
	    }
	    break;
	case MODE_CFGALARM_SETVOL_MAX:
	    switch(btn) {
		case BUTTONS_MENU:
		    piezo_tryalarm_stop();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    piezo_tryalarm_stop();
		    alarm.volume_min = mode.tmp[MODE_TMP_MIN];
		    alarm.volume_max = mode.tmp[MODE_TMP_MAX];
		    alarm_savevolume();
		    *mode.tmp = alarm.ramp_time;
		    mode_update(MODE_CFGALARM_SETVOL_TIME, DISPLAY_TRANS_UP);
		    break;
		case BUTTONS_PLUS:
		    ++mode.tmp[MODE_TMP_MAX];
		    if(mode.tmp[MODE_TMP_MAX] > 10) {
			mode.tmp[MODE_TMP_MAX] = mode.tmp[MODE_TMP_MIN] + 1;
		    }
		    piezo_setvolume(mode.tmp[MODE_TMP_MAX], 0);
		    piezo_tryalarm_start();
		    mode_update(MODE_CFGALARM_SETVOL_MAX,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) piezo_tryalarm_stop();
		    break;
	    }
	    break;
	case MODE_CFGALARM_SETVOL_TIME:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    alarm.ramp_time = *mode.tmp;
		    alarm_newramp();  // calculate alarm.ramp_int
		    alarm_saveramp(); // save alarm.ramp_time
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
		    break;
		case BUTTONS_PLUS:
		    if(++(*mode.tmp) > 60) *mode.tmp = 1;
		    mode_update(MODE_CFGALARM_SETVOL_TIME,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_CFGALARM_SETSNOOZE_MENU: ;
	    void menu_cfgalarm_setsnooze_init(void) {
		*mode.tmp = alarm.snooze_time / 60;
	    };

	    mode_menu_process_button(
		    MODE_TIME_DISPLAY,
		    MODE_CFGALARM_SETHEARTBEAT_MENU,
		    MODE_CFGALARM_SETSNOOZE_TIME,
		    menu_cfgalarm_setsnooze_init,
		    btn, FALSE);
	    break;
	case MODE_CFGALARM_SETSNOOZE_TIME:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    alarm.snooze_time = *mode.tmp * 60;
		    alarm_savesnooze();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
		    break;
		case BUTTONS_PLUS:
		    ++(*mode.tmp); *mode.tmp %= 31;
		    mode_update(MODE_CFGALARM_SETSNOOZE_TIME,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_CFGALARM_SETHEARTBEAT_MENU: ;
	    void menu_cfgalarm_setheartbeat_init(void) {
		*mode.tmp = alarm.status;

		if(*mode.tmp & ALARM_SOUNDING_PULSE) {
		    display.status |=  DISPLAY_PULSING;
		} else {
		    display.status &= ~DISPLAY_PULSING;
		    display_autodim();
		}
	    };

	    mode_menu_process_button(
		    MODE_TIME_DISPLAY,
#ifdef ADAFRUIT_BUTTONS
		    MODE_CFGALARM_MENU,
#else
		    MODE_CFGALARM_SETSOUND_MENU,
#endif  // ADAFRUIT_BUTTONS
		    MODE_CFGALARM_SETHEARTBEAT_TOGGLE,
		    menu_cfgalarm_setheartbeat_init,
		    btn, TRUE);
	    break;
	case MODE_CFGALARM_SETHEARTBEAT_TOGGLE:
	    switch(btn) {
		case BUTTONS_MENU:
		    display.status &= ~DISPLAY_PULSING;
		    display_autodim();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    display.status &= ~DISPLAY_PULSING;
		    display_autodim();
		    if(*mode.tmp & ALARM_SOUNDING_PULSE) {
			alarm.status |=  ALARM_SOUNDING_PULSE;
			alarm.status |=  ALARM_SNOOZING_PULSE;
		    } else {
			alarm.status &= ~ALARM_SOUNDING_PULSE;
			alarm.status &= ~ALARM_SNOOZING_PULSE;
		    }
		    alarm_savestatus();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
		    break;
		case BUTTONS_PLUS:
		    if(*mode.tmp & ALARM_SOUNDING_PULSE) {
			*mode.tmp      &= ~ALARM_SOUNDING_PULSE;
			display.status &= ~DISPLAY_PULSING;
			display_autodim();
		    } else {
			*mode.tmp      |= ALARM_SOUNDING_PULSE;
			display.status |= DISPLAY_PULSING;
		    }
		    mode_update(MODE_CFGALARM_SETHEARTBEAT_TOGGLE,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) {
			display.status &= ~DISPLAY_PULSING;
			display_autodim();
		    }
		    break;
	    }
	    break;
	case MODE_CFGDISP_MENU:
	    mode_menu_process_button(
		    MODE_TIME_DISPLAY,
		    MODE_CFGREGN_MENU,
		    MODE_CFGDISP_SETBRIGHT_MENU,
		    NULL,
		    btn, FALSE);
	    break;
	case MODE_CFGDISP_SETBRIGHT_MENU: ;
	    void menu_cfgdisp_setbright_init(void) {
#ifdef AUTOMATIC_DIMMER
		mode.tmp[MODE_TMP_MIN] = display.bright_min;
		mode.tmp[MODE_TMP_MAX] = display.bright_max;

		if(display.bright_min == display.bright_max) {
		    *mode.tmp = display.bright_min;
		} else {
		    *mode.tmp = 11;
		}
#endif  // AUTOMATIC_DIMMER
	    }

	    mode_menu_process_button(
		    MODE_TIME_DISPLAY,
		    MODE_CFGDISP_SETDIGITBRIGHT_MENU,
		    MODE_CFGDISP_SETBRIGHT_LEVEL,
		    menu_cfgdisp_setbright_init,
		    btn, FALSE);
	    break;
	case MODE_CFGDISP_SETBRIGHT_LEVEL:
	    switch(btn) {
		case BUTTONS_MENU:
		    display_loadbright();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
#ifdef AUTOMATIC_DIMMER
		    if(*mode.tmp == 11) {
			*mode.tmp = mode.tmp[MODE_TMP_MIN];
			mode_update(MODE_CFGDISP_SETBRIGHT_MIN,
				    DISPLAY_TRANS_UP);
		    } else {
			display_savebright();
			mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
		    }
#else
		    display_savebright();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
#endif  // AUTOMATIC_DIMMER
		    break;
		case BUTTONS_PLUS:
#ifdef AUTOMATIC_DIMMER
		    ++(*mode.tmp);
		    *mode.tmp %= 12;
#else
		    ++display.brightness;
		    display.brightness %= 11;
#endif  // AUTOMATIC_DIMMER
		    mode_update(MODE_CFGDISP_SETBRIGHT_LEVEL,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) {
			display_loadbright();
		    }
		    break;
	    }
	    break;
#ifdef AUTOMATIC_DIMMER
	case MODE_CFGDISP_SETBRIGHT_MIN:
	    switch(btn) {
		case BUTTONS_MENU:
		    display_loadbright();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
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
		    mode_update(MODE_CFGDISP_SETBRIGHT_MAX, DISPLAY_TRANS_UP);
		    break;
		case BUTTONS_PLUS:
		    if(++(*mode.tmp) > 9) *mode.tmp = -5;
		    mode_update(MODE_CFGDISP_SETBRIGHT_MIN,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) {
			display_loadbright();
		    }
		    break;
	    }
	    break;
	case MODE_CFGDISP_SETBRIGHT_MAX:
	    switch(btn) {
		case BUTTONS_MENU:
		    display_loadbright();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    display.bright_min = mode.tmp[MODE_TMP_MIN];
		    display.bright_max = *mode.tmp;
		    display_savebright();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
		    break;
		case BUTTONS_PLUS:
		    if(++(*mode.tmp) > 20) {
		        if(mode.tmp[MODE_TMP_MIN] < 1) {
			    *mode.tmp = 1;
			} else {
			    *mode.tmp = mode.tmp[MODE_TMP_MIN] + 1;
			}
		    }

		    mode_update(MODE_CFGDISP_SETBRIGHT_MAX,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) {
			display_loadbright();
		    }
		    break;
	    }
	    break;
#endif  // AUTOMATIC_DIMMER
	case MODE_CFGDISP_SETDIGITBRIGHT_MENU: ;
	    void menu_cfgdisp_setdigitbright_init(void) {
		*mode.tmp = 0;
	    }

	    mode_menu_process_button(
		    MODE_TIME_DISPLAY,
		    MODE_CFGDISP_SETAUTOOFF_MENU,
		    MODE_CFGDISP_SETDIGITBRIGHT_LEVEL,
		    menu_cfgdisp_setdigitbright_init,
		    btn, FALSE);
	    break;
	case MODE_CFGDISP_SETDIGITBRIGHT_LEVEL:
	    switch(btn) {
		case BUTTONS_MENU:
		    display_loaddigittimes();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    if(++(*mode.tmp) < DISPLAY_SIZE) {
			mode_update(MODE_CFGDISP_SETDIGITBRIGHT_LEVEL,
				    DISPLAY_TRANS_INSTANT);
		    } else {
			mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
		    }
		    break;
		case BUTTONS_PLUS:
		    if(display.digit_times[*mode.tmp] < 240) {
			display.digit_times[*mode.tmp] +=
					display.digit_times[*mode.tmp] / 5;
		    } else {
			display.digit_times[*mode.tmp] = 15;
		    }

		    display_noflicker();

		    mode_update(MODE_CFGDISP_SETDIGITBRIGHT_LEVEL,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) {
			display_loaddigittimes();
		    }
		    break;
	    }
	    break;
	case MODE_CFGDISP_SETAUTOOFF_MENU:
	    mode_menu_process_button(
		    MODE_TIME_DISPLAY,
		    MODE_CFGDISP_SETANIMATED_MENU,
#ifdef AUTOMATIC_DIMMER
		    MODE_CFGDISP_SETPHOTOOFF_MENU,
#else
		    MODE_CFGDISP_SETOFFTIME_MENU,
#endif
		    NULL, btn, FALSE);
	    break;
#ifdef AUTOMATIC_DIMMER
	case MODE_CFGDISP_SETPHOTOOFF_MENU: ;
	    void menu_cfgdisp_setphotooff_init(void) {
		uint8_t off_thr = display.off_threshold;
		for(*mode.tmp = 0; off_thr; off_thr >>= 1) ++(*mode.tmp);
	    }

	    mode_menu_process_button(
		    MODE_TIME_DISPLAY,
		    MODE_CFGDISP_SETOFFTIME_MENU,
		    MODE_CFGDISP_SETPHOTOOFF_THRESH,
		    menu_cfgdisp_setphotooff_init,
		    btn, FALSE);
	    break;
	case MODE_CFGDISP_SETPHOTOOFF_THRESH:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    display.off_threshold = (*mode.tmp ? (1<<(*mode.tmp-1)) : 0);
		    display_savephotooff();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
		    break;
		case BUTTONS_PLUS:
		    ++(*mode.tmp);
		    if(*mode.tmp > 8) *mode.tmp = 0;
		    mode_update(MODE_CFGDISP_SETPHOTOOFF_THRESH,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    break;
	    }
	    break;
#endif  // AUTOMATIC_DIMMER
	case MODE_CFGDISP_SETOFFTIME_MENU: ;
	    void menu_cfgdisp_setofftime_init(void) {
		*mode.tmp = display.off_hour & _BV(TIME_NODAY);
	    }

	    mode_menu_process_button(
		    MODE_TIME_DISPLAY,
		    MODE_CFGDISP_SETOFFDAYS_MENU,
		    MODE_CFGDISP_SETOFFTIME_TOGGLE,
		    menu_cfgdisp_setofftime_init,
		    btn, FALSE);
	    break;
	case MODE_CFGDISP_SETOFFTIME_TOGGLE:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    if(*mode.tmp) {
			display.off_hour |= _BV(TIME_NODAY);
			display_saveofftime();
			mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    } else {
			display.off_hour &= ~_BV(TIME_NODAY);
			mode_update(MODE_CFGDISP_SETOFFTIME_OFFHOUR,
				    DISPLAY_TRANS_DOWN);
		    }
		    break;
		case BUTTONS_PLUS:
		    *mode.tmp ^= _BV(TIME_NODAY);
		    mode_update(MODE_CFGDISP_SETOFFTIME_TOGGLE,
				DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_CFGDISP_SETOFFTIME_OFFHOUR:
	    switch(btn) {
		case BUTTONS_MENU:
		    display_loadofftime();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    mode_update(MODE_CFGDISP_SETOFFTIME_OFFMINUTE,
				DISPLAY_TRANS_INSTANT);
		    break;
		case BUTTONS_PLUS:
		    ++display.off_hour;
		    display.off_hour %= 24;
		    mode_update(MODE_CFGDISP_SETOFFTIME_OFFHOUR,
				DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) {
			display_loadofftime();
		    }
		    break;
	    }
	    break;
	case MODE_CFGDISP_SETOFFTIME_OFFMINUTE:
	    switch(btn) {
		case BUTTONS_MENU:
		    display_loadofftime();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    mode_update(MODE_CFGDISP_SETOFFTIME_ONHOUR,
				DISPLAY_TRANS_UP);
		    break;
		case BUTTONS_PLUS:
		    ++display.off_minute;
		    display.off_minute %= 60;
		    mode_update(MODE_CFGDISP_SETOFFTIME_OFFMINUTE,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) {
			display_loadofftime();
		    }
		    break;
	    }
	    break;
	case MODE_CFGDISP_SETOFFTIME_ONHOUR:
	    switch(btn) {
		case BUTTONS_MENU:
		    display_loadofftime();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    mode_update(MODE_CFGDISP_SETOFFTIME_ONMINUTE,
				DISPLAY_TRANS_INSTANT);
		    break;
		case BUTTONS_PLUS:
		    ++display.on_hour;
		    display.on_hour %= 24;
		    mode_update(MODE_CFGDISP_SETOFFTIME_ONHOUR,
				DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) {
			display_loadofftime();
		    }
		    break;
	    }
	    break;
	case MODE_CFGDISP_SETOFFTIME_ONMINUTE:
	    switch(btn) {
		case BUTTONS_MENU:
		    display_loadofftime();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    display_saveofftime();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
		    break;
		case BUTTONS_PLUS:
		    ++display.on_minute;
		    display.on_minute %= 60;
		    mode_update(MODE_CFGDISP_SETOFFTIME_ONMINUTE,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) {
			display_loadofftime();
		    }
		    break;
	    }
	    break;
	case MODE_CFGDISP_SETOFFDAYS_MENU: ;
	    void menu_cfgdisp_setoffdays_init(void) {
		*mode.tmp = display.off_days;
	    }

	    mode_menu_process_button(
		    MODE_TIME_DISPLAY,
		    MODE_CFGDISP_SETONDAYS_MENU,
		    MODE_CFGDISP_SETOFFDAYS_OPTIONS,
		    menu_cfgdisp_setoffdays_init,
		    btn, FALSE);
	    break;
	case MODE_CFGDISP_SETOFFDAYS_OPTIONS:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    switch((uint8_t)*mode.tmp) {
			case TIME_NODAYS:
			case TIME_ALLDAYS:
			case TIME_WEEKDAYS:
			case TIME_WEEKENDS:
			    display.off_days = *mode.tmp;
			    display_saveoffdays();
			    display.on_days &= ~display.off_days;
			    display_saveondays();
			    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
			    break;
			default:
			    mode.tmp[MODE_TMP_IDX] = TIME_SUN;
			    *mode.tmp = display.off_days;
			    if(!*mode.tmp) *mode.tmp = TIME_ALLDAYS;
			    mode_update(MODE_CFGDISP_SETOFFDAYS_CUSTOM,
				        DISPLAY_TRANS_UP);
			    break;
		    }
		    break;
		case BUTTONS_PLUS:
		    switch((uint8_t)*mode.tmp) {
			case TIME_NODAYS:
			    *mode.tmp = TIME_ALLDAYS;
			    break;
			case TIME_ALLDAYS:
			    *mode.tmp = TIME_WEEKDAYS;
			    break;
			case TIME_WEEKDAYS:
			    *mode.tmp = TIME_WEEKENDS;
			    break;
			case TIME_WEEKENDS:
			    *mode.tmp = _BV(TIME_NODAY);
			    break;
			default:
			    *mode.tmp = TIME_NODAYS;
			    break;
		    }

		    mode_update(MODE_CFGDISP_SETOFFDAYS_OPTIONS,
				DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_CFGDISP_SETOFFDAYS_CUSTOM:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    if(mode.tmp[MODE_TMP_IDX] < TIME_SAT) {
			++mode.tmp[MODE_TMP_IDX];
			mode_update(MODE_CFGDISP_SETOFFDAYS_CUSTOM,
				    DISPLAY_TRANS_INSTANT);
		    } else {
			display.off_days = *mode.tmp;
			display_saveoffdays();
			display.on_days &= ~display.off_days;
			display_saveondays();
			mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
		    }
		    break;
		case BUTTONS_PLUS:
		    *mode.tmp ^= _BV(mode.tmp[MODE_TMP_IDX]);
		    mode_update(MODE_CFGDISP_SETOFFDAYS_CUSTOM,
				DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_CFGDISP_SETONDAYS_MENU: ;
	    void menu_cfgdisp_setondays_init(void) {
		*mode.tmp = display.on_days;
	    }

	    mode_menu_process_button(
		    MODE_TIME_DISPLAY,
#ifdef AUTOMATIC_DIMMER
		    MODE_CFGDISP_SETPHOTOOFF_MENU,
#else
		    MODE_CFGDISP_SETOFFTIME_MENU,
#endif  // AUTOMATIC_DIMMER
		    MODE_CFGDISP_SETONDAYS_OPTIONS,
		    menu_cfgdisp_setondays_init,
		    btn, FALSE);
	    break;
	case MODE_CFGDISP_SETONDAYS_OPTIONS:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    switch((uint8_t)*mode.tmp) {
			case TIME_NODAYS:
			case TIME_ALLDAYS:
			case TIME_WEEKDAYS:
			case TIME_WEEKENDS:
			    display.on_days = *mode.tmp;
			    display_saveondays();
			    display.off_days &= ~display.on_days;
			    display_saveoffdays();
			    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
			    break;
			default:
			    mode.tmp[MODE_TMP_IDX] = TIME_SUN;
			    *mode.tmp = display.on_days;
			    if(!*mode.tmp) *mode.tmp = TIME_ALLDAYS;
			    mode_update(MODE_CFGDISP_SETONDAYS_CUSTOM,
				        DISPLAY_TRANS_UP);
			    break;
		    }
		    break;
		case BUTTONS_PLUS:
		    switch((uint8_t)*mode.tmp) {
			case TIME_NODAYS:
			    *mode.tmp = TIME_ALLDAYS;
			    break;
			case TIME_ALLDAYS:
			    *mode.tmp = TIME_WEEKDAYS;
			    break;
			case TIME_WEEKDAYS:
			    *mode.tmp = TIME_WEEKENDS;
			    break;
			case TIME_WEEKENDS:
			    *mode.tmp = _BV(TIME_NODAY);
			    break;
			default:
			    *mode.tmp = TIME_NODAYS;
			    break;
		    }

		    mode_update(MODE_CFGDISP_SETONDAYS_OPTIONS,
				DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_CFGDISP_SETONDAYS_CUSTOM:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    if(mode.tmp[MODE_TMP_IDX] < TIME_SAT) {
			++mode.tmp[MODE_TMP_IDX];
			mode_update(MODE_CFGDISP_SETONDAYS_CUSTOM,
				    DISPLAY_TRANS_INSTANT);
		    } else {
			display.on_days = *mode.tmp;
			display_saveondays();
			display.off_days &= ~display.on_days;
			display_saveoffdays();
			mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
		    }
		    break;
		case BUTTONS_PLUS:
		    *mode.tmp ^= _BV(mode.tmp[MODE_TMP_IDX]);
		    mode_update(MODE_CFGDISP_SETONDAYS_CUSTOM,
				DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_CFGDISP_SETANIMATED_MENU: ;
	    void menu_cfgdisp_setanimated_init(void) {
		*mode.tmp = display.status;
	    }

	    mode_menu_process_button(
		    MODE_TIME_DISPLAY,
#ifdef ADAFRUIT_BUTTONS
		    MODE_CFGDISP_MENU,
#else
		    MODE_CFGDISP_SETBRIGHT_MENU,
#endif
		    MODE_CFGDISP_SETANIMATED_TOGGLE,
		    menu_cfgdisp_setanimated_init,
		    btn, TRUE);
	    break;
	case MODE_CFGDISP_SETANIMATED_TOGGLE:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    display.status = *mode.tmp;
		    display_savestatus();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
		    break;
		case BUTTONS_PLUS:
		    *mode.tmp ^= DISPLAY_ANIMATED;
		    mode_update(MODE_CFGDISP_SETANIMATED_TOGGLE,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_CFGREGN_MENU:
	    mode_menu_process_button(
		    MODE_TIME_DISPLAY,
#ifdef ADAFRUIT_BUTTONS
		    MODE_TIME_DISPLAY,
#else
		    MODE_SETALARM_MENU,
#endif
		    MODE_CFGREGN_SETDST_MENU,
		    NULL,
		    btn, TRUE);
	    break;
	case MODE_CFGREGN_SETDST_MENU: ;
	    void menu_cfgregn_setdst_init(void) {
		*mode.tmp = time.status;
	    }

	    mode_menu_process_button(
		    MODE_TIME_DISPLAY,
#ifdef GPS_TIMEKEEPING
		    MODE_CFGREGN_SETZONE_MENU,
#else
		    MODE_CFGREGN_TIMEFMT_MENU,
#endif  // GPS_TIMEKEEPING
		    MODE_CFGREGN_SETDST_STATE,
		    menu_cfgregn_setdst_init,
		    btn, FALSE);
	    break;
	case MODE_CFGREGN_SETDST_STATE:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
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
				mode_update(MODE_TIME_DISPLAY,
					    DISPLAY_TRANS_UP);
				break;
			    default:  // GMT, CET, or EET
				mode_update(MODE_CFGREGN_SETDST_ZONE,
					    DISPLAY_TRANS_UP);
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
		    mode_update(MODE_CFGREGN_SETDST_STATE,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_CFGREGN_SETDST_ZONE:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    ATOMIC_BLOCK(ATOMIC_FORCEON) {
			time.status = *mode.tmp;
			time_autodst(FALSE);
			time_savestatus();
		    }
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
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
		    mode_update(MODE_CFGREGN_SETDST_ZONE,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    break;
	    }
	    break;
#ifdef GPS_TIMEKEEPING
	case MODE_CFGREGN_SETZONE_MENU: ;
	    void menu_cfgregn_setzone_init(void) {
		mode.tmp[MODE_TMP_HOUR]   = gps.rel_utc_hour;
		mode.tmp[MODE_TMP_MINUTE] = gps.rel_utc_minute;
	    }

	    mode_menu_process_button(
		    MODE_TIME_DISPLAY,
		    MODE_CFGREGN_TIMEFMT_MENU,
		    MODE_CFGREGN_SETZONE_HOUR,
		    menu_cfgregn_setzone_init,
		    btn, FALSE);
	    break;
	case MODE_CFGREGN_SETZONE_HOUR:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    mode_update(MODE_CFGREGN_SETZONE_MINUTE,
			        DISPLAY_TRANS_INSTANT);
		    break;
		case BUTTONS_PLUS:
		    if(mode.tmp[MODE_TMP_HOUR] >= GPS_HOUR_OFFSET_MAX) {
			mode.tmp[MODE_TMP_HOUR] = GPS_HOUR_OFFSET_MIN;
		    } else {
			++mode.tmp[MODE_TMP_HOUR];
		    }
		    mode_update(MODE_CFGREGN_SETZONE_HOUR,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_CFGREGN_SETZONE_MINUTE:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    ATOMIC_BLOCK(ATOMIC_FORCEON) {
			gps.rel_utc_hour   = mode.tmp[MODE_TMP_HOUR  ];
			gps.rel_utc_minute = mode.tmp[MODE_TMP_MINUTE];
			gps_saverelutc();
		    }
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
		    break;
		case BUTTONS_PLUS:
		    ++mode.tmp[MODE_TMP_MINUTE];
		    mode.tmp[MODE_TMP_MINUTE] %= 60;
		    mode_update(MODE_CFGREGN_SETZONE_MINUTE,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    break;
	    }
	    break;
#endif  // GPS_TIMEKEEPING
	case MODE_CFGREGN_TIMEFMT_MENU: ;
	    void menu_cfgregn_set12hour_init(void) {
		*mode.tmp = time.timeformat;
	    }

	    mode_menu_process_button(
		    MODE_TIME_DISPLAY,
		    MODE_CFGREGN_DATEFMT_MENU,
		    MODE_CFGREGN_TIMEFMT_12HOUR,
		    menu_cfgregn_set12hour_init,
		    btn, FALSE);
	    break;
	case MODE_CFGREGN_TIMEFMT_12HOUR:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    if(*mode.tmp & TIME_TIMEFORMAT_12HOUR) {
			*mode.tmp |= TIME_TIMEFORMAT_SHOWAMPM;

			if(*mode.tmp <= TIME_TIMEFORMAT_HH_MM) {
			    time.timeformat |= TIME_TIMEFORMAT_SHOWAMPM;
			} else {
			    time.timeformat &= ~TIME_TIMEFORMAT_SHOWAMPM;
			}
		    } else {
			*mode.tmp &= ~TIME_TIMEFORMAT_SHOWAMPM;

			if(*mode.tmp > TIME_TIMEFORMAT_HH_MM) {
			    *mode.tmp = TIME_TIMEFORMAT_HH_MM_SS;
			}
		    }
		    time.timeformat = *mode.tmp;
		    *mode.tmp      &= TIME_TIMEFORMAT_MASK;
		    mode_update(MODE_CFGREGN_TIMEFMT_FORMAT, DISPLAY_TRANS_UP);
		    break;
		case BUTTONS_PLUS:
		    *mode.tmp ^= TIME_TIMEFORMAT_12HOUR;
		    mode_update(MODE_CFGREGN_TIMEFMT_12HOUR,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_CFGREGN_TIMEFMT_FORMAT:
	    switch(btn) {
		case BUTTONS_MENU:
		    time_loadtimeformat();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    mode_update(MODE_CFGREGN_TIMEFMT_SHOWDST,
			        DISPLAY_TRANS_UP);
		    break;
		case BUTTONS_PLUS:
		    if(time.timeformat & TIME_TIMEFORMAT_12HOUR) {
			if(++(*mode.tmp) >= TIME_TIMEFORMAT_HHMMSSPM + 1) {
			    *mode.tmp = TIME_TIMEFORMAT_HH_MM_SS;
			}

			if(*mode.tmp <= TIME_TIMEFORMAT_HH_MM) {
			    time.timeformat |= TIME_TIMEFORMAT_SHOWAMPM;
			} else {
			    time.timeformat &= ~TIME_TIMEFORMAT_SHOWAMPM;
			}
		    } else {
			if(++(*mode.tmp) > TIME_TIMEFORMAT_HH_MM) {
			    *mode.tmp = TIME_TIMEFORMAT_HH_MM_SS;
			}

			time.timeformat &= ~TIME_TIMEFORMAT_SHOWAMPM;
		    }

		    time.timeformat &= ~TIME_TIMEFORMAT_MASK;
		    time.timeformat |= *mode.tmp;

		    mode_update(MODE_CFGREGN_TIMEFMT_FORMAT,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) {
			time_loadtimeformat();
		    } else if(!(mode.timer & 0x01FF)) {
			if(mode.timer & 0x0200) {
			    display_clearall();
			    display_transition(DISPLAY_TRANS_INSTANT);
			} else {
			    mode_time_display();
			    display_transition(DISPLAY_TRANS_INSTANT);
			}
		    }
		    break;
	    }
	    break;
	case MODE_CFGREGN_TIMEFMT_SHOWDST:
	    switch(btn) {
		case BUTTONS_MENU:
		    time_loadtimeformat();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
#ifdef GPS_TIMEKEEPING
		    if((time.timeformat & TIME_TIMEFORMAT_SHOWAMPM)
			    && (time.timeformat & TIME_TIMEFORMAT_SHOWDST)) {
			time.timeformat &= ~TIME_TIMEFORMAT_SHOWGPS;
			time_savetimeformat();
			mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
		    } else {
			mode_update(MODE_CFGREGN_TIMEFMT_SHOWGPS,
				    DISPLAY_TRANS_UP);
		    }
#else
		    time_savetimeformat();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
#endif  // GPS_TIMEKEEPING
		    break;
		case BUTTONS_PLUS:
		    time.timeformat ^= TIME_TIMEFORMAT_SHOWDST;
		    mode_update(MODE_CFGREGN_TIMEFMT_SHOWDST,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) {
			time_loadtimeformat();
		    }
		    break;
	    }
	    break;
#ifdef GPS_TIMEKEEPING
	case MODE_CFGREGN_TIMEFMT_SHOWGPS:
	    switch(btn) {
		case BUTTONS_MENU:
		    time_loadtimeformat();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    time_savetimeformat();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
		    break;
		case BUTTONS_PLUS:
		    time.timeformat ^= TIME_TIMEFORMAT_SHOWGPS;
		    mode_update(MODE_CFGREGN_TIMEFMT_SHOWGPS,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) {
			time_loadtimeformat();
		    }
		    break;
	    }
	    break;
#endif  // GPS_TIMEKEEPING
	case MODE_CFGREGN_DATEFMT_MENU: ;
	    void menu_cfgregn_datefmt(void) {
		*mode.tmp = time.dateformat;
	    }

	    mode_menu_process_button(
		    MODE_TIME_DISPLAY,
		    MODE_CFGREGN_MISCFMT_MENU,
		    MODE_CFGREGN_DATEFMT_SHOWWDAY,
		    menu_cfgregn_datefmt, btn, FALSE);
	    break;
	case MODE_CFGREGN_DATEFMT_SHOWWDAY:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    time.dateformat = *mode.tmp;
		    *mode.tmp = time.dateformat & TIME_DATEFORMAT_MASK;
		    mode_update(MODE_CFGREGN_DATEFMT_FORMAT, DISPLAY_TRANS_UP);
		    break;
		case BUTTONS_PLUS:
		    *mode.tmp ^= TIME_DATEFORMAT_SHOWWDAY;
		    mode_update(MODE_CFGREGN_DATEFMT_SHOWWDAY,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_CFGREGN_DATEFMT_FORMAT:
	    switch(btn) {
		case BUTTONS_MENU:
		    time_loaddateformat();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    switch(*mode.tmp) {
			case TIME_DATEFORMAT_TEXT_EU:
			case TIME_DATEFORMAT_TEXT_USA:
			    mode_update(MODE_CFGREGN_DATEFMT_SHOWYEAR,
				        DISPLAY_TRANS_UP);
			    break;
			default:
			    time.dateformat &= ~TIME_DATEFORMAT_SHOWYEAR;
			    time_savedateformat();
			    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
			    break;
		    }
		    break;
		case BUTTONS_PLUS:
		    if(++(*mode.tmp) > TIME_DATEFORMAT_TEXT_USA) {
			*mode.tmp = TIME_DATEFORMAT_DOTNUM_ISO;
		    }

		    time.dateformat &= ~TIME_DATEFORMAT_MASK;
		    time.dateformat |= *mode.tmp;

		    mode_update(MODE_CFGREGN_DATEFMT_FORMAT,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) {
			time_loaddateformat();
		    } else if(!(mode.timer & 0x01FF)) {
			if(mode.timer & 0x0200) {
			    display_clearall();
			    display_transition(DISPLAY_TRANS_INSTANT);
			} else {
			    mode_monthday_display();
			    display_transition(DISPLAY_TRANS_INSTANT);
			}
		    }
		    break;
	    }
	    break;
	case MODE_CFGREGN_DATEFMT_SHOWYEAR:
	    switch(btn) {
		case BUTTONS_MENU:
		    time_loaddateformat();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    time_savedateformat();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
		    break;
		case BUTTONS_PLUS:
		    time.dateformat ^= TIME_DATEFORMAT_SHOWYEAR;
		    mode_update(MODE_CFGREGN_DATEFMT_SHOWYEAR,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) {
			time_loaddateformat();
		    }
		    break;
	    }
	    break;
	case MODE_CFGREGN_MISCFMT_MENU: ;
	    mode_menu_process_button(
		    MODE_TIME_DISPLAY,
#ifdef ADAFRUIT_BUTTONS
		    MODE_CFGREGN_MENU,
#else
		    MODE_CFGREGN_SETDST_MENU,
#endif
		    MODE_CFGREGN_MISCFMT_ZEROPAD,
		    NULL,
		    btn, TRUE);
	    break;
	case MODE_CFGREGN_MISCFMT_ZEROPAD:
	    switch(btn) {
		case BUTTONS_MENU:
		    display_loadstatus();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    mode_update(MODE_CFGREGN_MISCFMT_ALTNINE,
			        DISPLAY_TRANS_UP);
		    break;
		case BUTTONS_PLUS:
		    display.status ^= DISPLAY_ZEROPAD;
		    mode_update(MODE_CFGREGN_MISCFMT_ZEROPAD,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) {
			display_loadstatus();
		    }
		    break;
	    }
	    break;
	case MODE_CFGREGN_MISCFMT_ALTNINE:
	    switch(btn) {
		case BUTTONS_MENU:
		    display_loadstatus();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    mode_update(MODE_CFGREGN_MISCFMT_ALTALPHA,
			        DISPLAY_TRANS_UP);
		    break;
		case BUTTONS_PLUS:
		    display.status ^= DISPLAY_ALTNINE;
		    mode_update(MODE_CFGREGN_MISCFMT_ALTNINE,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) {
			display_loadstatus();
		    }
		    break;
	    }
	    break;
	case MODE_CFGREGN_MISCFMT_ALTALPHA:
	    switch(btn) {
		case BUTTONS_MENU:
		    display_loadstatus();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    display_savestatus();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
		    break;
		case BUTTONS_PLUS:
		    display.status ^= DISPLAY_ALTALPHA;
		    mode_update(MODE_CFGREGN_MISCFMT_ALTALPHA,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) {
			display_loadstatus();
		    }
		    break;
	    }
	    break;
	default:
	    break;
    }

    if(++mode.timer > MODE_TIMEOUT) {
	mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
    }
}


// called when the alarm switch is turned on
void mode_alarmset(void) {
    if(mode.state <= MODE_SNOOZEON_DISPLAY) {
	mode_update(MODE_ALARMSET_DISPLAY, DISPLAY_TRANS_DOWN);
    }
}


// called when the alarm switch is turned off
void mode_alarmoff(void) {
    if(mode.state <= MODE_SNOOZEON_DISPLAY) {
	mode_update(MODE_ALARMOFF_DISPLAY, DISPLAY_TRANS_DOWN);
    }
}


// called when the snooze mode is activated
void mode_snoozing(void) {
    if(mode.state <= MODE_SNOOZEON_DISPLAY) {
	mode_update(MODE_SNOOZEON_DISPLAY, DISPLAY_TRANS_DOWN);
    }
}


// change mode to specified state and update display
void mode_update(uint8_t new_state, uint8_t disp_trans) {
    PGM_P pstr_ptr;

    mode.timer = 0;
    mode.state = new_state;

    display_clearall();

    switch(mode.state) {
	case MODE_TIME_DISPLAY:
	    // display current time
	    mode_time_display();
	    break;
	case MODE_DAYOFWEEK_DISPLAY:
	    display_pstr(0, time_wday2pstr(
			        time_dayofweek(time.year, time.month,
					       time.day)));
	    break;
	case MODE_MONTHDAY_DISPLAY:
	    mode_monthday_display();
	    break;
	case MODE_YEAR_DISPLAY:
	    display_twodigit_zeropad(3, 20);
	    display_twodigit_zeropad(5, time.year);
	    break;
	case MODE_ALARMSET_DISPLAY:
	    display_pstr(0, PSTR("alar set"));
	    break;
	case MODE_ALARMIDX_DISPLAY:
	    display_pstr(0, PSTR("alarm"));
	    display_twodigit_leftadj(7, *mode.tmp + 1);
	    break;
	case MODE_ALARMTIME_DISPLAY:
	    if(alarm.days[*mode.tmp] & ALARM_ENABLED) {
		mode_alarm_display(alarm.hours[*mode.tmp],
				   alarm.minutes[*mode.tmp]);
	    } else {
		display_pstr(0, PSTR("disabled"));
	    }
	    break;
	case MODE_ALARMDAYS_DISPLAY:
	    switch(alarm.days[*mode.tmp] & ~ALARM_ENABLED) {
		case TIME_ALLDAYS:
		    display_pstr(0, PSTR("all days"));
		    break;
		case TIME_WEEKDAYS:
		    display_pstr(0, PSTR("weekdays"));
		    break;
		case TIME_WEEKENDS:
		    display_pstr(0, PSTR("weekends"));
		    break;
		default:
		    mode_daysofweek_display(alarm.days[*mode.tmp]);
		    break;
	    }
	    break;
	case MODE_ALARMOFF_DISPLAY:
	    display_pstr(0, PSTR("alar off"));
	    break;
	case MODE_SNOOZEON_DISPLAY:
	    display_pstr(0, PSTR("snoozing"));
	    break;
	case MODE_SETALARM_MENU:
	    display_pstr(0, PSTR("set alar"));
	    break;
	case MODE_SETALARM_IDX:
	    display_pstr(0, PSTR("alarm"));
	    display_twodigit_leftadj(7, *mode.tmp + 1);
	    break;
	case MODE_SETALARM_ENABLE:
	    if(alarm.days[*mode.tmp] & ALARM_ENABLED) {
		pstr_ptr = PSTR("on");
	    } else {
		pstr_ptr = PSTR("off");
	    }
	    mode_texttext_display(PSTR("alar"), pstr_ptr);
	    break;
	case MODE_SETALARM_HOUR:
	    mode_alarm_display(alarm.hours[*mode.tmp],
			       alarm.minutes[*mode.tmp]);
	    if(time.timeformat & TIME_TIMEFORMAT_12HOUR) {
		display_dotselect(1, 2);
	    } else {
		display_dotselect(2, 3);
	    }
	    break;
	case MODE_SETALARM_MINUTE:
	    mode_alarm_display(alarm.hours[*mode.tmp],
			       alarm.minutes[*mode.tmp]);
	    if(time.timeformat & TIME_TIMEFORMAT_12HOUR) {
		display_dotselect(4, 5);
	    } else {
		display_dotselect(5, 6);
	    }
	    break;
	case MODE_SETALARM_DAYS_OPTIONS:
	    switch((uint8_t)mode.tmp[MODE_TMP_DAYS]) {
		case TIME_ALLDAYS | ALARM_ENABLED:
		    display_pstr(0, PSTR("all days"));
		    break;
		case TIME_WEEKDAYS | ALARM_ENABLED:
		    display_pstr(0, PSTR("weekdays"));
		    break;
		case TIME_WEEKENDS | ALARM_ENABLED:
		    display_pstr(0, PSTR("weekends"));
		    break;
		default:
		    display_pstr(0, PSTR(" custom "));
		    break;
	    }
	    display_dotselect(1, 8);
	    break;
	case MODE_SETALARM_DAYS_CUSTOM:
	    mode_daysofweek_display(mode.tmp[MODE_TMP_DAYS]);
	    display_dot(1 + mode.tmp[MODE_TMP_IDX], TRUE);
	    break;
	case MODE_SETTIME_MENU:
	    display_pstr(0, PSTR("set time"));
	    break;
	case MODE_SETTIME_HOUR:
	    mode_settime_display(mode.tmp[MODE_TMP_HOUR],
		                 mode.tmp[MODE_TMP_MINUTE],
			         mode.tmp[MODE_TMP_SECOND]);
	    display_dotselect(1, 2);
	    break;
	case MODE_SETTIME_MINUTE:
	    mode_settime_display(mode.tmp[MODE_TMP_HOUR],
		                 mode.tmp[MODE_TMP_MINUTE],
			         mode.tmp[MODE_TMP_SECOND]);
	    display_dotselect(4, 5);
	    break;
	case MODE_SETTIME_SECOND:
	    mode_settime_display(mode.tmp[MODE_TMP_HOUR],
		                 mode.tmp[MODE_TMP_MINUTE],
			         mode.tmp[MODE_TMP_SECOND]);
	    display_dotselect(7, 8);
	    break;
	case MODE_SETDATE_MENU:
	    display_pstr(0, PSTR("set date"));
	    break;
	case MODE_SETDATE_YEAR:
	    display_twodigit_zeropad(1, 20);
	    display_twodigit_zeropad(3, mode.tmp[MODE_TMP_YEAR]);
	    display_clear(5);
	    display_pstr(6, time_month2pstr(mode.tmp[MODE_TMP_MONTH]));
	    display_dotselect(3, 4);
	    break;
	case MODE_SETDATE_MONTH:
	    display_twodigit_zeropad(1, 20);
	    display_twodigit_leftadj(3, mode.tmp[MODE_TMP_YEAR]);
	    display_clear(5);
	    display_pstr(6, time_month2pstr(mode.tmp[MODE_TMP_MONTH]));
	    display_dotselect(6, 8);
	    break;
	case MODE_SETDATE_DAY:
	    display_pstr(0, time_month2pstr(mode.tmp[MODE_TMP_MONTH]));
	    display_twodigit_rightadj(5, mode.tmp[MODE_TMP_DAY]);
	    display_dotselect(5, 6);
	    break;
	case MODE_CFGALARM_MENU:
	    display_pstr(0, PSTR("cfg alar"));
	    break;
	case MODE_CFGALARM_SETSOUND_MENU:
	    display_pstr(0, PSTR("a sound"));
	    display_dot(1, TRUE);
	    break;
	case MODE_CFGALARM_SETSOUND:
	    display_pstr(0, piezo_pstr());
	    display_dotselect(1, 8);
	    break;
	case MODE_CFGALARM_SETVOL_MENU:
	    display_pstr(0, PSTR("a volume"));
	    display_dot(1, TRUE);
	    break;
	case MODE_CFGALARM_SETVOL:
	    pstr_ptr = PSTR("vol");
	    if(*mode.tmp == 11) {
		mode_texttext_display(pstr_ptr, PSTR("prog"));
	    } else {
		mode_textnum_display(pstr_ptr, *mode.tmp);
	    }
	    break;
	case MODE_CFGALARM_SETVOL_MIN:
	    mode_textnum_display(PSTR("v min"), mode.tmp[MODE_TMP_MIN]);
	    break;
	case MODE_CFGALARM_SETVOL_MAX:
	    mode_textnum_display(PSTR("v max"), mode.tmp[MODE_TMP_MAX]);
	    break;
	case MODE_CFGALARM_SETVOL_TIME:
	    mode_textnum_display(PSTR("time"), *mode.tmp);
	    break;
	case MODE_CFGALARM_SETSNOOZE_MENU:
	    display_pstr(0, PSTR("a snooze"));
	    display_dot(1, TRUE);
	    break;
	case MODE_CFGALARM_SETSNOOZE_TIME:
	    pstr_ptr = PSTR("snoz");
	    if(*mode.tmp) {
		mode_textnum_display(pstr_ptr, *mode.tmp);
	    } else {
		mode_texttext_display(pstr_ptr, PSTR("off"));
		display_dotselect(6, 8);
	    }
	    break;
	case MODE_CFGALARM_SETHEARTBEAT_MENU:
	    display_pstr(0, PSTR("a pulse "));
	    display_dot(1, TRUE);
	    break;
	case MODE_CFGALARM_SETHEARTBEAT_TOGGLE:
	    if(*mode.tmp & ALARM_SOUNDING_PULSE) {
		pstr_ptr = PSTR("on");
	    } else {
		pstr_ptr = PSTR("off");
	    }
	    mode_texttext_display(PSTR("puls"), pstr_ptr);
	    break;
	case MODE_CFGDISP_MENU:
	    display_pstr(0, PSTR("cfg disp"));
	    break;
	case MODE_CFGDISP_SETBRIGHT_MENU:
	    display_pstr(0, PSTR("disp bri"));
	    break;
	case MODE_CFGDISP_SETBRIGHT_LEVEL:
#ifdef AUTOMATIC_DIMMER
	    if(*mode.tmp == 11) {
		display_loadbright();
		mode_texttext_display(PSTR("bri"), PSTR("auto"));
	    } else {
		display.bright_min = display.bright_max = *mode.tmp;
		display_autodim();
		mode_textnum_display(PSTR("bri"), *mode.tmp);
	    }
#else
	    display_autodim();
	    mode_textnum_display(PSTR("bri"), display.brightness);
#endif  // AUTOMATIC_DIMMER
	    break;
#ifdef AUTOMATIC_DIMMER
	case MODE_CFGDISP_SETBRIGHT_MIN:
	    if(*mode.tmp < 0) {
		// user might not be able to see lowest brightness
		display_loadbright();
	    } else {
		display.bright_min = display.bright_max = *mode.tmp;
		display_autodim();
	    }
	    mode_textnum_display(PSTR("b min"), *mode.tmp);
	    display_dot(1, TRUE);
	    break;
	case MODE_CFGDISP_SETBRIGHT_MAX:
	    display.bright_min = display.bright_max = *mode.tmp;
	    display_autodim();
	    mode_textnum_display(PSTR("b max"), *mode.tmp);
	    display_dot(1, TRUE);
	    break;
#endif  // AUTOMATIC_DIMMER
	case MODE_CFGDISP_SETDIGITBRIGHT_MENU:
	    display_pstr(0, PSTR("digt bri"));
	    break;
	case MODE_CFGDISP_SETDIGITBRIGHT_LEVEL:
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
	case MODE_CFGDISP_SETAUTOOFF_MENU:
	    display_pstr(0, PSTR("auto off"));
	    break;
#ifdef AUTOMATIC_DIMMER
	case MODE_CFGDISP_SETPHOTOOFF_MENU:
	    display_pstr(0, PSTR("off dark"));
	    break;
	case MODE_CFGDISP_SETPHOTOOFF_THRESH:
	    if(*mode.tmp) {
		mode_textnum_display(PSTR("thrsh"), *mode.tmp);
	    } else {
		display_pstr(0, PSTR("disabled"));
		display_dotselect(1, 8);
	    }
	    break;
#endif  // AUTOMATIC_DIMMER
	case MODE_CFGDISP_SETOFFTIME_MENU:
	    display_pstr(0, PSTR("off time"));
	    break;
	case MODE_CFGDISP_SETOFFTIME_TOGGLE:
	    if(*mode.tmp) {
		display_pstr(0, PSTR("disabled"));
	    } else {
		display_pstr(0, PSTR("enabled "));
	    }
	    display_dotselect(1, 8);
	    break;
	case MODE_CFGDISP_SETOFFTIME_OFFHOUR:
	    mode_alarm_display(display.off_hour,
			       display.off_minute);
	    if(time.timeformat & TIME_TIMEFORMAT_12HOUR) {
		display_dotselect(1, 2);
	    } else {
		display_dotselect(2, 3);
	    }
	    break;
	case MODE_CFGDISP_SETOFFTIME_OFFMINUTE:
	    mode_alarm_display(display.off_hour,
			       display.off_minute);
	    if(time.timeformat & TIME_TIMEFORMAT_12HOUR) {
		display_dotselect(4, 5);
	    } else {
		display_dotselect(5, 6);
	    }
	    break;
	case MODE_CFGDISP_SETOFFTIME_ONHOUR:
	    mode_alarm_display(display.on_hour,
			       display.on_minute);
	    if(time.timeformat & TIME_TIMEFORMAT_12HOUR) {
		display_dotselect(1, 2);
	    } else {
		display_dotselect(2, 3);
	    }
	    break;
	case MODE_CFGDISP_SETOFFTIME_ONMINUTE:
	    mode_alarm_display(display.on_hour,
			       display.on_minute);
	    if(time.timeformat & TIME_TIMEFORMAT_12HOUR) {
		display_dotselect(4, 5);
	    } else {
		display_dotselect(5, 6);
	    }
	    break;
	case MODE_CFGDISP_SETOFFDAYS_MENU:
	    display_pstr(0, PSTR("off days"));
	    break;
	case MODE_CFGDISP_SETONDAYS_MENU:
	    display_pstr(0, PSTR("on days "));
	    break;
	case MODE_CFGDISP_SETOFFDAYS_OPTIONS:
	case MODE_CFGDISP_SETONDAYS_OPTIONS:
	    switch((uint8_t)*mode.tmp) {
		case TIME_NODAYS:
		    display_pstr(0, PSTR("disabled"));
		    break;
		case TIME_ALLDAYS:
		    display_pstr(0, PSTR("all days"));
		    break;
		case TIME_WEEKDAYS:
		    display_pstr(0, PSTR("weekdays"));
		    break;
		case TIME_WEEKENDS:
		    display_pstr(0, PSTR("weekends"));
		    break;
		default:
		    display_pstr(0, PSTR(" custom "));
		    break;
	    }
	    display_dotselect(1, 8);
	    break;
	case MODE_CFGDISP_SETOFFDAYS_CUSTOM:
	case MODE_CFGDISP_SETONDAYS_CUSTOM:
	    mode_daysofweek_display(*mode.tmp);
	    display_dot(1 + mode.tmp[MODE_TMP_IDX], TRUE);
	    break;
	case MODE_CFGDISP_SETANIMATED_MENU:
	    display_pstr(0, PSTR("animated"));
	    break;
	case MODE_CFGDISP_SETANIMATED_TOGGLE:
	    if(*mode.tmp & DISPLAY_ANIMATED) {
		pstr_ptr = PSTR("on");
	    } else {
		pstr_ptr = PSTR("off");
	    }
	    mode_texttext_display(PSTR("anim"), pstr_ptr);
	    break;
	case MODE_CFGREGN_MENU:
	    display_pstr(0, PSTR("cfg regn"));
	    break;
	case MODE_CFGREGN_SETDST_MENU:
	    display_pstr(0, PSTR("set dst"));
	    break;
	case MODE_CFGREGN_SETDST_STATE:
	    switch(*mode.tmp & TIME_AUTODST_MASK) {
		case TIME_AUTODST_USA:
		    pstr_ptr = PSTR("usa");
		    display_dotselect(6, 8);
		    break;
		case TIME_AUTODST_NONE:
		    if(*mode.tmp & TIME_DST) {
			pstr_ptr = PSTR("on");
		    } else {
			pstr_ptr = PSTR("off");
		    }
		    break;
		default:  // GMT, CET, or EET
		    pstr_ptr = PSTR("eu");
		    break;
	    }

	    mode_texttext_display(PSTR("dst"), pstr_ptr);
	    break;
	case MODE_CFGREGN_SETDST_ZONE:
	    switch(*mode.tmp & TIME_AUTODST_MASK) {
		case TIME_AUTODST_EU_CET:
		    pstr_ptr = PSTR("cet");
		    break;
		case TIME_AUTODST_EU_EET:
		    pstr_ptr = PSTR("eet");
		    break;
		default:  // GMT
		    pstr_ptr = PSTR("utc");
		    break;
	    }

	    mode_texttext_display(PSTR("zone"), pstr_ptr);
	    break;
#ifdef GPS_TIMEKEEPING
	case MODE_CFGREGN_SETZONE_MENU:
	    display_pstr(0, PSTR("set zone"));
	    break;
	case MODE_CFGREGN_SETZONE_HOUR:
	    mode_zone_display();
	    display_dotselect(2, 3);
	    break;
	case MODE_CFGREGN_SETZONE_MINUTE:
	    mode_zone_display();
	    display_dotselect(6, 7);
	    break;
#endif  // GPS_TIMEKEEPING
	case MODE_CFGREGN_TIMEFMT_MENU:
	    display_pstr(0, PSTR("time fmt"));
	    break;
	case MODE_CFGREGN_TIMEFMT_12HOUR:
	    mode_textnum_display(PSTR("hours"),
		    (*mode.tmp & TIME_TIMEFORMAT_12HOUR ? 12 : 24));
	    break;
	case MODE_CFGREGN_TIMEFMT_FORMAT:
	    mode_time_display();
	    break;
	case MODE_CFGREGN_TIMEFMT_SHOWDST:
	    pstr_ptr = PSTR("dst");
	    if(time.timeformat & TIME_TIMEFORMAT_SHOWDST) {
		mode_texttext_display(pstr_ptr, PSTR("show"));
	    } else {
		mode_texttext_display(pstr_ptr, PSTR("hide"));
	    }
	    break;
#ifdef GPS_TIMEKEEPING
	case MODE_CFGREGN_TIMEFMT_SHOWGPS:
	    pstr_ptr = PSTR("gps");
	    if(time.timeformat & TIME_TIMEFORMAT_SHOWGPS) {
		mode_texttext_display(pstr_ptr, PSTR("show"));
	    } else {
		mode_texttext_display(pstr_ptr, PSTR("hide"));
	    }
	    break;
#endif  // GPS_TIMEKEEPING
	case MODE_CFGREGN_DATEFMT_MENU:
	    display_pstr(0, PSTR("date fmt"));
	    break;
	case MODE_CFGREGN_DATEFMT_SHOWWDAY:
	    if(*mode.tmp & TIME_DATEFORMAT_SHOWWDAY) {
		pstr_ptr = PSTR("on");
	    } else {
		pstr_ptr = PSTR("off");
	    }
	    mode_texttext_display(PSTR("wday"), pstr_ptr);
	    break;
	case MODE_CFGREGN_DATEFMT_FORMAT:
	    mode_monthday_display();
	    break;
	case MODE_CFGREGN_DATEFMT_SHOWYEAR:
	    if(time.dateformat & TIME_DATEFORMAT_SHOWYEAR) {
		pstr_ptr = PSTR("on");
	    } else {
		pstr_ptr = PSTR("off");
	    }

	    mode_texttext_display(PSTR("year"), pstr_ptr);
	    break;
	case MODE_CFGREGN_MISCFMT_MENU:
	    display_pstr(0, PSTR("misc fmt"));
	    break;
	case MODE_CFGREGN_MISCFMT_ZEROPAD:
	    mode_textnum_display(PSTR("zero"), 0);
	    break;
	case MODE_CFGREGN_MISCFMT_ALTNINE:
	    mode_textnum_display(PSTR("nine"), 9);
	    break;
	case MODE_CFGREGN_MISCFMT_ALTALPHA:
	    mode_texttext_display(PSTR("char"), PSTR("eg"));
	    break;
	default:
	    display_pstr(0, PSTR("-error-"));
	    break;
    }

    mode.timer = 0;
    mode.state = new_state;

    display_transition(disp_trans);
}


// updates the time display every second
void mode_time_display(void) {
    uint8_t hour_to_display = time.hour;
    uint8_t hour_idx = 1;

    display_clearall();

    if((time.timeformat & TIME_TIMEFORMAT_MASK) == TIME_TIMEFORMAT_HH_MM) {
	hour_idx = 2;
    }

    if(time.timeformat & TIME_TIMEFORMAT_12HOUR) {
	if(hour_to_display > 12) hour_to_display -= 12;
	if(hour_to_display == 0) hour_to_display  = 12;
	display_twodigit_rightadj(hour_idx, hour_to_display);
    } else {
	display_twodigit_zeropad(hour_idx, hour_to_display);
    }

    switch(time.timeformat & TIME_TIMEFORMAT_MASK) {
	case TIME_TIMEFORMAT_HH_MM_SS:
	    display_twodigit_zeropad(4, time.minute);
	    display_twodigit_zeropad(7, time.second);
	    break;
	case TIME_TIMEFORMAT_HH_MM_dial:
	    display_twodigit_zeropad(4, time.minute);
	    display_dial(7, time.second);
	    break;
	case TIME_TIMEFORMAT_HH_MM:
	    display_twodigit_zeropad(5, time.minute);
	    break;
	case TIME_TIMEFORMAT_HH_MM_PM:
	    display_twodigit_zeropad(4, time.minute);
	    display_char(7, (time.hour < 12 ? 'a' : 'p'));
	    display_char(8, 'm');
	    break;
	case TIME_TIMEFORMAT_HHMMSSPM:
	    display_dot(2, TRUE);
	    display_twodigit_zeropad(3, time.minute);
	    display_dot(4, TRUE);
	    display_twodigit_zeropad(5, time.second);
	    display_dot(6, TRUE);
	    display_char(7, (time.hour < 12 ? 'a' : 'p'));
	    display_char(8, 'm');
	    break;
	default:
	    break;
    }

    // set or clear am or pm indicator
    if(time.timeformat & TIME_TIMEFORMAT_SHOWAMPM) {
	// show leftmost circle if pm
	display_dot(0, time.hour >= 12);
	if(time.timeformat & TIME_TIMEFORMAT_SHOWDST) {
	    // show rightmost decimal if dst
	    display_dot(8, time.status & TIME_DST);
	}
#ifdef GPS_TIMEKEEPING
	else if(time.timeformat & TIME_TIMEFORMAT_SHOWGPS) {
	    display_dot(8, gps.status & GPS_SIGNAL_GOOD);
	}
#endif  // GPS_TIMEKEEPING
    } else {
	if(time.timeformat & TIME_TIMEFORMAT_SHOWDST) {
	    // show leftmost circle if dst
	    display_dot(0, time.status & TIME_DST);
#ifdef GPS_TIMEKEEPING
	    if(time.timeformat & TIME_TIMEFORMAT_SHOWGPS) {
		display_dot(8, gps.status & GPS_SIGNAL_GOOD);
	    }
#endif  // GPS_TIMEKEEPING
	}
#ifdef GPS_TIMEKEEPING
	else if(time.timeformat & TIME_TIMEFORMAT_SHOWGPS) {
	    display_dot(0, gps.status & GPS_SIGNAL_GOOD);
	}
#endif  // GPS_TIMEKEEPING
    }

    // show alarm status with leftmost dash
    display_dash(0, alarm.status & ALARM_SET
		    && ( !(alarm.status & (ALARM_SOUNDING | ALARM_SNOOZE))
		    || time.second % 2));
}


// updates the time display every second
void mode_settime_display(uint8_t hour, uint8_t minute,
		          uint8_t second) {
    uint8_t hour_to_display = hour;

    display_clear(0);

    if(time.timeformat & TIME_TIMEFORMAT_12HOUR) {
	display_dot(0, hour >= 12);  // leftmost circle if pm
	if(hour_to_display == 0) hour_to_display  = 12;
	if(hour_to_display > 12) hour_to_display -= 12;
	display_twodigit_rightadj(1, hour_to_display);
    } else {
	display_twodigit_zeropad(1, hour_to_display);
    }

    display_clear(3);

    display_twodigit_zeropad(4, minute);
    display_clear(6);
    display_twodigit_zeropad(7, second);
}


// displays current alarm time
void mode_alarm_display(uint8_t hour, uint8_t minute) {
    if(time.timeformat & TIME_TIMEFORMAT_12HOUR) {
	uint8_t hour_to_display = hour;
	if(hour_to_display > 12) hour_to_display -= 12;
	if(hour_to_display == 0) hour_to_display  = 12;
	display_clear(0);
	display_twodigit_rightadj(1, hour_to_display);
	display_clear(3);
	display_twodigit_zeropad(4, minute);
	display_clear(6);
	if(hour < 12) {
	    display_char(7, 'a');
	    display_char(8, 'm');
	} else {
	    display_char(7, 'p');
	    display_char(8, 'm');
	}
    } else {
	display_clear(0);
	display_clear(1);
	display_twodigit_zeropad(2, hour);
	display_clear(4);
	display_twodigit_zeropad(5, minute);
	display_clear(7);
	display_clear(8);
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
    display_twodigit_rightadj(2, hour_to_display);
    display_char(4, 'h');
    display_clear(5);
    display_twodigit_rightadj(6, mode.tmp[MODE_TMP_MINUTE]);
    display_char(8, 'm');
}


// displays text with a number option
void mode_textnum_display(PGM_P label, int8_t num) {
    display_pstr(0, label);
    display_twodigit_rightadj(7, num);
    display_dotselect(7, 8);
}


// displays text with text option
void mode_texttext_display(PGM_P label, PGM_P opt) {
    display_pstr(0, label);
    uint8_t opt_idx = DISPLAY_SIZE - strlen_P(opt);
    display_pstr(opt_idx, opt);
    display_dotselect(opt_idx, 8);
}


// displays current month and day
void mode_monthday_display(void) {
    display_clear(0);

    switch(time.dateformat & TIME_DATEFORMAT_MASK) {
	case TIME_DATEFORMAT_DOTNUM_ISO:
	    display_twodigit_zeropad(1, 20);
	    display_twodigit_zeropad(3, time.year);
	    display_dot(4, TRUE);
	    display_twodigit_rightadj(5, time.month);
	    display_dot(6, TRUE);
	    display_twodigit_rightadj(7, time.day);
	    break;
	case TIME_DATEFORMAT_DOTNUM_EU:
	    display_twodigit_rightadj(1, time.day);
	    display_dot(2, TRUE);
	    display_twodigit_rightadj(3, time.month);
	    display_dot(4, TRUE);
	    display_twodigit_zeropad(5, 20);
	    display_twodigit_zeropad(7, time.year);
	    break;
	case TIME_DATEFORMAT_DOTNUM_USA:
	    display_twodigit_rightadj(1, time.month);
	    display_dot(2, TRUE);
	    display_twodigit_rightadj(3, time.day);
	    display_dot(4, TRUE);
	    display_twodigit_zeropad(5, 20);
	    display_twodigit_zeropad(7, time.year);
	    break;
	case TIME_DATEFORMAT_DASHNUM_EU:
	    display_twodigit_rightadj(1, time.day);
	    display_char(3, '-');
	    display_twodigit_rightadj(4, time.month);
	    display_char(6, '-');
	    display_twodigit_zeropad(7, time.year);
	    break;
	case TIME_DATEFORMAT_DASHNUM_USA:
	    display_twodigit_rightadj(1, time.month);
	    display_char(3, '-');
	    display_twodigit_rightadj(4, time.day);
	    display_char(6, '-');
	    display_twodigit_zeropad(7, time.year);
	    break;
	case TIME_DATEFORMAT_TEXT_EU:
	    display_clear(1);
	    display_twodigit_rightadj(2, time.day);
	    display_clear(4);
	    display_pstr(5, time_month2pstr(time.month));
	    display_clear(8);
	    break;
	case TIME_DATEFORMAT_TEXT_USA:
	    display_clear(1);
	    display_pstr(2, time_month2pstr(time.month));
	    display_clear(5);
	    display_twodigit_leftadj(6, time.day);
	    display_clear(8);
	    break;
	default:
	    break;
    }
}


// displays the days-of-week for an alarm
void mode_daysofweek_display(uint8_t days) {
    display_pstr(0, PSTR("smtwtfs"));
    for(uint8_t i = 0; i < TIME_NODAY; ++i) {
	if( !(days & _BV(i)) ) {
	    display_clear(1 + i);
	}
    }
}


// helper function to process button presses
// for menu states in menu_semitick()
void mode_menu_process_button(uint8_t up, uint8_t next, uint8_t down,
			      void (*init_func)(void), uint8_t btn,
			      uint8_t next_is_up) {
    switch(btn) {
	case BUTTONS_MENU:
#ifdef ADAFRUIT_BUTTONS
	    if(next_is_up) {
		mode_update(next, DISPLAY_TRANS_DOWN);
	    } else {
		mode_update(next, DISPLAY_TRANS_LEFT);
	    }
#else
	    mode_update(up, DISPLAY_TRANS_DOWN);
#endif
	    break;
	case BUTTONS_SET:
	    if(init_func) init_func();
	    mode_update(down, DISPLAY_TRANS_UP);
	    break;
	case BUTTONS_PLUS:
#ifdef ADAFRUIT_BUTTONS
	    mode_update(up, DISPLAY_TRANS_DOWN);
#else
	    mode_update(next, DISPLAY_TRANS_LEFT);
#endif
	    break;
	default:
	    break;
    }
}

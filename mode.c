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
#include "pizo.h"     // for making clicks and alarm sounds
#include "buttons.h"  // for processing button presses
#include "gps.h"      // for setting the utc offset
#include "usart.h"    // for debugging output


// extern'ed clock mode data
volatile mode_t mode;


// private function declarations
void mode_update(uint8_t new_state, uint8_t disp_trans);
void mode_zone_display(void);
void mode_time_display(uint8_t hour, uint8_t minute, uint8_t second);
void mode_alarm_display(uint8_t hour, uint8_t minute);
void mode_textnum_display(PGM_P pstr, int8_t num);
void mode_dayofweek_display(void);
void mode_monthday_display(void);
void mode_daysofweek_display(uint8_t days);
void mode_menu_process_button(uint8_t up, uint8_t next, uint8_t down,
			      void (*init_func)(void), uint8_t btn);


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
	   } else if(system.initial_mcusr & _BV(BORF)) {
	       display_pstr(0, PSTR("bod rset"));
	   } else if(system.initial_mcusr & _BV(EXTRF)) {
	       display_pstr(0, PSTR("ext rset"));
	   } else if(system.initial_mcusr & _BV(PORF)) {
	       display_pstr(0, PSTR("pwr rset"));
	   } else {
	       display_pstr(0, PSTR(""));
	   }
	   display_transition(DISPLAY_TRANS_INSTANT);
	} else if(gps.data_timer && !gps.warn_timer && time.second % 2) {
	    display_pstr(0, PSTR("gps lost"));
	    display_transition(DISPLAY_TRANS_INSTANT);
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
#ifdef ADAFRUIT_BUTTONS
		    mode_update(MODE_DAYOFWEEK_DISPLAY, DISPLAY_TRANS_LEFT);
#else
		    mode_update(MODE_DAYOFWEEK_DISPLAY, DISPLAY_TRANS_DOWN);
#endif
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
	case MODE_MONTHDAY_DISPLAY:
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
		(gps.data_timer && gps.warn_timer ? MODE_CFGTIME_MENU
		 				  : MODE_SETTIME_MENU),
		MODE_SETALARM_IDX,
	        menu_setalarm_init, btn);
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
		    btn);
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
		    btn);
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
		    MODE_CFGTIME_MENU,
		    MODE_CFGALARM_SETSOUND_MENU,
		    NULL,
		    btn);
	    break;
	case MODE_CFGALARM_SETSOUND_MENU: ;
	    void menu_cfgalarm_setsound_init(void) {
		pizo_setvolume((alarm.volume_min + alarm.volume_max) >> 1, 0);
		pizo_tryalarm_start();
	    }

	    mode_menu_process_button(
		    MODE_TIME_DISPLAY,
		    MODE_CFGALARM_SETVOL_MENU,
		    MODE_CFGALARM_SETSOUND,
		    menu_cfgalarm_setsound_init,
		    btn);
	    break;
	case MODE_CFGALARM_SETSOUND:
	    switch(btn) {
		case BUTTONS_MENU:
		    pizo_tryalarm_stop();
		    pizo_loadsound();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    pizo_savesound();
		    pizo_tryalarm_stop();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
		    break;
		case BUTTONS_PLUS:
		    pizo_nextsound();
		    pizo_tryalarm_start();
		    mode_update(MODE_CFGALARM_SETSOUND, DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) {
			pizo_tryalarm_stop();
			pizo_loadsound();
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
		    pizo_setvolume(alarm.volume_min, 0);
		    pizo_tryalarm_start();
		}
		mode.tmp[MODE_TMP_MIN] = alarm.volume_min;
		mode.tmp[MODE_TMP_MAX] = alarm.volume_max;
	    }

	    mode_menu_process_button(
		    MODE_TIME_DISPLAY,
		    MODE_CFGALARM_SETSNOOZE_MENU,
		    MODE_CFGALARM_SETVOL,
		    menu_cfgalarm_setvol_init,
		    btn);
	    break;
	case MODE_CFGALARM_SETVOL:
	    switch(btn) {
		case BUTTONS_MENU:
		    pizo_tryalarm_stop();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    if(*mode.tmp < 11) {
			pizo_tryalarm_stop();
			pizo_setvolume(*mode.tmp, 0);
			alarm.volume_min = *mode.tmp;
			alarm.volume_max = *mode.tmp;
			alarm_savevolume();
			mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
		    } else {
			pizo_setvolume(mode.tmp[MODE_TMP_MIN], 0);
			pizo_tryalarm_start();
			mode_update(MODE_CFGALARM_SETVOL_MIN, DISPLAY_TRANS_UP);
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

		    mode_update(MODE_CFGALARM_SETVOL, DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) pizo_tryalarm_stop();
		    break;
	    }
	    break;
	case MODE_CFGALARM_SETVOL_MIN:
	    switch(btn) {
		case BUTTONS_MENU:
		    pizo_tryalarm_stop();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    pizo_setvolume(mode.tmp[MODE_TMP_MAX], 0);
		    pizo_tryalarm_start();
		    mode_update(MODE_CFGALARM_SETVOL_MAX, DISPLAY_TRANS_UP);
		    break;
		case BUTTONS_PLUS:
		    ++mode.tmp[MODE_TMP_MIN];
		    mode.tmp[MODE_TMP_MIN] %= 10;
		    pizo_setvolume(mode.tmp[MODE_TMP_MIN], 0);
		    pizo_tryalarm_start();
		    mode_update(MODE_CFGALARM_SETVOL_MIN,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) pizo_tryalarm_stop();
		    break;
	    }
	    break;
	case MODE_CFGALARM_SETVOL_MAX:
	    switch(btn) {
		case BUTTONS_MENU:
		    pizo_tryalarm_stop();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    pizo_tryalarm_stop();
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
		    pizo_setvolume(mode.tmp[MODE_TMP_MAX], 0);
		    pizo_tryalarm_start();
		    mode_update(MODE_CFGALARM_SETVOL_MAX,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    if(mode.timer == MODE_TIMEOUT) pizo_tryalarm_stop();
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
		    btn);
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
		}
	    };

	    mode_menu_process_button(
		    MODE_TIME_DISPLAY,
#ifdef ADAFRUIT_BUTTONS
		    MODE_CFGALARM_MENU,
#else
		    MODE_CFGALARM_SETSOUND_MENU,
#endif
		    MODE_CFGALARM_SETHEARTBEAT_TOGGLE,
		    menu_cfgalarm_setheartbeat_init,
		    btn);
	    break;
	case MODE_CFGALARM_SETHEARTBEAT_TOGGLE:
	    switch(btn) {
		case BUTTONS_MENU:
		    display.status &= ~DISPLAY_PULSING;
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    display.status &= ~DISPLAY_PULSING;
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
		    }
		    break;
	    }
	    break;
	case MODE_CFGTIME_MENU:
	    mode_menu_process_button(
		    MODE_TIME_DISPLAY,
		    MODE_CFGDISP_MENU,
		    MODE_CFGTIME_SETDST_MENU,
		    NULL,
		    btn);
	    break;
	case MODE_CFGTIME_SETDST_MENU: ;
	    void menu_cfgtime_setdst_init(void) {
		*mode.tmp = time.status;
	    }

	    mode_menu_process_button(
		    MODE_TIME_DISPLAY,
		    MODE_CFGTIME_SETZONE_MENU,
		    MODE_CFGTIME_SETDST_STATE,
		    menu_cfgtime_setdst_init,
		    btn);
	    break;
	case MODE_CFGTIME_SETDST_STATE:
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
				mode_update(MODE_CFGTIME_SETDST_ZONE,
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
		    mode_update(MODE_CFGTIME_SETDST_STATE,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_CFGTIME_SETDST_ZONE:
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
		    mode_update(MODE_CFGTIME_SETDST_ZONE,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_CFGTIME_SETZONE_MENU: ;
	    void menu_cfgtime_setzone_init(void) {
		mode.tmp[MODE_TMP_HOUR]   = gps.rel_utc_hour;
		mode.tmp[MODE_TMP_MINUTE] = gps.rel_utc_minute;
	    }

	    mode_menu_process_button(
		    MODE_TIME_DISPLAY,
		    MODE_CFGTIME_SET12HOUR_MENU,
		    MODE_CFGTIME_SETZONE_HOUR,
		    menu_cfgtime_setzone_init,
		    btn);
	    break;
	case MODE_CFGTIME_SETZONE_HOUR:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    mode_update(MODE_CFGTIME_SETZONE_MINUTE,
			        DISPLAY_TRANS_INSTANT);
		    break;
		case BUTTONS_PLUS:
		    if(mode.tmp[MODE_TMP_HOUR] >= GPS_HOUR_OFFSET_MAX) {
			mode.tmp[MODE_TMP_HOUR] = GPS_HOUR_OFFSET_MIN;
		    } else {
			++mode.tmp[MODE_TMP_HOUR];
		    }
		    mode_update(MODE_CFGTIME_SETZONE_HOUR,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_CFGTIME_SETZONE_MINUTE:
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
		    mode_update(MODE_CFGTIME_SETZONE_MINUTE,
			        DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_CFGTIME_SET12HOUR_MENU: ;
	    void menu_cfgtime_set12hour_init(void) {
		*mode.tmp = time.status;
	    }

	    mode_menu_process_button(
		    MODE_TIME_DISPLAY,
#ifdef ADAFRUIT_BUTTONS
		    MODE_CFGTIME_MENU,
#else
		    MODE_CFGTIME_SETDST_MENU,
#endif
		    MODE_CFGTIME_SET12HOUR,
		    menu_cfgtime_set12hour_init,
		    btn);
	    break;
	case MODE_CFGTIME_SET12HOUR:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    time.status = *mode.tmp;
		    time_savestatus();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
		    break;
		case BUTTONS_PLUS:
		    *mode.tmp ^= TIME_12HOUR;
		    mode_update(MODE_CFGTIME_SET12HOUR, DISPLAY_TRANS_INSTANT);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_CFGDISP_MENU:
	    mode_menu_process_button(
		    MODE_TIME_DISPLAY,
#ifdef ADAFRUIT_BUTTONS
		    MODE_TIME_DISPLAY,
#else
		    MODE_SETALARM_MENU,
#endif
		    MODE_CFGDISP_SETBRIGHT_MENU,
		    NULL,
		    btn);
	    break;
	case MODE_CFGDISP_SETBRIGHT_MENU: ;
	    void menu_cfgdisp_setbright_init(void) {
		mode.tmp[MODE_TMP_MIN] = display.bright_min;
		mode.tmp[MODE_TMP_MAX] = display.bright_max;

		if(display.bright_min == display.bright_max) {
		    *mode.tmp = display.bright_min;
		} else {
		    *mode.tmp = 11;
		}
	    }

	    mode_menu_process_button(
		    MODE_TIME_DISPLAY,
		    MODE_CFGDISP_SETDIGITBRIGHT_MENU,
		    MODE_CFGDISP_SETBRIGHT_LEVEL,
		    menu_cfgdisp_setbright_init,
		    btn);
	    break;
	case MODE_CFGDISP_SETBRIGHT_LEVEL:
	    switch(btn) {
		case BUTTONS_MENU:
		    display_loadbright();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    if(*mode.tmp == 11) {
			*mode.tmp = mode.tmp[MODE_TMP_MIN];
			mode_update(MODE_CFGDISP_SETBRIGHT_MIN,
				    DISPLAY_TRANS_UP);
		    } else {
			display_savebright();
			mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
		    }
		    break;
		case BUTTONS_PLUS:
		    ++(*mode.tmp);
		    *mode.tmp %= 12;
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
	case MODE_CFGDISP_SETDIGITBRIGHT_MENU: ;
	    void menu_cfgdisp_setdigitbright_init(void) {
		*mode.tmp = 0;
	    }

	    mode_menu_process_button(
		    MODE_TIME_DISPLAY,
		    MODE_CFGDISP_SETAUTOOFF_MENU,
		    MODE_CFGDISP_SETDIGITBRIGHT_LEVEL,
		    menu_cfgdisp_setdigitbright_init,
		    btn);
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
		    if(display.digit_times[*mode.tmp] < 2 * UINT8_MAX / 3 ) {
			display.digit_times[*mode.tmp] +=
					display.digit_times[*mode.tmp] >> 1;
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
	case MODE_CFGDISP_SETAUTOOFF_MENU: ;
	    void menu_cfgdisp_setautooff_init(void) {
		*mode.tmp = UINT8_MAX - display.off_threshold;
	    }

	    mode_menu_process_button(
		    MODE_TIME_DISPLAY,
		    MODE_CFGDISP_SETANIMATED_MENU,
		    MODE_CFGDISP_SETAUTOOFF,
		    menu_cfgdisp_setautooff_init,
		    btn);
	    break;
	case MODE_CFGDISP_SETAUTOOFF:
	    switch(btn) {
		case BUTTONS_MENU:
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
		    break;
		case BUTTONS_SET:
		    display.off_threshold = UINT8_MAX - *mode.tmp;
		    display_saveoff();
		    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_UP);
		    break;
		case BUTTONS_PLUS:
		    ++(*mode.tmp);
		    *mode.tmp %= 51;
		    mode_update(MODE_CFGDISP_SETAUTOOFF,
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
		    btn);
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
	default:
	    break;
    }

    if(++mode.timer > MODE_TIMEOUT) {
	mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_DOWN);
    }
}


// set default mode when waking from sleep
void mode_wake(void) {
    mode_update(MODE_TIME_DISPLAY, DISPLAY_TRANS_INSTANT);
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
	case MODE_ALARMIDX_DISPLAY:
	    display_pstr(0, PSTR("alarm"));
	    display_digit(8, *mode.tmp + 1);
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
	    display_digit(8, *mode.tmp + 1);
	    break;
	case MODE_SETALARM_ENABLE:
	    display_pstr(0, PSTR("alar"));
	    if(alarm.days[*mode.tmp] & ALARM_ENABLED) {
		display_pstr(7, PSTR("on"));
		display_dotselect(7, 8);
	    } else {
		display_pstr(6, PSTR("off"));
		display_dotselect(6, 8);
	    }
	    break;
	case MODE_SETALARM_HOUR:
	    mode_alarm_display(alarm.hours[*mode.tmp],
			       alarm.minutes[*mode.tmp]);
	    if(time.status & TIME_12HOUR) {
		display_dotselect(1, 2);
	    } else {
		display_dotselect(2, 3);
	    }
	    break;
	case MODE_SETALARM_MINUTE:
	    mode_alarm_display(alarm.hours[*mode.tmp],
			       alarm.minutes[*mode.tmp]);
	    if(time.status & TIME_12HOUR) {
		display_dotselect(4, 5);
	    } else {
		display_dotselect(5, 6);
	    }
	    break;
	case MODE_SETALARM_DAYS_OPTIONS:
	    switch((uint8_t)mode.tmp[MODE_TMP_DAYS]) {
		case TIME_ALLDAYS | ALARM_ENABLED:
		    display_pstr(0, PSTR("all days"));
		    display_dotselect(1, 3);
		    display_dotselect(5, 8);
		    break;
		case TIME_WEEKDAYS | ALARM_ENABLED:
		    display_pstr(0, PSTR("weekdays"));
		    display_dotselect(1, 8);
		    break;
		case TIME_WEEKENDS | ALARM_ENABLED:
		    display_pstr(0, PSTR("weekends"));
		    display_dotselect(1, 8);
		    break;
		default:
		    display_pstr(0, PSTR(" custom "));
		    display_dotselect(2, 7);
		    break;
	    }
	    break;
	case MODE_SETALARM_DAYS_CUSTOM:
	    mode_daysofweek_display(mode.tmp[MODE_TMP_DAYS]);
	    display_dot(1 + mode.tmp[MODE_TMP_IDX], TRUE);
	    break;
	case MODE_SETTIME_MENU:
	    display_pstr(0, PSTR("set time"));
	    break;
	case MODE_SETTIME_HOUR:
	    mode_time_display(mode.tmp[MODE_TMP_HOUR],
		              mode.tmp[MODE_TMP_MINUTE],
			      mode.tmp[MODE_TMP_SECOND]);

	    if(time.status & TIME_12HOUR
		    && (   mode.tmp[MODE_TMP_HOUR] < 10
		        || (   mode.tmp[MODE_TMP_HOUR] > 12
			    && mode.tmp[MODE_TMP_HOUR] - 12 < 10))) {
		display_dot(2, TRUE);
	    } else {
		display_dotselect(1, 2);
	    }
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
	case MODE_SETDATE_MENU:
	    display_pstr(0, PSTR("set date"));
	    break;
	case MODE_SETDATE_YEAR:
	    display_pstr(0, PSTR("20"));
	    display_digit(3, mode.tmp[MODE_TMP_YEAR] / 10);
	    display_digit(4, mode.tmp[MODE_TMP_YEAR] % 10);
	    display_pstr(6, time_month2pstr(mode.tmp[MODE_TMP_MONTH]));
	    display_dotselect(3, 4);
	    break;
	case MODE_SETDATE_MONTH:
	    display_pstr(0, PSTR("20"));
	    display_digit(3, mode.tmp[MODE_TMP_YEAR] / 10);
	    display_digit(4, mode.tmp[MODE_TMP_YEAR] % 10);
	    display_pstr(6, time_month2pstr(mode.tmp[MODE_TMP_MONTH]));
	    display_dotselect(6, 8);
	    break;
	case MODE_SETDATE_DAY:
	    display_pstr(0, time_month2pstr(mode.tmp[MODE_TMP_MONTH]));
	    display_digit(5, mode.tmp[MODE_TMP_DAY] / 10);
	    display_digit(6, mode.tmp[MODE_TMP_DAY] % 10);
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
	    switch(pizo.status & PIZO_SOUND_MASK) {
		case PIZO_SOUND_MERRY_XMAS:
		    display_pstr(0, PSTR("mery chr"));
		    display_dotselect(1, 8);
		    break;
		case PIZO_SOUND_BIG_BEN:
		    display_pstr(0, PSTR("big ben"));
		    display_dotselect(1, 7);
		    break;
		case PIZO_SOUND_REVEILLE:
		    display_pstr(0, PSTR("reveille"));
		    display_dotselect(1, 8);
		    break;
		default:
		    display_pstr(0, PSTR(" beeps"));
		    display_dotselect(2, 6);
		    break;
	    }
	    break;
	case MODE_CFGALARM_SETVOL_MENU:
	    display_pstr(0, PSTR("a volume"));
	    display_dot(1, TRUE);
	    break;
	case MODE_CFGALARM_SETVOL:
	    if(*mode.tmp == 11) {
		display_pstr(0, PSTR("vol prog"));
		display_dotselect(5, 8);
	    } else {
		mode_textnum_display(PSTR("vol"), *mode.tmp);
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
	    if(*mode.tmp) {
		mode_textnum_display(PSTR("snoz"), *mode.tmp);
	    } else {
		display_pstr(0, PSTR("snoz off"));
		display_dotselect(6, 8);
	    }
	    break;
	case MODE_CFGALARM_SETHEARTBEAT_MENU:
	    display_pstr(0, PSTR("a pulse "));
	    display_dot(1, TRUE);
	    break;
	case MODE_CFGALARM_SETHEARTBEAT_TOGGLE:
	    if(*mode.tmp & ALARM_SOUNDING_PULSE) {
		display_pstr(0, PSTR("puls  on"));
		display_dotselect(7, 8);
	    } else {
		display_pstr(0, PSTR("puls off"));
		display_dotselect(6, 8);
	    }
	    break;
	case MODE_CFGTIME_MENU:
	    display_pstr(0, PSTR("cfg time"));
	    break;
	case MODE_CFGTIME_SETDST_MENU:
	    display_pstr(0, PSTR("set dst"));
	    break;
	case MODE_CFGTIME_SETDST_STATE:
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
	case MODE_CFGTIME_SETDST_ZONE:
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
	case MODE_CFGTIME_SETZONE_MENU:
	    display_pstr(0, PSTR("set zone"));
	    break;
	case MODE_CFGTIME_SETZONE_HOUR:
	    mode_zone_display();
	    display_dotselect(2, 3);
	    break;
	case MODE_CFGTIME_SETZONE_MINUTE:
	    mode_zone_display();
	    display_dotselect(6, 7);
	    break;
	case MODE_CFGTIME_SET12HOUR_MENU:
	    display_pstr(0, PSTR("set 1224"));
	    display_dot(6, TRUE);
	    break;
	case MODE_CFGTIME_SET12HOUR:
	    display_pstr(0, PSTR("24-hour"));
	    if(*mode.tmp & TIME_12HOUR) {
		display_pstr(1, PSTR("12"));
	    }
	    display_dotselect(1, 2);
	    break;
	case MODE_CFGDISP_MENU:
	    display_pstr(0, PSTR("cfg disp"));
	    break;
	case MODE_CFGDISP_SETBRIGHT_MENU:
	    display_pstr(0, PSTR("disp bri"));
	    break;
	case MODE_CFGDISP_SETBRIGHT_LEVEL:
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
	case MODE_CFGDISP_SETBRIGHT_MIN:
	    if(*mode.tmp < 0) {
		// user might not be able to see lowest brightness
		display_loadbright();
	    } else {
		display.bright_min = display.bright_max = *mode.tmp;
		display_autodim();
	    }
	    mode_textnum_display(PSTR("b min"), *mode.tmp);
	    break;
	case MODE_CFGDISP_SETBRIGHT_MAX:
	    display.bright_min = display.bright_max = *mode.tmp;
	    display_autodim();
	    mode_textnum_display(PSTR("b max"), *mode.tmp);
	    break;
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
	case MODE_CFGDISP_SETAUTOOFF:
	    if(*mode.tmp) {
		mode_textnum_display(PSTR("thrsh"), *mode.tmp);
	    } else {
		display_pstr(0, PSTR("alwys on"));
		display_dotselect(1, 5);
		display_dotselect(7, 8);
	    }
	    break;
	case MODE_CFGDISP_SETANIMATED_MENU:
	    display_pstr(0, PSTR("animated"));
	    break;
	case MODE_CFGDISP_SETANIMATED_TOGGLE:
	    if(*mode.tmp & DISPLAY_ANIMATED) {
		display_pstr(0, PSTR("anim  on"));
		display_dotselect(7, 8);
	    } else {
		display_pstr(0, PSTR("anim off"));
		display_dotselect(6, 8);
	    }
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
void mode_time_display(uint8_t hour, uint8_t minute,
		       uint8_t second) {
    uint8_t hour_to_display = hour;

    if(time.status & TIME_12HOUR) {
	if(hour_to_display > 12) hour_to_display -= 12;
	if(hour_to_display == 0) hour_to_display  = 12;
    }

    // display current time
    if(time.status & TIME_12HOUR && hour_to_display < 10) {
	display_clear(1);
    } else {
	display_digit(1, hour_to_display / 10);
    }
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
    display_clear(8);
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
			      void (*init_func)(void), uint8_t btn) {
    switch(btn) {
	case BUTTONS_MENU:
#ifdef ADAFRUIT_BUTTONS
	    mode_update(next, DISPLAY_TRANS_LEFT);
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

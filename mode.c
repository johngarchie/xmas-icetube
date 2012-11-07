#include <avr/pgmspace.h> // for defining program memory strings with PSTR()

#include "mode.h"
#include "display.h"
#include "time.h"
#include "alarm.h"
#include "button.h"


// extern'ed clock mode data
volatile mode_t mode;


// private function declarations
void mode_update(uint8_t new_state);
void mode_time_display(uint8_t hour, uint8_t minute, uint8_t second);
void mode_alarm_display(uint8_t hour, uint8_t minute);
void mode_date_display(void);
void mode_bright_display(void);
void mode_volume_display(void);
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
	    for(uint8_t i = 0; i < 9; ++i) {
		display_clear(i);
	    }
	} else {
	    mode_time_display(time.hour, time.minute, time.second);
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
	    if(++mode.timer > 1000) mode_update(MODE_MONTHDAY_DISPLAY);
	    return;  // use timer ourselves; skip code below
	case MODE_ALARMSET_DISPLAY:
	    if(++mode.timer > 1000) mode_update(MODE_ALARMTIME_DISPLAY);
	    return;  // use timer ourselves; skip code below
	case MODE_MONTHDAY_DISPLAY:
	case MODE_ALARMTIME_DISPLAY:
	case MODE_ALARMOFF_DISPLAY:
	    if(++mode.timer > 1000) mode_update(MODE_TIME_DISPLAY);
	    return;  // use timer ourselves; ignore code below
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
		    time_settime(mode.tmp[MODE_TMP_HOUR],
			         mode.tmp[MODE_TMP_MINUTE],
			         mode.tmp[MODE_TMP_SECOND]);
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
		    mode_update(MODE_MENU_SETFORMAT);
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
		    time_setdate(mode.tmp[MODE_TMP_YEAR],
				 mode.tmp[MODE_TMP_MONTH],
				 mode.tmp[MODE_TMP_DAY]);
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
	case MODE_MENU_SETFORMAT:
	    switch(btn) {
		case BUTTON_MENU:
		    mode_update(MODE_MENU_SETBRIGHT);
		    break;
		case BUTTON_SET:
		    mode.tmp[0] = time.status & TIME_12HOUR;
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
		    if(mode.tmp[0]) {
			time.status |= TIME_12HOUR;
		    } else {
			time.status &= ~TIME_12HOUR;
		    }
		    time_savestatus();

		    // prompt for date format
		    mode.tmp[0] = time.status & TIME_MMDDYY;
		    mode_update(MODE_SETDATE_FORMAT);
		    break;
		case BUTTON_PLUS:
		    mode.tmp[0] = !mode.tmp[0];
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
		    if(mode.tmp[0]) {
			time.status |=  TIME_MMDDYY;
		    } else {
			time.status &= ~TIME_MMDDYY;
		    }
		    time_savestatus();

		    // return to time desplay
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTON_PLUS:
		    mode.tmp[0] = !mode.tmp[0];
		    mode_update(MODE_SETDATE_FORMAT);
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
		    mode.tmp[MODE_TMP_BRIGHT] = display.brightness;
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
		    display.brightness = mode.tmp[MODE_TMP_BRIGHT];
		    display_savebright();
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTON_PLUS:
		    ++(mode.tmp[MODE_TMP_BRIGHT]);
		    *mode.tmp %= 11;
		    display_setbright(mode.tmp[MODE_TMP_BRIGHT]);
		    mode_update(MODE_SETBRIGHT_LEVEL);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_MENU_SETVOLUME:
	    switch(btn) {
		case BUTTON_MENU:
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTON_SET:
		    mode.tmp[MODE_TMP_VOLUME] = alarm.volume;
		    alarm_beep(1000);
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
		    alarm.volume = mode.tmp[MODE_TMP_VOLUME];
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTON_SET:
		    alarm_savevolume();
		    mode_update(MODE_TIME_DISPLAY);
		    break;
		case BUTTON_PLUS:
		    ++alarm.volume;
		    alarm.volume %= 11;
		    alarm_beep(1000);
		    mode_update(MODE_SETVOLUME_LEVEL);
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


// change mode to specified state and update display
void mode_update(uint8_t new_state) {
    mode.timer = 0;
    mode.state = new_state;

    switch(mode.state) {
	case MODE_TIME_DISPLAY:
	    mode_time_display(time.hour, time.minute, time.second);
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
	case MODE_SETTIME_FORMAT:
	    if(mode.tmp[0]) {
		display_pstr(PSTR("12-hour"));
	    } else {
		display_pstr(PSTR("24-hour"));
	    }
	    display_dotselect(1, 2);
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
	case MODE_MENU_SETDATE:
	    display_pstr(PSTR("set date"));
	    break;
	case MODE_SETDATE_FORMAT:
	    if(mode.tmp[0]) {
		display_pstr(PSTR("mm-dd-yy"));
	    } else {
		display_pstr(PSTR("dd-mm-yy"));
	    }
	    display_dotselect(1, 8);
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
	case MODE_MENU_SETFORMAT:
	    display_pstr(PSTR("set form"));
	    break;
	case MODE_MENU_SETBRIGHT:
	    display_pstr(PSTR("set brit"));
	    break;
	case MODE_SETBRIGHT_LEVEL:
	    mode_bright_display();
	    display_dotselect(7, 8);
	    break;
	case MODE_MENU_SETVOLUME:
	    display_pstr(PSTR("set vol "));
	    break;
	case MODE_SETVOLUME_LEVEL:
	    mode_volume_display();
	    display_dotselect(6, 7);
	    break;
	default:
	    break;
    }

    mode.timer = 0;
    mode.state = new_state;
}


// updates the time display every second
void mode_time_display(uint8_t hour, uint8_t minute, uint8_t second) {
    uint8_t hour_to_display = hour;

    if(time.status & TIME_12HOUR) {
	display_dot(0, hour_to_display >= 12);  // show dot if pm
	if(hour_to_display > 12) hour_to_display -= 12;
	if(hour_to_display == 0) hour_to_display  = 12;
    } else {
	display_dot(0, 0);  // never show dot
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


// displays current display brightness
void mode_bright_display(void) {
    display_pstr(PSTR("brite"));
    display_digit(7, mode.tmp[MODE_TMP_BRIGHT] / 10);
    display_digit(8, mode.tmp[MODE_TMP_BRIGHT] % 10);
}


// displays current volume setting
void mode_volume_display(void) {
    display_pstr(PSTR(" vol"));
    display_digit(6, alarm.volume / 10);
    display_digit(7, alarm.volume % 10);
}


// displays current day of the week
void mode_dayofweek_display(void) {
    switch(time_dayofweek(time.year, time.month, time.day)) {
	case TIME_SUN:
	    display_pstr(PSTR("sunday"));
	    break;
	case TIME_MON:
	    display_pstr(PSTR("monday"));
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
	    display_pstr(PSTR("friday"));
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
	    display_pstr(PSTR("jan"));
	    break;
	case TIME_FEB:
	    display_pstr(PSTR("feb"));
	    break;
	case TIME_MAR:
	    display_pstr(PSTR("march"));
	    break;
	case TIME_APR:
	    display_pstr(PSTR("april"));
	    break;
	case TIME_MAY:
	    display_pstr(PSTR("may"));
	    break;
	case TIME_JUN:
	    display_pstr(PSTR("june"));
	    break;
	case TIME_JUL:
	    display_pstr(PSTR("july"));
	    break;
	case TIME_AUG:
	    display_pstr(PSTR("augst"));
	    break;
	case TIME_SEP:
	    display_pstr(PSTR("sept"));
	    break;
	case TIME_OCT:
	    display_pstr(PSTR("octob"));
	    break;
	case TIME_NOV:
	    display_pstr(PSTR("novem"));
	    break;
	case TIME_DEC:
	    display_pstr(PSTR("decem"));
	    break;
	default:
	    break;
    }

    display_digit(7, time.day / 10);
    display_digit(8, time.day % 10);
}

#include "mode.h"

#include "display.h"
#include "time.h"
#include "alarm.h"
#include "button.h"

void mode_update(uint8_t new_state);

void mode_time_display(void);
void mode_setalarm_display(void);
void mode_settime_display(void);
void mode_setdate_display(void);
void mode_setbright_display(void);
void mode_setvolume_display(void);

volatile mode_t mode;

// called on startup; shows time
void mode_init() {
    mode.timer = 0;
    mode_update(MODE_TIME);
}

// called each second; updates current mode as required
void mode_tick(void) {
    switch(mode.state) {
	case MODE_TIME:
	    mode_time_display();
	    break;
	default:
	    break;
    }
}


// called each semisecond; updates current mode as required
void mode_semitick(void) {
    switch(mode.state) {
	case MODE_TIME:
	    // display dash to indicate alarm status
	    display_dash(0, alarm.status & ALARM_SET
	                    && ( !(alarm.status & ALARM_SNOOZE)
		             || time.second % 2));

	    // check for button presses
	    switch(button_process()) {
		case BUTTON_MENU:
		    mode_update(MODE_MENU_SETALARM);
		    break;
		case BUTTON_PLUS:
		case BUTTON_SET:
		    // TODO: show date
		    break;
		default:
		    break;
	    }
	    return;  // no timout, so return
	case MODE_MENU_SETALARM:
	    switch(button_process()) {
		case BUTTON_MENU:
		    mode_update(MODE_MENU_SETTIME);
		    break;
		case BUTTON_SET:
		    mode.tmp[MODE_TMP_HOUR]   = alarm.hour;
		    mode.tmp[MODE_TMP_MINUTE] = alarm.minute;
		    mode_update(MODE_SETALARM_HOUR);
		    break;
		case BUTTON_PLUS:
		    mode_update(MODE_TIME);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETALARM_HOUR:
	    switch(button_process()) {
		case BUTTON_MENU:
		    mode_update(MODE_TIME);
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
	    switch(button_process()) {
		case BUTTON_MENU:
		    mode_update(MODE_TIME);
		    break;
		case BUTTON_SET:
		    alarm_settime(mode.tmp[MODE_TMP_HOUR],
			          mode.tmp[MODE_TMP_MINUTE]);
		    mode_update(MODE_TIME);
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
	    switch(button_process()) {
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
		    mode_update(MODE_TIME);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETTIME_HOUR:
	    switch(button_process()) {
		case BUTTON_MENU:
		    mode_update(MODE_TIME);
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
	    switch(button_process()) {
		case BUTTON_MENU:
		    mode_update(MODE_TIME);
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
	    switch(button_process()) {
		case BUTTON_MENU:
		    mode_update(MODE_TIME);
		    break;
		case BUTTON_SET:
		    time_settime(mode.tmp[MODE_TMP_HOUR],
			         mode.tmp[MODE_TMP_MINUTE],
			         mode.tmp[MODE_TMP_SECOND]);
		    mode_update(MODE_TIME);
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
	    switch(button_process()) {
		case BUTTON_MENU:
		    mode_update(MODE_MENU_SETBRIGHT);
		    break;
		case BUTTON_SET:
		    mode.tmp[MODE_TMP_YEAR]  = time.year;
		    mode.tmp[MODE_TMP_MONTH] = time.month;
		    mode.tmp[MODE_TMP_DAY]   = time.day;

		    if(time.status & TIME_MMDDYY) {
			mode_update(MODE_SETDATE_MONTH);
		    } else {
			mode_update(MODE_SETDATE_DAY);
		    }
		    break;
		case BUTTON_PLUS:
		    mode_update(MODE_TIME);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETDATE_DAY:
	    switch(button_process()) {
		case BUTTON_MENU:
		    mode_update(MODE_TIME);
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
	    switch(button_process()) {
		case BUTTON_MENU:
		    mode_update(MODE_TIME);
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
	    switch(button_process()) {
		case BUTTON_MENU:
		    mode_update(MODE_TIME);
		    break;
		case BUTTON_SET:
		    time_setdate(mode.tmp[MODE_TMP_YEAR],
				 mode.tmp[MODE_TMP_MONTH],
				 mode.tmp[MODE_TMP_DAY]);
		    mode_update(MODE_TIME);
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
	case MODE_MENU_SETBRIGHT:
	    switch(button_process()) {
		case BUTTON_MENU:
		    mode_update(MODE_MENU_SETVOLUME);
		    break;
		case BUTTON_SET:
		    mode.tmp[MODE_TMP_BRIGHT] = display.brightness;
		    mode_update(MODE_SETBRIGHT_LEVEL);
		    break;
		case BUTTON_PLUS:
		    mode_update(MODE_TIME);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETBRIGHT_LEVEL:
	    switch(button_process()) {
		case BUTTON_MENU:
		    display_setbright(display.brightness);
		    mode_update(MODE_TIME);
		    break;
		case BUTTON_SET:
		    display.brightness = mode.tmp[MODE_TMP_BRIGHT];
		    display_savebright();
		    mode_update(MODE_TIME);
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
	    switch(button_process()) {
		case BUTTON_MENU:
		    mode_update(MODE_TIME);
		    break;
		case BUTTON_SET:
		    mode.tmp[MODE_TMP_VOLUME] = alarm.volume;
		    alarm_beep(1000);
		    mode_update(MODE_SETVOLUME_LEVEL);
		    break;
		case BUTTON_PLUS:
		    mode_update(MODE_TIME);
		    break;
		default:
		    break;
	    }
	    break;
	case MODE_SETVOLUME_LEVEL:
	    switch(button_process()) {
		case BUTTON_MENU:
		    alarm.volume = mode.tmp[MODE_TMP_VOLUME];
		    mode_update(MODE_TIME);
		    break;
		case BUTTON_SET:
		    alarm_savevolume();
		    mode_update(MODE_TIME);
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
	mode_update(MODE_TIME);
    }
}


// change mode to specified state and update display
void mode_update(uint8_t new_state) {
    mode.timer = 0;
    mode.state = new_state;

    switch(mode.state) {
	case MODE_TIME:
	    mode_time_display();
	    break;
	case MODE_MENU_SETALARM:
	    display_string("set alar");
	    display_clear(0);
	    break;
	case MODE_SETALARM_HOUR:
	    mode_setalarm_display();
	    display_dot(1, 1);
	    display_dot(2, 1);
	    break;
	case MODE_SETALARM_MINUTE:
	    mode_setalarm_display();
	    display_dot(4, 1);
	    display_dot(5, 1);
	    break;
	case MODE_MENU_SETTIME:
	    display_clear(0);
	    display_string("set time");
	    break;
	case MODE_SETTIME_HOUR:
	    mode_settime_display();
	    display_dot(1, 1);
	    display_dot(2, 1);
	    break;
	case MODE_SETTIME_MINUTE:
	    mode_settime_display();
	    display_dot(4, 1);
	    display_dot(5, 1);
	    break;
	case MODE_SETTIME_SECOND:
	    mode_settime_display();
	    display_dot(7, 1);
	    display_dot(8, 1);
	    break;
	case MODE_MENU_SETDATE:
	    display_clear(0);
	    display_string("set date");
	    break;
	case MODE_SETDATE_DAY:
	    mode_setdate_display();
	    if(time.status & TIME_MMDDYY) {
		display_dot(4, 1);
		display_dot(5, 1);
	    } else {
		display_dot(1, 1);
		display_dot(2, 1);
	    }
	    break;
	case MODE_SETDATE_MONTH:
	    mode_setdate_display();
	    if(time.status & TIME_MMDDYY) {
		display_dot(1, 1);
		display_dot(2, 1);
	    } else {
		display_dot(4, 1);
		display_dot(5, 1);
	    }
	    break;
	case MODE_SETDATE_YEAR:
	    mode_setdate_display();
	    display_dot(7, 1);
	    display_dot(8, 1);
	    break;
	case MODE_MENU_SETBRIGHT:
	    display_clear(0);
	    display_string("set brit");
	    break;
	case MODE_SETBRIGHT_LEVEL:
	    mode_setbright_display();
	    break;
	case MODE_MENU_SETVOLUME:
	    display_clear(0);
	    display_string("set vol ");
	    break;
	case MODE_SETVOLUME_LEVEL:
	    mode_setvolume_display();
	    break;
	default:
	    break;
    }

    mode.timer = 0;
    mode.state = new_state;
}


// updates the time display every second
void mode_time_display(void) {
    uint8_t hour_to_display = time.hour;

    if(time.status & TIME_12HOUR) {
	display_dot(0, hour_to_display >= 12);  // show dot if pm
	if(hour_to_display > 12) hour_to_display -= 12;
	if(hour_to_display == 0) hour_to_display  = 12;
    } else {
	display_dot(0, 0);  // never show dot
    }

    if(time.status & TIME_UNSET && time.second % 2) {
	for(uint8_t i = 1; i < 9; ++i) {
	    display_clear(i);
	}
    } else {
	// display current time
	display_digit(1, hour_to_display / 10);
	display_digit(2, hour_to_display % 10);
	display_clear(3);
	display_digit(4, time.minute / 10);
	display_digit(5, time.minute % 10);
	display_clear(6);
	display_digit(7, time.second / 10);
	display_digit(8, time.second % 10);
    }
}


void mode_setalarm_display(void) {
    uint8_t hour_to_display = mode.tmp[MODE_TMP_HOUR];

    if(time.status & TIME_12HOUR) {
	if(hour_to_display > 12) hour_to_display -= 12;
	if(hour_to_display == 0) hour_to_display  = 12;
    }

    // display current time
    display_clear(0);
    display_digit(1, hour_to_display / 10);
    display_digit(2, hour_to_display % 10);
    display_clear(3);
    display_digit(4, mode.tmp[MODE_TMP_MINUTE] / 10);
    display_digit(5, mode.tmp[MODE_TMP_MINUTE] % 10);
    display_clear(6);

    if(time.status & TIME_12HOUR) {
	if(mode.tmp[MODE_TMP_HOUR] < 12) {
	    display_char(7, 'a');
	    display_char(8, 'm');
	} else {
	    display_char(7, 'p');
	    display_char(8, 'm');
	}
    } else {
	display_clear(7);
	display_clear(8);
    }
}


void mode_settime_display(void) {
    uint8_t hour_to_display = mode.tmp[MODE_TMP_HOUR];

    if(time.status & TIME_12HOUR) {
	display_dot(0, hour_to_display >= 12);  // show dot if pm
	if(hour_to_display > 12) hour_to_display -= 12;
    } else {
	display_dot(0, 0);
    }

    // display current time
    display_digit(1, hour_to_display / 10);
    display_digit(2, hour_to_display % 10);
    display_clear(3);
    display_digit(4, mode.tmp[MODE_TMP_MINUTE] / 10);
    display_digit(5, mode.tmp[MODE_TMP_MINUTE] % 10);
    display_clear(6);
    display_digit(7, mode.tmp[MODE_TMP_SECOND] / 10);
    display_digit(8, mode.tmp[MODE_TMP_SECOND] % 10);
}

void mode_setdate_display(void) {
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


void mode_setbright_display(void) {
    display_string("brite ");
    display_digit(7, mode.tmp[MODE_TMP_BRIGHT] / 10);
    display_digit(8, mode.tmp[MODE_TMP_BRIGHT] % 10);
    display_dot(7, 1);
    display_dot(8, 1);
}


void mode_setvolume_display(void) {
    display_string(" vol    ");
    display_digit(6, alarm.volume / 10);
    display_digit(7, alarm.volume % 10);
    display_dot(6, 1);
    display_dot(7, 1);
}

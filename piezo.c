// piezo.c  --  piezo element control
//
//    PB2 (OC1B)        first  buzzer pin
//    PB1 (OC1A)        second buzzer pin
//    counter/timer1    buzzer pin (OC1A & OC1B) control
//


#include <avr/io.h>       // for using avr register names
#include <avr/power.h>    // for enabling and disabling timer1
#include <avr/eeprom.h>   // for accessing eeprom
#include <util/atomic.h>  // for noninterruptable blocks

#include "piezo.h"
#include "system.h" // alarm behavior depends on power source
#include "usart.h"  // for debugging macros


// extern'ed piezo data
volatile piezo_t piezo;


// macros for each note in an octave
#define Cn 0   // C  (C normal)
#define Cs 1   // C# (C sharp)
#define Df Cs  // Db (D flat)
#define Dn 2   // D  (D normal)
#define Ds 3   // D# (D sharp)
#define Ef Ds  // Eb (E flat)
#define En 4   // E  (E normal)
#define Fn 5   // F  (F normal)
#define Fs 6   // F# (F sharp)
#define Gf Fs  // Gb (G flat)
#define Gn 7   // G  (G normal)
#define Gs 8   // G# (G sharp)
#define Af Gs  // Ab (A flat)
#define An 9   // A  (A normal)
#define As 10  // A# (A sharp)
#define Bf As  // Bb (B flat)
#define Bn 11  // B  (B normal)


// notes are specified with 16 bits
// the octave:    high nibble, high byte
// the note:      low nibble,  high byte
// the duration:  low byte
#define N(note, octave, timing) ((octave << 12) | (note << 8) | timing)
#define NOTE_MASK   0x0F00
#define OCTAVE_MASK 0xF000
#define TIMING_MASK 0x00FF

// other "sounds" are defined within octave zero
// which otherwise does not exits
#define PAUSE(timing)     N(0,0,timing)  // silence

#define BEEP_HIGH(timing) N(1,0,timing)  // 4100 Hz beep
#define BEEP_HIGH_TOP     1951

#define BEEP_LOW(timing)  N(3,0,timing)  // 1367 Hz beep
#define BEEP_LOW_TOP      5854


// The table below is used to convert alarm volume (0 to 10) into timer
// settings.  The values were derived by ear.  With the exception of the
// first two (2 and 7), perceived volume seems roughly proportional to
// the log of the values below.  (cm = compare match)
const uint8_t piezo_vol2cm[] PROGMEM = {2,7,11,15,21,28,38,51,69,93,128};


// timer1 top values for the third octave;
// timer values for other octaves can be derived
// by dividing by powers of two (right bitshifts)
const uint16_t third_octave[] PROGMEM = {
    F_CPU / 130.81,  // C
    F_CPU / 138.59,  // C#, Db
    F_CPU / 146.83,  // D
    F_CPU / 155.56,  // D#, Eb
    F_CPU / 164.81,  // E
    F_CPU / 174.61,  // F
    F_CPU / 185.00,  // F#, Gb
    F_CPU / 196.00,  // G
    F_CPU / 207.65,  // G#, Ab
    F_CPU / 220.00,  // A
    F_CPU / 233.08,  // A#, Bb
    F_CPU / 246.94,  // B
};


// notes and timing of high frequency pulse alarm
const uint16_t pulse_high[] PROGMEM = {
    BEEP_HIGH(3),
    PAUSE(3),
    BEEP_HIGH(3),
    PAUSE(3),
    BEEP_HIGH(3),
    PAUSE(21),
0};


// notes and timing of low frequency pulse alarm
const uint16_t pulse_low[] PROGMEM = {
    BEEP_LOW(3),
    PAUSE(3),
    BEEP_LOW(3),
    PAUSE(3),
    BEEP_LOW(3),
    PAUSE(21),
0};


// the notes and timing of "merry christmas"
const uint16_t merry_xmas[] PROGMEM = {
    N(Dn,6,16),
    N(Gn,6,16), N(Gn,6,8), N(An,6,8), N(Gn,6,8), N(Fs,6,8),
    N(En,6,16), N(En,6,16), N(En,6,16),
    N(An,6,16), N(An,6,8), N(Bn,6,8), N(An,6,8), N(Gn,6,8),
    N(Fs,6,16), N(Dn,6,16), N(Dn,6,16),
    N(Bn,6,16), N(Bn,6,8), N(Cn,7,8), N(Bn,6,8), N(An,6,8),
    N(Gn,6,16), N(En,6,16), N(En,6,8), N(En,6,8),
    N(En,6,16), N(An,6,16), N(Fs,6,16),
    N(Gn,6,32),

    N(Dn,6,16),
    N(Gn,6,16), N(Gn,6,16), N(Gn,6,16),
    N(Fs,6,32), N(Fs,6,16),
    N(Gn,6,16), N(Fs,6,16), N(En,6,16),
    N(Dn,6,32), N(Bn,6,16),
    N(Cn,7,16), N(Bn,6,16), N(An,6,16),
    N(Dn,7,16), N(Dn,6,16), N(Dn,6,8), N(Dn,6,8),
    N(En,6,16), N(An,6,16), N(Fs,6,16),
    N(Gn,6,32), PAUSE(16),
0};


// notes and timing of "for he's a jolly good fellow"
const uint16_t jolly_good[] PROGMEM = {
    N(Dn,7,8),
    N(Bn,6,16), N(Bn,6,8),  N(Bn,6,8), N(An,6,8),  N(Bn,6,8),
    N(Cn,7,24), N(Bn,6,16), N(Bn,6,8),
    N(An,6,16), N(An,6,8),  N(An,6,8), N(Gn,6,8),  N(An,6,8),
    N(Bn,6,24), N(Gn,6,16), N(An,6,8),
    N(Bn,6,16), N(Bn,6,8),  N(Bn,6,8), N(An,6,8),  N(Bn,6,8),
    N(Cn,7,24), N(En,7,16), N(En,7,8),
    N(Dn,7,8),  N(En,7,8),  N(Dn,7,8), N(Cn,7,8),  N(Bn,6,8), N(An,6,8),
    N(Gn,6,24), N(Gn,6,16), N(Bn,6,8),
    N(Dn,7,8),  N(Dn,7,8),  N(Dn,7,8), N(En,7,16), N(En,7,8),

    N(Dn,7,24), N(Dn,7,16), N(Dn,7,8),
    N(Bn,6,8),  N(Bn,6,8),  N(Bn,6,8),  N(Cn,7,16), N(Cn,7,8),
    N(Bn,6,24), N(Bn,6,8),  N(Gn,6,8),  N(An,6,8),
    N(Bn,6,16), N(Bn,6,8),  N(Bn,6,8),  N(An,6,8),  N(Bn,6,8),
    N(Cn,7,24), N(Bn,6,16), N(Bn,6,8),
    N(An,6,16), N(An,6,8),  N(An,6,8),  N(Gn,6,8),  N(An,6,8),
    N(Bn,6,24), N(Gn,6,16), N(An,6,8),
    N(Bn,6,16), N(Bn,6,8),  N(Bn,6,8),  N(An,6,8),  N(Bn,6,8),
    N(Cn,7,16), N(Dn,7,8),  N(En,7,16), N(En,7,8),
    N(Dn,7,8),  N(En,7,8),  N(Dn,7,8),  N(Cn,7,16), N(An,6,8),
    N(Gn,6,24), N(Gn,6,16), PAUSE(8),
    PAUSE(48),
0};


// the notes and timing of "reville" (military roll call)
const uint16_t reveille[] PROGMEM = {
    N(Gn,6,4),
    N(Cn,7,8), N(En,7,4), N(Cn,7,4), N(Gn,6,8), N(En,7,8),
    N(Cn,7,8), N(En,7,4), N(Cn,7,4), N(Gn,6,8), N(En,7,8),
    N(Cn,7,8), N(En,7,4), N(Cn,7,4), N(Gn,6,8), N(Cn,7,8),
    N(En,7,16), N(Cn,7,8), N(Gn,6,8),
    N(Cn,7,8), N(En,7,4), N(Cn,7,4), N(Gn,6,8), N(En,7,8),
    N(Cn,7,8), N(En,7,4), N(Cn,7,4), N(Gn,6,8), N(En,7,8),
    N(Cn,7,8), N(En,7,4), N(Cn,7,4), N(Gn,6,8), N(Gn,6,8),
    N(Cn,7,24), PAUSE(8),
    N(En,7,8),
    N(En,7,8), N(En,7,8), N(En,7,8), N(En,7,8),
    N(Gn,7,16), N(En,7,8), N(Cn,7,8),
    N(En,7,8), N(Cn,7,8), N(En,7,8), N(Cn,7,8),
    N(En,7,16), N(Cn,7,8), N(En,7,8),
    N(En,7,8), N(En,7,8), N(En,7,8), N(En,7,8),
    N(Gn,7,16), N(En,7,8), N(Cn,7,8),
    N(En,7,8), N(Cn,7,8), N(Gn,6,8), N(Gn,6,8),
    N(Cn,7,24), PAUSE(8),
0};


// notes and timing of the big ben chime
const uint16_t big_ben[] PROGMEM = {
    N(Bn,5,32), N(Gn,5,32), N(An,5,32), N(Dn,5,32),
    N(Gn,5,32), N(An,5,32), N(Bn,5,32), N(Gn,5,32),
    N(Bn,5,32), N(An,5,32), N(Gn,5,32), N(Dn,5,32),
    N(Dn,5,32), N(An,5,32), N(Bn,5,32), N(Gn,5,32),
    N(Gn,5,32), N(Gn,5,32), N(Gn,5,32), N(Gn,5,32),
    PAUSE(32),
0};


uint8_t ee_piezo_sound EEMEM = PIEZO_DEFAULT_SOUND;


void piezo_init(void) {
    // configure buzzer pins
    DDRB  |=  _BV(PB2) |  _BV(PB1);  // set as outputs
    PORTB &= ~_BV(PB2) & ~_BV(PB1);  // clamp to ground

    // if any timer is disabled during sleep, the system locks up sporadically
    // and nondeterministically, so enable timer1 in PRR and leave it alone!
    power_timer1_enable();

    piezo_loadsound();
}


// load alarm sound from eeprom
void piezo_loadsound(void) {
    piezo.status = eeprom_read_byte(&ee_piezo_sound) & PIEZO_SOUND_MASK;
    piezo_configsound();
}


// save alarm sound to eeprom
void piezo_savesound(void) {
    eeprom_write_byte(&ee_piezo_sound, piezo.status & PIEZO_SOUND_MASK);
}


// configure needed settings for current sound;
// also ensures valid current sound
void piezo_configsound(void) {
    switch(piezo.status & PIEZO_SOUND_MASK) {
	case PIEZO_SOUND_MERRY_XMAS:
	    piezo.music = merry_xmas;
	    break;
	case PIEZO_SOUND_BIG_BEN:
	    piezo.music = big_ben;
	    break;
	case PIEZO_SOUND_REVEILLE:
	    piezo.music = reveille;
	    break;
	case PIEZO_SOUND_JOLLY_GOOD:
	    piezo.music = jolly_good;
	    break;
	case PIEZO_SOUND_PULSE_HIGH:
	    piezo.music = pulse_high;
	    break;
	case PIEZO_SOUND_PULSE_LOW:
	    piezo.music = pulse_low;
	    break;
	case PIEZO_SOUND_BEEPS_LOW:
	case PIEZO_SOUND_BEEPS_HIGH:
	    break;
	default:
	    piezo.status &= ~PIEZO_SOUND_MASK;
	    piezo.status |=  PIEZO_SOUND_BEEPS_HIGH;
	    break;
    }
}


// change alarm sound
void piezo_nextsound(void) {
    uint8_t next_sound = (piezo.status & PIEZO_SOUND_MASK) + 0x10;
    if(next_sound > PIEZO_SOUND_MAX) next_sound = 0;

    piezo.status &= ~PIEZO_SOUND_MASK;
    piezo.status |= next_sound;

    piezo_configsound();
}


// set piezo volume to vol is [0-10]
// (interpolation between vol and vol+1 using interp)
void piezo_setvolume(uint8_t vol, uint8_t interp) {
    // if sleeping, compensate for reduced voltage by increasing volume
    if((system.status & SYSTEM_SLEEP) && vol < 10) ++vol;

    piezo.cm_max = pgm_read_byte(&(piezo_vol2cm[vol]));

    if(vol < 10 && interp) {
	uint16_t cm_slope = pgm_read_byte(&(piezo_vol2cm[vol+1]))-piezo.cm_max;
	piezo.cm_max += ((cm_slope * interp) >> 8);
    }

    piezo.cm_max <<= 3;
}


// configure piezo control for full-power mode
void piezo_wake(void) {
    if((piezo.status & PIEZO_STATE_MASK) == PIEZO_ALARM_BEEPS) {
	if(TCCR1A) {  // if the buzzer is active,
	    // compensate for 4x faster clock
	    ICR1  <<= 2;
	    OCR1A <<= 2;
	    OCR1B = ICR1 - OCR1A;
	}
    }
}


// silence any nonalarm noises during sleep
void piezo_sleep(void) {
    switch(piezo.status & PIEZO_STATE_MASK) {
	case PIEZO_ALARM_MUSIC:
	    // switch to beeps to reduce power consumption
	    piezo.status &= ~PIEZO_STATE_MASK;
	    piezo.status |=  PIEZO_ALARM_BEEPS;
	    piezo.timer   = 0;
	    piezo_buzzeroff();
	    break;

	case PIEZO_ALARM_BEEPS:
	    if(TCCR1A) {  // if the buzzer is active,
		// compensate for 4x slower clock
		ICR1  >>= 2;
		OCR1A >>= 2;
		OCR1B = ICR1 - OCR1A;
	    }

	case PIEZO_INACTIVE:
	    break;

	default:
	    piezo_stop();
	    break;
    }
}


// toggles buzzer each second
void piezo_tick(void) {
    switch(piezo.status & PIEZO_STATE_MASK) {
	case PIEZO_ALARM_BEEPS:
	    ++piezo.timer;

	    if(piezo.timer & 0x0001) {
		uint16_t sound;
		if(system.status & SYSTEM_SLEEP) {
		    sound = BEEP_HIGH(0);
		} else {
		    switch(piezo.status & PIEZO_SOUND_MASK) {
			case PIEZO_SOUND_BEEPS_LOW:
			    sound = BEEP_LOW(0);
			    break;
			default:
			    sound = BEEP_HIGH(0);
			    break;
		    }
		}
		piezo_buzzeron(sound);
		system.status |= SYSTEM_ALARM_SOUNDING;
	    } else {
		piezo_buzzeroff();
		system.status &= ~SYSTEM_ALARM_SOUNDING;
	    }
	    break;

	default:
	    break;
    }
}


// controls piezo element depending on current state
void piezo_semitick(void) {
    switch(piezo.status & PIEZO_STATE_MASK) {
	case PIEZO_BEEP:
	    // stop buzzer if beep has timed out
	    if(!piezo.timer) piezo_stop();

	    --piezo.timer;

	    break;

	case PIEZO_CLICK:
	    if(piezo.timer == PIEZO_CLICKTIME / 2) {
		// flip from +5v to -5v on piezo
		PORTB |=  _BV(PB2);
		PORTB &= ~_BV(PB1);
	    }

	    if(!piezo.timer) piezo_stop();

	    --piezo.timer;

	    break;

	case PIEZO_TRYALARM_BEEPS:;
            uint16_t sound;
	    if(!piezo.timer) {
		switch(piezo.status & PIEZO_SOUND_MASK) {
		    case PIEZO_SOUND_BEEPS_LOW:
			sound = BEEP_LOW(0);
			break;
		    default:
			sound = BEEP_HIGH(0);
			break;
		}
		piezo_buzzeron(sound);
		piezo.timer = 1600;
	    }

	    if(piezo.timer == 800) {
		piezo_buzzeroff();
	    }

	    --piezo.timer;

	    break;

	case PIEZO_TRYALARM_MUSIC:
	case PIEZO_ALARM_MUSIC:
	    // when timer expires, play next note or pause
	    if(!piezo.timer) {
		uint16_t note = pgm_read_word(&(piezo.music[piezo.pos]));
		piezo.timer = note & TIMING_MASK;

		if(!piezo.timer) {  // zero indicates end-of-tune,
		    piezo.pos = 0;  // so repeat from beginning
		    note = pgm_read_word(&(piezo.music[piezo.pos]));
		    piezo.timer = note & TIMING_MASK;
		}

		piezo.timer <<= 5;  // 128 semiticks per time unit

		// play required note
		piezo_buzzeron(note);

		++piezo.pos; // increment note index
	    }

	    // brief pause to make notes distinct

	    switch(piezo.status & PIEZO_SOUND_MASK) {
		case PIEZO_SOUND_REVEILLE:
		    if(piezo.timer == 32) piezo_buzzeroff();
		    break;
		case PIEZO_SOUND_PULSE_HIGH:
		case PIEZO_SOUND_PULSE_LOW:
		    break;
		default:
		    if(piezo.timer == 64) piezo_buzzeroff();
		    break;
	    }

	    --piezo.timer;

	    break;

	default: // PIEZO_INACTIVE or PIEZO_ALARM_BEEPS
	    break;
    }
}


// enables buzzer with given sound: PAUSE, BEEP, or N(a,b)
void piezo_buzzeron(uint16_t sound) {
    uint16_t top_value;
    uint8_t top_shift;

    switch(sound & ~TIMING_MASK) {
	case PAUSE(0):
	    piezo_buzzeroff();
	    return;
	case BEEP_HIGH(0):
	    top_value = BEEP_HIGH_TOP;
	    top_shift = 0;
	    break;
	case BEEP_LOW(0):
	    top_value = BEEP_LOW_TOP;
	    top_shift = 0;
	    break;
	default:
	    // calculate the number of octaves above the third
	    // (the upper nibble of a note specifies the octave)
	    top_shift = ((sound & OCTAVE_MASK) >> 12) - 3;

	    // find TOP for desired note in the third octave
	    // (the lower nibble of a note specifies the index)
	    top_value = pgm_read_word(&(third_octave[(sound&NOTE_MASK)>>8]));
	    break;
    }

    // shift counter top from third octave to desired octave
    top_value >>= top_shift;

    // determine compare match value for given top
    uint16_t compare_match = (top_value >> 1);

    // reduce compare_match to control volume, when possible
    if(compare_match > piezo.cm_max) compare_match = piezo.cm_max;

    if(system.status & SYSTEM_SLEEP) {
	// compensate frequency for 4x slower clock
	top_value     >>= 2;
	compare_match >>= 2;
    }


    // configure Timer/Counter1 to generate buzzer sound

    // set TOP for desired frequency
    ICR1 = top_value;

    // reset counter
    TCNT1 = 0;

    // set compare match registers for desired volume
    OCR1A = compare_match;
    OCR1B = top_value - compare_match;

    // COM1A1 = 10, clear OC1A on Compare Match, set OC1A at BOTTOM
    // COM1B1 = 11, set OC1B on Compare Match, clear OC1B at BOTTOM
    TCCR1A = _BV(COM1A1) | _BV(COM1B1) | _BV(COM1B0) | _BV(WGM11);

    // WGM1 = 1110, fast PWM, TOP is ICR1
    // CS1  = 001,  clock timer with system clock
    TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS10);
}


// schedule stop-buzzer interrupt on
// next OCR1A compare match (minimizes
// clicking noise at low volume)
void piezo_buzzeroff(void) {
    uint16_t counter_low  = (ICR1 >> 1) - 32;
    uint16_t counter_mid  = (ICR1 >> 1) + 16;

    // pull speaker pins low
    PORTB &= ~_BV(PB2) & ~_BV(PB1);
    
    while(TCCR1B) {
	ATOMIC_BLOCK(ATOMIC_FORCEON) {
	    if(counter_low < TCNT1 && TCNT1 < counter_mid) {
		// wait until counter is midway to ensure OC1A and OC2B
		// are both off (reduces clicking noise)
		while(TCNT1 < counter_mid);

		// disable timer/counter1 (buzzer timer)
		TCCR1A = 0; TCCR1B = 0;
	    }
	}
    }
}


// make a clicking sound
void piezo_click(void) {
    // only click if piezo inactive
    if((piezo.status & PIEZO_STATE_MASK) == PIEZO_INACTIVE) {
	// set state and timer, so click can be completed
	// in subsequent calls to piezo_semitick()
	piezo.status &= ~PIEZO_STATE_MASK;
	piezo.status |=  PIEZO_CLICK;
	piezo.timer   =  PIEZO_CLICKTIME;

	// apply +5v to buzzer
	PORTB |=  _BV(PB1);
	PORTB &= ~_BV(PB2);
    }
}


// beep for the specified duration (semiseconds)
void piezo_beep(uint16_t duration) {
    switch(piezo.status & PIEZO_STATE_MASK) {
	// a beep interrupts anything except alarm sounds
	case PIEZO_ALARM_MUSIC:
	case PIEZO_ALARM_BEEPS:
	case PIEZO_TRYALARM_MUSIC:
	case PIEZO_TRYALARM_BEEPS:
	    return;

	default:
	    // override any existing noise
	    piezo_buzzeroff();

	    // set timer and flag, so beep routine can be
	    // completed in subsequent calls to piezo_semitick()
	    piezo.status &= ~PIEZO_STATE_MASK;
	    piezo.status |=  PIEZO_BEEP;
	    piezo.timer   =  duration;

	    piezo_buzzeron(BEEP_HIGH(0));
	    break;
    }
}


// start alarm sounding
void piezo_alarm_start(void) {
    // override any existing noise
    piezo_buzzeroff();

    // set state
    piezo.status &= ~PIEZO_STATE_MASK;
    
    switch(piezo.status & PIEZO_SOUND_MASK) {
	case PIEZO_SOUND_BEEPS_HIGH:
	case PIEZO_SOUND_BEEPS_LOW:
	   piezo.status |= PIEZO_ALARM_BEEPS;
	   break;

	default:
	   piezo.status |= PIEZO_ALARM_MUSIC;
	   break;
    }

    // reset music poisition and timer
    piezo.pos   = 0;
    piezo.timer = 0;
}


// stop alarm sounding
void piezo_alarm_stop(void) {
    switch(piezo.status & PIEZO_STATE_MASK) {
	// only stop a tryalarm state
	case PIEZO_ALARM_MUSIC:
	case PIEZO_ALARM_BEEPS:
	    // override any existing noise
	    piezo_stop();
	    break;

	default:
	    break;
    }
}


// start alarm sounding demo
void piezo_tryalarm_start(void) {
    switch(piezo.status & PIEZO_STATE_MASK) {
	// a tryalarm interrupts anything except an alarm
	case PIEZO_ALARM_MUSIC:
	case PIEZO_ALARM_BEEPS:
	    return;

	default:
	    piezo_buzzeroff();

	    // set state
	    piezo.status &= ~PIEZO_STATE_MASK;

	    switch(piezo.status & PIEZO_SOUND_MASK) {
		case PIEZO_SOUND_BEEPS_HIGH:
		case PIEZO_SOUND_BEEPS_LOW:
		   piezo.status |= PIEZO_TRYALARM_BEEPS;
		   break;

		default:
		   piezo.status |= PIEZO_TRYALARM_MUSIC;
		   break;
	    }

	    // reset music poisition and timer
	    piezo.pos   = 0;
	    piezo.timer = 0;

	    break;
    }
}


// stop tryalarm noise
void piezo_tryalarm_stop(void) {
    switch(piezo.status & PIEZO_STATE_MASK) {
	// only stop a tryalarm state
	case PIEZO_TRYALARM_MUSIC:
	case PIEZO_TRYALARM_BEEPS:
	    // override any existing noise
	    piezo_stop();
	    break;

	default:
	    break;
    }
}


// silences piezo and changes state to inactive
void piezo_stop(void) {
    // override any existing noise
    piezo_buzzeroff();
    piezo.status  &= ~PIEZO_STATE_MASK;
    piezo.status  |=  PIEZO_INACTIVE;
    system.status &= ~SYSTEM_ALARM_SOUNDING;
}


// returns the name of the current sound as a program memory string
PGM_P piezo_pstr(void) {
    switch(piezo.status & PIEZO_SOUND_MASK) {
	case PIEZO_SOUND_BEEPS_HIGH:
	    return PSTR("beeps hi");
	case PIEZO_SOUND_BEEPS_LOW:
	    return PSTR("beeps lo");
	case PIEZO_SOUND_PULSE_HIGH:
	    return PSTR("pulse hi");
	case PIEZO_SOUND_PULSE_LOW:
	    return PSTR("pulse lo");
	case PIEZO_SOUND_MERRY_XMAS:
	    return PSTR("mery chr");
	case PIEZO_SOUND_BIG_BEN:
	    return PSTR("big ben");
	case PIEZO_SOUND_REVEILLE:
	    return PSTR("reveille");
	case PIEZO_SOUND_JOLLY_GOOD:
	    return PSTR("jly good");
	default:
	    return PSTR("-error-");
    }
}

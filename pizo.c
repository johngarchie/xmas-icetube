// pizo.c  --  pizo element control
//
//    PB2 (OC1B)        first  buzzer pin
//    PB1 (OC1A)        second buzzer pin
//    counter/timer1    buzzer pin (OC1A & OC1B) control
//


#include <avr/io.h>       // for using avr register names
#include <avr/power.h>    // for enabling and disabling timer1
#include <avr/eeprom.h>   // for accessing eeprom
#include <util/atomic.h>  // for noninterruptable blocks

#include "pizo.h"
#include "system.h" // alarm behavior depends on power source


// extern'ed pizo data
volatile pizo_t pizo;


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


// macro for placing each note in an octave: a note
// is stored in the lower nibble; an octave, the upper
#define N(note, octave, timing) ((octave << 12) | (note << 8) | timing)
#define NOTE_MASK   0x0F00
#define OCTAVE_MASK 0xF000
#define TIMING_MASK 0x00FF

// other possible "sounds"
// (since a note must have an octave of at least three,
// any value, less than 48 (3 * 2^8) is not a valid note)
#define PAUSE(timing) (timing)  // silence instead of note
#define PAUSE_MASK  0xFF00      // mask to determine pauses
#define PAUSE_VALUE 0           // mask to determine pauses
#define BEEP        1           // arbitrary beep sound


// The table below is used to convert alarm volume (0 to 10) into timer
// settings.  The values were derived by ear.  With the exception of the
// first two (2 and 7), perceived volume seems roughly proportional to
// the log of the values below.  (cm = compare match)
const uint8_t pizo_vol2cm[] PROGMEM = {2,7,11,15,21,28,38,51,69,93,128};


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


// reville
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


// big ben chime
const uint16_t big_ben[] PROGMEM = {
    N(Bn,5,32), N(Gn,5,32), N(An,5,32), N(Dn,5,32),
    N(Gn,5,32), N(An,5,32), N(Bn,5,32), N(Gn,5,32),
    N(Bn,5,32), N(An,5,32), N(Gn,5,32), N(Dn,5,32),
    N(Dn,5,32), N(An,5,32), N(Bn,5,32), N(Gn,5,32),
    N(Gn,5,32), N(Gn,5,32), N(Gn,5,32), N(Gn,5,32),
    PAUSE(32),
0};


uint8_t ee_pizo_sound EEMEM = PIZO_DEFAULT_SOUND;


void pizo_init(void) {
    // configure buzzer pins
    DDRB  |=  _BV(PB2) |  _BV(PB1);  // set as outputs
    PORTB &= ~_BV(PB2) & ~_BV(PB1);  // clamp to ground

    // if any timer is disabled during sleep, the system locks up sporadically
    // and nondeterministically, so enable timer1 in PRR and leave it alone!
    power_timer1_enable();

    pizo_loadsound();
}


// load alarm sound from eeprom
void pizo_loadsound(void) {
    pizo.status = eeprom_read_byte(&ee_pizo_sound) & PIZO_SOUND_MASK;
    pizo_configsound();
}


// save alarm sound to eeprom
void pizo_savesound(void) {
    eeprom_write_byte(&ee_pizo_sound, pizo.status & PIZO_SOUND_MASK);
}


// configure needed settings for current sound;
// also ensures valid current sound
void pizo_configsound(void) {
    switch(pizo.status & PIZO_SOUND_MASK) {
	case PIZO_SOUND_MERRY_XMAS:
	    pizo.music = merry_xmas;
	    break;
	case PIZO_SOUND_BIG_BEN:
	    pizo.music = big_ben;
	    break;
	case PIZO_SOUND_REVEILLE:
	    pizo.music = reveille;
	    break;
	default:
	    pizo.status &= ~PIZO_SOUND_MASK;
	    pizo.status |=  PIZO_SOUND_BEEPS;
	    break;
    }
}


// change alarm sound
void pizo_nextsound(void) {
    switch(pizo.status & PIZO_SOUND_MASK) {
	case PIZO_SOUND_BEEPS:
	    pizo.status &= ~PIZO_SOUND_MASK;
	    pizo.status |=  PIZO_SOUND_MERRY_XMAS;
	    break;
	case PIZO_SOUND_MERRY_XMAS:
	    pizo.status &= ~PIZO_SOUND_MASK;
	    pizo.status |=  PIZO_SOUND_BIG_BEN;
	    break;
	case PIZO_SOUND_BIG_BEN:
	    pizo.status &= ~PIZO_SOUND_MASK;
	    pizo.status |=  PIZO_SOUND_REVEILLE;
	    break;
	case PIZO_SOUND_REVEILLE:
	    pizo.status &= ~PIZO_SOUND_MASK;
	    pizo.status |=  PIZO_SOUND_BEEPS;
	    break;
	default:
	    pizo.status &= ~PIZO_SOUND_MASK;
	    pizo.status |=  PIZO_SOUND_BEEPS;
	    break;
    }

    pizo_configsound();
}


// set pizo volume to vol is [0-10]
// (interpolation between vol and vol+1 using interp)
void pizo_setvolume(uint8_t vol, uint8_t interp) {
    // if sleeping, compensate for reduced voltage by increasing volume
    if((system.status & SYSTEM_SLEEP) && vol < 10) ++vol;

    pizo.cm_factor = pgm_read_byte(pizo_vol2cm + vol);

    if(vol < 10 && interp) {
	uint16_t cm_slope = pgm_read_byte(pizo_vol2cm+vol+1) - pizo.cm_factor;
	pizo.cm_factor <<= 8;
	pizo.cm_factor += (cm_slope * interp);
    } else {
	pizo.cm_factor <<= 8;
    }
}


// configure pizo control for full-power mode
void pizo_wake(void) {
    if((pizo.status & PIZO_STATE_MASK) == PIZO_ALARM_BEEPS) {
	if(TCCR1A) {  // if the buzzer is active,
	    // compensate for 4x faster clock
	    ICR1  <<= 2;
	    OCR1A <<= 2;
	    OCR1B = ICR1 - OCR1A;
	}
    }
}


// silence any nonalarm noises during sleep
void pizo_sleep(void) {
    switch(pizo.status & PIZO_STATE_MASK) {
	case PIZO_ALARM_MUSIC:
	    // switch to beeps to reduce power consumption
	    pizo.status &= ~PIZO_STATE_MASK;
	    pizo.status |=  PIZO_ALARM_BEEPS;
	    pizo.timer   = 0;
	    pizo_buzzeroff();
	    break;

	case PIZO_ALARM_BEEPS:
	    if(TCCR1A) {  // if the buzzer is active,
		// compensate for 4x slower clock
		ICR1  >>= 2;
		OCR1A >>= 2;
		OCR1B = ICR1 - OCR1A;
	    }

	case PIZO_INACTIVE:
	    break;

	default:
	    pizo_stop();
	    break;
    }
}


// toggles buzzer each second
void pizo_tick(void) {
    switch(pizo.status & PIZO_STATE_MASK) {
	case PIZO_ALARM_BEEPS:
	    ++pizo.timer;

	    if(pizo.timer & 0x0001) {
		pizo_buzzeron(BEEP);
		system.status |= SYSTEM_ALARM_SOUNDING;
	    } else {
		pizo_buzzeroff();
		system.status &= ~SYSTEM_ALARM_SOUNDING;
	    }
	    break;

	default:
	    break;
    }
}


// controls pizo element depending on current state
void pizo_semitick(void) {
    switch(pizo.status & PIZO_STATE_MASK) {
	case PIZO_BEEP:
	    // stop buzzer if beep has timed out
	    if(!pizo.timer) pizo_stop();

	    --pizo.timer;

	    break;

	case PIZO_CLICK:
	    if(pizo.timer == PIZO_CLICKTIME / 2) {
		// flip from +5v to -5v on pizo
		PORTB |=  _BV(PB2);
		PORTB &= ~_BV(PB1);
	    }

	    if(!pizo.timer) pizo_stop();

	    --pizo.timer;

	    break;

	case PIZO_TRYALARM_BEEPS:
	    if(!pizo.timer) {
		pizo_buzzeron(BEEP);
		pizo.timer = 2020;
	    }

	    if(pizo.timer == 1010) {
		pizo_buzzeroff();
	    }

	    --pizo.timer;

	    break;

	case PIZO_TRYALARM_MUSIC:
	case PIZO_ALARM_MUSIC:
	    // when timer expires, play next note or pause
	    if(!pizo.timer) {
		uint16_t note = pgm_read_word(&(pizo.music[pizo.pos]));
		pizo.timer = note & TIMING_MASK;

		if(!pizo.timer) {  // zero indicates end-of-tune,
		    pizo.pos = 0;  // so repeat from beginning
		    note = pgm_read_word(&(pizo.music[pizo.pos]));
		    pizo.timer = note & TIMING_MASK;
		}

		pizo.timer <<= 5;  // 128 semiticks per time unit

		// play required note
		pizo_buzzeron(note);

		++pizo.pos; // increment note index
	    }

	    // brief pause to make notes distinct
	    if((pizo.status & PIZO_SOUND_MASK) == PIZO_SOUND_REVEILLE) {
		if(pizo.timer == 32) pizo_buzzeroff();
	    } else {
		if(pizo.timer == 64) pizo_buzzeroff();
	    }

	    --pizo.timer;

	    break;

	default: // PIZO_INACTIVE or PIZO_ALARM_BEEPS
	    break;
    }
}


// enables buzzer with given sound: PAUSE, BEEP, or N(a,b)
void pizo_buzzeron(uint16_t sound) {
    uint16_t top_value;
    uint8_t top_shift;

    if(sound == BEEP) {
	top_value = 2048;
	top_shift = 0;
    } else if((sound & PAUSE_MASK) == PAUSE_VALUE) {
	pizo_buzzeroff();
	return;
    } else {
	// calculate the number of octaves above the third
	// (the upper nibble of a note specifies the octave)
	top_shift = ((sound & OCTAVE_MASK) >> 12) - 3;

	// find TOP for desired note in the third octave
	// (the lower nibble of a note specifies the index)
	top_value = pgm_read_word(&(third_octave[(sound & NOTE_MASK) >> 8]));
    }

    // shift counter top from third octave to desired octave
    top_value >>= top_shift;

    // determine compare match value for given top
    uint16_t compare_match;
    if(top_value > 1920) {
	// A TOP of 1920 corresponds to 4166 Hz--the resonance
	// of the pizo.  Going beyond that will only reduce volume.
	compare_match = (((uint32_t)1920      * pizo.cm_factor) >> 16);
    } else {
	compare_match = (((uint32_t)top_value * pizo.cm_factor) >> 16);
    }

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
void pizo_buzzeroff(void) {
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
void pizo_click(void) {
    // only click if pizo inactive
    if((pizo.status & PIZO_STATE_MASK) == PIZO_INACTIVE) {
	// set state and timer, so click can be completed
	// in subsequent calls to pizo_semitick()
	pizo.status &= ~PIZO_STATE_MASK;
	pizo.status |=  PIZO_CLICK;
	pizo.timer   =  PIZO_CLICKTIME;

	// apply +5v to buzzer
	PORTB |=  _BV(PB1);
	PORTB &= ~_BV(PB2);
    }
}


// beep for the specified duration (semiseconds)
void pizo_beep(uint16_t duration) {
    switch(pizo.status & PIZO_STATE_MASK) {
	// a beep interrupts anything except alarm sounds
	case PIZO_ALARM_MUSIC:
	case PIZO_ALARM_BEEPS:
	case PIZO_TRYALARM_MUSIC:
	case PIZO_TRYALARM_BEEPS:
	    return;

	default:
	    // override any existing noise
	    pizo_buzzeroff();

	    // set timer and flag, so beep routine can be
	    // completed in subsequent calls to pizo_semitick()
	    pizo.status &= ~PIZO_STATE_MASK;
	    pizo.status |=  PIZO_BEEP;
	    pizo.timer   =  duration;

	    pizo_buzzeron(BEEP);
	    break;
    }
}


// start alarm sounding
void pizo_alarm_start(void) {
    // override any existing noise
    pizo_buzzeroff();

    // set state
    pizo.status &= ~PIZO_STATE_MASK;
    pizo.status |= ((pizo.status & PIZO_SOUND_MASK) == PIZO_SOUND_BEEPS 
	    	        || system.status & SYSTEM_SLEEP ?
		    PIZO_ALARM_BEEPS : PIZO_ALARM_MUSIC);

    // reset music poisition and timer
    pizo.pos   = 0;
    pizo.timer = 0;
}


// stop alarm sounding
void pizo_alarm_stop(void) {
    switch(pizo.status & PIZO_STATE_MASK) {
	// only stop a tryalarm state
	case PIZO_ALARM_MUSIC:
	case PIZO_ALARM_BEEPS:
	    // override any existing noise
	    pizo_stop();
	    break;

	default:
	    break;
    }
}


// start alarm sounding demo
void pizo_tryalarm_start(void) {
    switch(pizo.status & PIZO_STATE_MASK) {
	// a tryalarm interrupts anything except an alarm
	case PIZO_ALARM_MUSIC:
	case PIZO_ALARM_BEEPS:
	    return;

	default:
	    pizo_buzzeroff();

	    // set state
	    pizo.status &= ~PIZO_STATE_MASK;
	    pizo.status |= ((pizo.status & PIZO_SOUND_MASK) == PIZO_SOUND_BEEPS
		            ? PIZO_TRYALARM_BEEPS : PIZO_TRYALARM_MUSIC);

	    // reset music poisition and timer
	    pizo.pos   = 0;
	    pizo.timer = 0;

	    break;
    }
}


// stop tryalarm noise
void pizo_tryalarm_stop(void) {
    switch(pizo.status & PIZO_STATE_MASK) {
	// only stop a tryalarm state
	case PIZO_TRYALARM_MUSIC:
	case PIZO_TRYALARM_BEEPS:
	    // override any existing noise
	    pizo_stop();
	    break;

	default:
	    break;
    }
}


// silences pizo and changes state to inactive
void pizo_stop(void) {
    // override any existing noise
    pizo_buzzeroff();
    pizo.status &= ~PIZO_STATE_MASK;
    pizo.status |=  PIZO_INACTIVE;
    system.status &= ~SYSTEM_ALARM_SOUNDING;
}

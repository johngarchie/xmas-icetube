#ifndef PIEZO_H
#define PIEZO_H

#include <stdint.h>        // for using standard integer types
#include <avr/pgmspace.h>  // for accessing data in program memory

#include "config.h"  // for configuration macros


// duration in semiseconds of the sound made by piezo_click()
#define PIEZO_CLICKTIME 8  // semiseconds


// piezo.status stores the current sound type in the low nibble
// and current alarm sound type (beeps or specific song).
#define PIEZO_STATE_MASK 0x0F
#define PIEZO_SOUND_MASK 0xF0

// piezo.status current sound (lower nibble)
#define PIEZO_INACTIVE        0x00  // doing nothing
#define PIEZO_BEEP            0x01  // playing a beep
#define PIEZO_CLICK           0x02  // making a click
#define PIEZO_ALARM_BEEPS     0x03  // sounding alarm beeps
#define PIEZO_ALARM_MUSIC     0x04  // playing alarm music

// the PIEZO_TRYALARM_* macros are nearly identical to their
// PIEZO_ALARM_* cousins, but are disabled during sleep
#define PIEZO_TRYALARM_BEEPS  0x05  // sounding alarm beeps
#define PIEZO_TRYALARM_MUSIC  0x06  // playing alarm music


// piezo.status alarm type (higher nibble)
#define PIEZO_SOUND_BEEPS_HIGH 0x00
#define PIEZO_SOUND_BEEPS_LOW  0x10
#define PIEZO_SOUND_PULSE_HIGH 0x20
#define PIEZO_SOUND_PULSE_LOW  0x30
#define PIEZO_SOUND_MERRY_XMAS 0x40
#define PIEZO_SOUND_BIG_BEN    0x50
#define PIEZO_SOUND_REVEILLE   0x60
#define PIEZO_SOUND_JOLLY_GOOD 0x70
#define PIEZO_SOUND_MAX        0x70

#define PIEZO_DEFAULT_SOUND PIEZO_SOUND_BEEPS_HIGH


typedef struct {
    uint8_t  status;
    uint16_t cm_max;
    uint16_t timer;

    uint8_t  pos;
    const uint16_t *music;
} piezo_t;


extern volatile piezo_t piezo;


void piezo_init(void);

void piezo_wake(void);
void piezo_sleep(void);

void piezo_tick(void);
void piezo_semitick(void);

void piezo_setvolume(uint8_t vol, uint8_t interp);

void piezo_buzzeron(uint16_t sound);
void piezo_buzzeroff(void);

void piezo_click(void);
void piezo_beep(uint16_t duration);

void piezo_alarm_start(void);
void piezo_alarm_stop(void);

void piezo_loadsound(void);
void piezo_savesound(void);
void piezo_configsound(void);
void piezo_nextsound(void);

void piezo_tryalarm_start(void);
void piezo_tryalarm_stop(void);

void piezo_stop(void);

PGM_P piezo_pstr(void);

#endif

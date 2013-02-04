#ifndef PIZO_H
#define PIZO_H

#include <stdint.h>        // for using standard integer types
#include <avr/pgmspace.h>  // for accessing data in program memory

#include "config.h"  // for configuration macros


// duration in semiseconds of the sound made by pizo_click()
#define PIZO_CLICKTIME 8  // semiseconds


// pizo.status stores the current sound type in the low nibble
// and current alarm sound type (beeps or specific song).
#define PIZO_STATE_MASK 0x0F
#define PIZO_SOUND_MASK 0xF0

// pizo.status current sound (lower nibble)
#define PIZO_INACTIVE        0x00  // doing nothing
#define PIZO_BEEP            0x01  // playing a beep
#define PIZO_CLICK           0x02  // making a click
#define PIZO_ALARM_BEEPS     0x03  // sounding alarm beeps
#define PIZO_ALARM_MUSIC     0x04  // playing alarm music

// the PIZO_TRYALARM_* macros are nearly identical to their
// PIZO_ALARM_* cousins, but are disabled during sleep
#define PIZO_TRYALARM_BEEPS  0x05  // sounding alarm beeps
#define PIZO_TRYALARM_MUSIC  0x06  // playing alarm music


// pizo.status alarm type (higher nibble)
#define PIZO_SOUND_BEEPS      0x00
#define PIZO_SOUND_MERRY_XMAS 0x10
#define PIZO_SOUND_BIG_BEN    0x20
#define PIZO_SOUND_REVEILLE   0x30

#define PIZO_DEFAULT_SOUND PIZO_SOUND_BEEPS


typedef struct {
    uint8_t  status;
    uint8_t  cm_factor;
    uint16_t timer;

    uint8_t  pos;
    const uint16_t *music;
} pizo_t;


extern volatile pizo_t pizo;


void pizo_init(void);

void pizo_wake(void);
void pizo_sleep(void);

void pizo_tick(void);
void pizo_semitick(void);

void pizo_setvolume(uint8_t vol, uint8_t interp);

void pizo_buzzeron(uint16_t sound);
void pizo_buzzeroff(void);

void pizo_click(void);
void pizo_beep(uint16_t duration);

void pizo_alarm_start(void);
void pizo_alarm_stop(void);

void pizo_loadsound(void);
void pizo_savesound(void);
void pizo_configsound(void);
void pizo_nextsound(void);

void pizo_tryalarm_start(void);
void pizo_tryalarm_stop(void);

void pizo_stop(void);

#endif

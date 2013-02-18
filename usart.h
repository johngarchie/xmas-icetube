#ifndef USART_H
#define USART_H

#include <stdint.h>        // for using standard integer types
#include <avr/pgmspace.h>  // for using program memory

#include "config.h"  // for configuration macros

#if defined(DEBUG) || defined(GPS_TIMEKEEPING)

#ifdef DEBUG
// when debugging, dump macros should print to usart

// transmit integer name and value for debugging
#define DUMPINT(VAR) usart_dumpvar(PSTR(#VAR), VAR)

// transmit string for debugging
#define DUMPSTR(STR) usart_dumpstr(PSTR(STR))

#else

// if not debugging, debugging macros expand to nothing
#define DUMPINT(VAR)
#define DUMPSTR(STR)

#endif


void usart_init(void);

void usart_wake(void);
void usart_sleep(void);

inline void usart_tick(void) {};
inline void usart_semitick(void) {};

#ifdef DEBUG
void usart_dumpvar(PGM_P name, int32_t value);
void usart_dumpstr(PGM_P pstr);
#endif

void usart_print_pstr(PGM_P pstr);
void usart_print_int(int32_t n);
void usart_print_ln(void);

int usart_getc(void);
void usart_putc(char c);

#else  // DEBUG || GPS_TIMEKEEPING

void usart_init(void);

inline void usart_wake(void) {};
inline void usart_sleep(void) {};

inline void usart_tick(void) {};
inline void usart_semitick(void) {};

#define DUMPINT(VAR)
#define DUMPSTR(STR)

#endif  // DEBUG || GPS_TIMEKEEPING
#endif  // USART_H

#ifndef USART_H
#define USART_H

#include <stdint.h>        // for using standard integer types
#include <avr/pgmspace.h>  // for using program memory

#define USART_BAUDRATE 9600

#ifdef DEBUG

// when debugging, dump macros should print to usart
#define DUMPVAR(VAR) usart_dumpvar(PSTR(#VAR), VAR)
#define DUMPSTR(STR) usart_dumpstr(PSTR(STR))

#else

// otherwise, dump macros should expand to nothing
#define DUMPVAR(VAR)
#define DUMPSTR(STR)

#endif


void usart_init(void);

void usart_wake(void);
void usart_sleep(void);

#ifdef DEBUG
void usart_dumpvar(PGM_P name, int32_t value);
void usart_dumpstr(PGM_P pstr);
#endif

void usart_print_pstr(PGM_P pstr);
void usart_print_int(int32_t n);
void usart_print_ln(void);

int usart_getc(void);
void usart_putc(char c);

#endif

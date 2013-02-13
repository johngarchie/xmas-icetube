// usart.c  --  manage serial communication (debugging)
//
//    RXD (PD0)    RS232 data input
//    TXD (PD1)    RS232 data output
//    usart0       usart module
//

#include <avr/io.h>     // for using register names
#include <avr/power.h>  // for enabling and disabling usart

#include "usart.h"
#include "config.h"  // for configuration macros


#if defined(DEBUG) || defined(GPS_TIMEKEEPING)

// initialize usart after system reset
void usart_init(void) {
    // configure PD0 and PD1 (rxd, txd);
    // settings are overridden when usart is active
    DDRD  |=  _BV(PD1) |  _BV(PD0);  // configure as output
    PORTD &= ~_BV(PD1) & ~_BV(PD0);  // clamp to ground
}


// enable usart while awake
void usart_wake(void) {
    power_usart0_enable();  // enable usart

    // set desired baudrate in terms of system clock
    UBRR0 = F_CPU / (USART_BAUDRATE * 16UL) - 1;

    // standard serial port settings: 8 bits, no parity, 1 stop
    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);

    // enable transmitter and receiver
    UCSR0B = _BV(RXEN0)  | _BV(TXEN0);
}


// disable usart during sleep
void usart_sleep(void) {
    // diable usart: rxd and txd are clamped to ground
    power_usart0_disable();
}


#ifdef DEBUG
// write a string and newline to usart
void usart_dumpstr(PGM_P pstr) {
    usart_print_pstr(pstr);
    usart_print_ln();
}
#endif


#ifdef DEBUG
// write a program-memory string to usart
void usart_dumpvar(PGM_P name, int32_t value) {
    usart_print_pstr(name);
    usart_print_pstr(PSTR(": "));
    usart_print_int(value);
    usart_print_ln();
}
#endif


// write a program-memory string to usart
void usart_print_pstr(PGM_P pstr) {
    uint8_t i = 0;
    char c = pgm_read_byte(&(pstr[i]));

    while(c) {
	usart_putc(c);
	c = pgm_read_byte(&(pstr[++i]));
    }
}


// print an integer to usart
void usart_print_int(int32_t n) {
    if(n < 0) {
	usart_putc('-');
	n *= -1;
    }

    uint8_t print = 0;

    for(uint32_t order = 1000000000UL; order; order /= 10) {
	uint8_t digit = n / order;
	if(print || digit) {
	    usart_putc('0' + digit);
	    print = 1;
	}
	n %= order;
    }

    if(!print) {
	usart_putc('0');
    }
}


// print newline to usart
void usart_print_ln(void) {
    usart_putc('\n');
    usart_putc('\r');
}


// write single character to usart
void usart_putc(char c) {
    while(!(UCSR0A & _BV(UDRE0)));
    UDR0 = c;
}


// read single character from usart
int usart_getc(void) {
    if(UCSR0A & _BV(RXC0)) {
	return UDR0;
    } else {
	return -1;
    }
}

#else  // DEBUG || GPS_TIMEKEEPING

// initialize usart after system reset
void usart_init(void) {
    // enable pull-ups on PD0 and PD1 (rxd, txd);
    PORTD |= _BV(PD1) | _BV(PD0);
}

#endif  // DEBUG || GPS_TIMEKEEPING

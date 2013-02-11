// temp.c  --  acquires measurements from temperature sensor
//
//    PC2        1-Wire bus
//

#include "config.h"

#ifdef TEMPERATURE_SENSOR

#include <avr/io.h>
#include <util/delay.h>
#include <util/atomic.h>
#include <avr/wdt.h>

#include "temp.h"
#include "usart.h"

#define TEMP_CMD_SKIPROM     0xCC
#define TEMP_CMD_CONVERTTEMP 0x44
#define TEMP_CMD_RSCRATCHPAD 0xBE

// extern'ed temperature data
volatile temp_t temp;


uint8_t temp_reset(void);

void temp_write_bit(uint8_t);
uint8_t temp_read_bit(void);

void temp_write_byte(uint8_t);
uint8_t temp_read_byte(void);

uint8_t temp_read_scratch(void);


void temp_tick(void) {
    static uint8_t temp_timer = 1;
    if(--temp_timer) return;
    temp_timer = 30;

    // request temperature conversion
    if(!temp_reset()) return;
    temp_write_byte(TEMP_CMD_SKIPROM);
    temp_write_byte(TEMP_CMD_CONVERTTEMP);

    // wait for conversion to complete
    while(!temp_read_bit());

    // read conversion
    if(!temp_reset()) return;
    temp_write_byte(TEMP_CMD_SKIPROM);
    temp_write_byte(TEMP_CMD_RSCRATCHPAD);
    temp_read_scratch();

    DUMPINT(temp_degC());
    DUMPINT(temp_degF());
}


// returns current temperature in degrees celsius
int16_t temp_degC(void) {
    int16_t degC = temp.temp;
    degC += 0x08;
    degC >>= 4;
    return degC;
}


// returns current temperature in degrees Farenheit
int16_t temp_degF(void) {
    int16_t degF = temp.temp / 5 * 9 + 512;
    degF += 0x08;
    degF >>= 4;
    return degF;
}


// resets the 1-Wire bus
// returns true if successful; false otherwise
uint8_t temp_reset(void) {
    uint8_t response = 0;

    // pull low for at least 500 us
    DDRC |= _BV(PC2);  // set as output
    _delay_us(500);

    // release bus and check device response (timing critical)
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
	DDRC &= ~_BV(PC2);  // set as input
	_delay_us(80);      // wait for response
	if(PINC & _BV(PC2)) response = 1;  // query bus
    }

    // additional 420 us delay, so bus released for at least 500 us
    _delay_us(420);

    // return true if reset successful
    // (response is low/false if successful)
    return !response;
}


// writes a bit to the 1-Wire bus:
// if bit is true, writes 1; if false, writes 0
void temp_write_bit(uint8_t bit) {
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
	PORTC &= ~_BV(PC2);  // disable pull-up
	DDRC  |=  _BV(PC2);  // pull low
	_delay_us(10);

	if(bit) {  // sending 1
	    PORTC |= _BV(PC2);  // pull high
	    _delay_us(55);      // for 55 us
	} else {  // sending 0
	    _delay_us(55);  // remain low for 55 us
	    PORTC |= _BV(PC2);  // pull high
	}

	_delay_us(5);  // 5 us recovery time

	DDRC  &= ~_BV(PC2);  // set to input
	PORTC &= ~_BV(PC2);  // disable pull-up
    }
}


// requests a bit from the 1-Wire bus;
// returns the bit (1 or 0)
uint8_t temp_read_bit(void) {
    uint8_t bit = 0;

    ATOMIC_BLOCK(ATOMIC_FORCEON) {
	// initiate read timeslot
	PORTC &= ~_BV(PC2);      // disable pull-up
	DDRC  |=  _BV(PC2);      // pull low
	_delay_us(3);

	// wait for and read response
	DDRC &= ~_BV(PC2);       // set to input
	_delay_us(10);           // wait for response
	if(PINC & _BV(PC2)) bit = 1;  // query bus
    }

    // 52 us to complete 65 us timeslot with 5us recovery time
    _delay_us(57);

    return bit;
}


// writes a byte to the 1-Wire bus
void temp_write_byte(uint8_t byte) {
    for(uint8_t i = 0; i < 8; ++i, byte >>= 1) {
	temp_write_bit(byte & 0x01);
    }
}


// reads a byte from the 1-Wire bus
uint8_t temp_read_byte(void) {
    uint8_t byte = 0;

    for(uint8_t bit = 0x01; bit; bit <<= 1) {
	if(temp_read_bit()) byte |= bit;
    }

    return byte;
}


// reads the entire scratchpad
uint8_t temp_read_scratch(void) {
    uint8_t calculated_crc = 0;
    int16_t reported_temp  = 0;

    for(uint8_t i = 0; i < 8; ++i) {
	uint8_t byte = temp_read_byte();

	switch(i) {
	    case 0:  // temperature lsb
		reported_temp |= byte;
		break;
	    case 1:  // temperature msb
		reported_temp |= ((uint16_t)byte << 8);
		break;
	    default:
		break;
	}

	// update crc with newly ready byte
	for(uint8_t j = 0; j < 8; ++j, byte >>= 1) {
	    byte ^= calculated_crc & 0x01;
	    calculated_crc >>= 1;
	    if(byte & 0x01) calculated_crc ^= 0x8C;
	}
    }

    uint8_t reported_crc = temp_read_byte();

    if(calculated_crc == reported_crc) {
	ATOMIC_BLOCK(ATOMIC_FORCEON) {
	    temp.temp = reported_temp + TEMPERATURE_ADJUST;
	}

	return 1;
    } else {
	return 0;
    }
}

#endif  // TEMPERATURE_SENSOR

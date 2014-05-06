// temp.c  --  acquires measurements from temperature sensor
//
//    PC1        1-Wire bus
//

#include "config.h"

#ifdef TEMPERATURE_SENSOR

#include <avr/io.h>
#include <util/delay.h>
#include <util/atomic.h>
#include <avr/wdt.h>

#include "temp.h"
#include "time.h"
#include "usart.h"
#include "system.h"

#define TEMP_CMD_SKIPROM     0xCC
#define TEMP_CMD_CONVERTTEMP 0x44
#define TEMP_CMD_RSCRATCHPAD 0xBE

// extern'ed temperature data
volatile temp_t temp;


void temp_start_conv(void);
void temp_read_conv(void);
void temp_calc_error(void);

uint8_t temp_reset(void);

void temp_write_bit(uint8_t);
uint8_t temp_read_bit(void);

void temp_write_byte(uint8_t);
uint8_t temp_read_byte(void);

void temp_power_bus(void);

uint8_t temp_read_scratch(void);


// initialize timekeeping variables
void temp_init(void) {
    temp.status     = 0;
    temp.int_timer  = 0;
    temp.conv_timer = 0;
    temp.adjust     = 0;
    temp.error      = 0;
    temp.temp       = TEMP_INVALID;  // invalid temperature
}


void temp_sleep(void) {
    // disable output on the one-wire bus
    DDRC  &= ~_BV(PC1);  // set as input
    PORTC &= ~_BV(PC1);  // disable pull-up

    // mark current conversion as invalid
    temp.status |= TEMP_CONV_INVALID;
}


// query temperature probe as needed, once per second
void temp_tick(void) {
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
	++temp.int_timer;
    }

    // sample temperature if awake
    if(!(system.status & SYSTEM_SLEEP)) {

	// prevent interruption if communication already in progress
	cli();
	if(temp.status & TEMP_COMM_LOCK) {
	    sei();
	    return;
	} else {
	    temp.status |= TEMP_COMM_LOCK;
	    sei();
	}

	// check previous temperature conversion as required
	if((temp.status & TEMP_CONV_STARTED) && !--temp.conv_timer) {
	    temp_read_conv();
	    ATOMIC_BLOCK(ATOMIC_FORCEON) {
		if(!(temp.status & TEMP_CONV_INVALID)) {
		    temp_calc_error();
		    temp.int_timer = 0;
		}
		temp.status &= ~TEMP_CONV_STARTED;
		temp.status &= ~TEMP_CONV_INVALID;
	    }
	}

	// start new temperature conversion as required
	if(!(temp.status & TEMP_CONV_STARTED)
		|| (temp.status & TEMP_CONV_INVALID)) {
	    temp.status |=  TEMP_CONV_STARTED;
	    temp.status &= ~TEMP_CONV_INVALID;
	    temp.conv_timer = TEMP_CONV_INTERVAL;
	    temp_start_conv();
	}
    }

    // release sensor communication lock
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
	temp.status &= ~TEMP_COMM_LOCK;
    }
}


// starts a new temperature conversion
void temp_start_conv(void) {
    if(temp_reset()) {
	temp_write_byte(TEMP_CMD_SKIPROM);
	temp_write_byte(TEMP_CMD_CONVERTTEMP);
	temp_power_bus();
    } else {
	temp.status |= TEMP_CONV_INVALID;
    }
}


// reads result of temperature conversion from scratchpad
void temp_read_conv(void) {
    if(temp_reset()) {
	temp_write_byte(TEMP_CMD_SKIPROM);
	temp_write_byte(TEMP_CMD_RSCRATCHPAD);
	if(!temp_read_scratch()) {
	    temp.status |= TEMP_CONV_INVALID;
	} else {
	    DUMPINT(temp_degC());
	    DUMPINT(temp_degF());
	}
    } else {
	temp.status |= TEMP_CONV_INVALID;
    }
}


// calculate timekeeping error using last known temperature
// (temp.temp) and time interval (temp.int_timer)
void temp_calc_error(void) {
    int32_t error;
    if(temp.int_timer < 300) {
	// use accurate method for small time intervals
	// to minimize rounding error
	error  = XTAL_TURNOVER_TEMP - temp.temp;
	error *= error;
	error *= XTAL_FREQUENCY_COEF;
        error *= temp.int_timer;
	temp.error += error;
    } else {
	// use less accurate method for large time intervals
	// to prevent overflow (when the clock has been
	// sleeping for more than about five minutes)
	error  = (XTAL_TURNOVER_TEMP - (temp.temp + 0x08)) >> 4;
	error *= error;
	error *= XTAL_FREQUENCY_COEF;
        error *= temp.int_timer;
	while(error > (1000000000UL >> 7)) {
	    ATOMIC_BLOCK(ATOMIC_FORCEON) {
		if(temp.adjust == 127) {
		    temp.adjust = 0;
		    time_tick();
		} else {
		    ++temp.adjust;
		}
	    }
	    error -= (1000000000UL >> 7);
	}
	temp.error += (error << 8);
    }

    while(temp.error > (1000000000UL << 1)) {
	ATOMIC_BLOCK(ATOMIC_FORCEON) {
	    ++temp.adjust;
	}
	temp.error -= (1000000000UL << 1);
    }
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
    PORTC &= ~_BV(PC1);  // pull low
    DDRC  |=  _BV(PC1);  // set as output
    _delay_us(500);

    // release bus and check device response (timing critical)
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
	DDRC &= ~_BV(PC1);  // set as input
	_delay_us(80);      // wait for response
	if(PINC & _BV(PC1)) response = 1;  // query bus
	temp.missed_ovf += 2;
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
	PORTC &= ~_BV(PC1);  // disable pull-up
	DDRC  |=  _BV(PC1);  // pull low
	_delay_us(10);

	if(bit) {  // sending 1
	    PORTC |= _BV(PC1);  // pull high
	    _delay_us(55);      // for 55 us
	} else {  // sending 0
	    _delay_us(55);  // remain low for 55 us
	    PORTC |= _BV(PC1);  // pull high
	}

	_delay_us(5);  // 5 us recovery time
	temp.missed_ovf += 2;
    }
}


// requests a bit from the 1-Wire bus;
// returns the bit (1 or 0)
uint8_t temp_read_bit(void) {
    uint8_t bit = 0;

    ATOMIC_BLOCK(ATOMIC_FORCEON) {
	// initiate read timeslot
	PORTC &= ~_BV(PC1);      // disable pull-up
	DDRC  |=  _BV(PC1);      // pull low
	_delay_us(3);

	// wait for and read response
	DDRC &= ~_BV(PC1);       // set to input
	_delay_us(10);           // wait for response
	if(PINC & _BV(PC1)) bit = 1;  // query bus
    }

    // 50 us to complete 60 us timeslot
    _delay_us(47);

    // pull bus high for 5 us recovery time
    PORTC |=  _BV(PC1);      // enable pull-up
    DDRC  |=  _BV(PC1);      // pull low
    _delay_us(5);

    return bit;
}


// writes a byte to the 1-Wire bus
void temp_write_byte(uint8_t byte) {
    for(uint8_t i = 0; i < 8; ++i, byte >>= 1) {
	temp_write_bit(byte & 0x01);
	if(temp.status & TEMP_CONV_INVALID) return;
    }
}


// reads a byte from the 1-Wire bus
uint8_t temp_read_byte(void) {
    uint8_t byte = 0;

    for(uint8_t bit = 0x01; bit; bit <<= 1) {
	if(temp_read_bit()) byte |= bit;
	if(temp.status & TEMP_CONV_INVALID) return 0;
    }

    return byte;
}


// sends power via the bus (sensor wired in parasitic mode)
void temp_power_bus(void) {
    PORTC |= _BV(PC1);  // enable pull-up
    DDRC  |= _BV(PC1);  // push high
}


// reads the entire scratchpad
uint8_t temp_read_scratch(void) {
    uint8_t calculated_crc = 0;
    int16_t new_temp  = 0;

    for(uint8_t i = 0; i < 8; ++i) {
	uint8_t byte = temp_read_byte();
	if(temp.status & TEMP_CONV_INVALID) return 0;

	switch(i) {
	    case 0:  // temperature lsb
		new_temp |= byte;
		break;
	    case 1:  // temperature msb
		new_temp |= ((uint16_t)byte << 8);
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

    if(!(temp.status & TEMP_CONV_INVALID)
	    && calculated_crc == reported_crc) {

	ATOMIC_BLOCK(ATOMIC_FORCEON) {
	    temp.temp = new_temp;
	}

	return 1;
    } else {
	return 0;
    }
}

#endif  // TEMPERATURE_SENSOR

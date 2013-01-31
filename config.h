#ifndef CONFIG_H
#define CONFIG_H

// ADAFRUIT-STYLE BUTTONS
//
// By default this xmas-icetube uses a unique button scheme:
// The "menu" button means "enter menu" or "exit menu."  Meanwhile,
// the "+" button means "next menu item," "next selected option",
// or "increment selected number."
//
// The following macro enables an Adafruit-style buttons where
// the "menu" button means "enter menu," "next menu item", or
// "exit menu." Meanwhile the "+" button can mean "exit menu,"
// "next selected option," or "increment selected number."
//
#define ADAFRUIT_BUTTONS


// TEMPERATURE COMPENSATED CRYSTAL OSCILLATOR HACK
//
// The following macro should be defined if using an external
// 32.768 kHz clock source for timekeeping such as a Maxim DS32kHz.
// This modification for high accuracy timekeeping is discussed in
// detail on the Adafruit Clocks forum:
// http://forums.adafruit.com/viewtopic.php?f=41&t=14941
//
// #define EXTERNAL_CLOCK


// PICO POWER HACK
//
// Defining the following macro will enable MCU power savings features
// for use with the hardware pico-power hack posted to the Adafruit
// Clocks forum:
// 
// #define PICO_POWER


// DEBUGGING FEATURES
//
// The following macro enables debugging.  When enabled, debugging
// information may be sent over USART via the DUMPINT() and DUMPSTR()
// macros defined in usart.h.  The default speed is 9600 baud.
//
// #define DEBUG


#endif  // CONFIG_H

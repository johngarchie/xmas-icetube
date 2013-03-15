#ifndef CONFIG_H
#define CONFIG_H

// ADAFRUIT-STYLE BUTTONS
//
// By default this xmas-icetube uses a unique button scheme:
// The "menu" button means "enter menu" or "exit menu."  Meanwhile,
// the "+" button means "next menu item," "next selected option",
// or "increment selected number."
//
// The following macro enables Adafruit-style buttons where
// the "menu" button means "enter menu," "next menu item", or
// "exit menu." Meanwhile the "+" button can mean "exit menu,"
// "next selected option," or "increment selected number."
//
//
#define ADAFRUIT_BUTTONS


// AUTOMATIC DIMMER HACK
//
// Defining the following macro enables support for Automatic dimming
// when a 10-20k resister and CdS photoresistor are installed in R3 and
// CT1, adjacent to the ATMEGA's PC5 pin.  This hack also allows for the
// clock to be disabled at night (when dark).  For more details, visit
// the Adafruit Clocks forum:
//
//   http://forums.adafruit.com/viewtopic.php?f=41&t=12932
//
//
// #define AUTOMATIC_DIMMER


// GPS TIMEKEEPING
//
// Defining the following macro enables GPS detection on the ATMEGA's
// RX pin.  The connection speed is 9600 baud by default for
// compatibility with the Adafruit Ultimate GPS Module.  For more
// information, check out the following sites:
//
//   http://forums.adafruit.com/viewtopic.php?f=41&t=36873
//   http://www.ladyada.net/make/icetube/mods.html
//   http://forums.adafruit.com/viewtopic.php?f=41&t=32660
//
//
// #define GPS_TIMEKEEPING


// TEMPERATURE COMPENSATED CRYSTAL OSCILLATOR
//
// The following macro should be defined if using an external
// 32.768 kHz clock source for timekeeping such as a Maxim DS32kHz.
// This hack is discussed in detail on the Adafruit Clocks forum:
//
//   http://forums.adafruit.com/viewtopic.php?f=41&t=14941
//
//
// #define EXTERNAL_CLOCK


// EXTENDED BATTERY HACK
//
// Normally, the Ice Tube Clock uses 47 uA during sleep, so the clock
// can keep time for about five weeks on a typical 40 mAh CR1220 battery.
// With a hardware and software modification, sleep current can be
// reduced to 1.7 uA.  That's around two and a half years of sleep!
//
// That this hack is compatible with the DS32kHz hack above, but
// sleep current will only fall to 7 uA--about 7 months of sleep.
//
// Note that this hack does change the required fuse settings, so
// after enabling or disabling this option, be sure to do both
// "make install-fuse" and "make install".
//
// Form more information see the Adafruit Clocks forum:
//
//   http://forums.adafruit.com/viewtopic.php?f=41&t=36697
//
//
// #define PICO_POWER


// DISPLAY BRIGHTNESS / BOOST CONFIGURATION
//
// VFD displays lose brightness as they age, but increasing the
// grid/segment voltage can extend the useful life.  This voltage
// is controlled by the OCR0A register, and OCR0A is set by
//
//   OCR0A = OCR0A_MIN + OCR0A_SCALE * brightness
//
// where brightness is 0-10 as set through the configuration menu.
// The grid/segment voltage can be estimated by
//
//   voltage [approximately equals] OCR0A / 4 + 6
//
// The IV-18 display has an absolute maximum grid/segment voltage of
// 70 volts, but on the Ice Tube Clock, a Zener diode prevents this
// from exceeding 60 volts.  And the clock's fuse will probably kick
// in well before that point.  Thus the hardware will ensure that
// the grid/segment voltage maximum is never exceeded.
//
// With a dim display, it might also be necessary to increase current
// across the VFD tube filament.  Current can be increased slightly by
// replacing R3 with a jumper:
//
//   http://forums.adafruit.com/viewtopic.php?f=41&t=23586&start=30
//
// To further increase filament current, replace Q3 with a PNP transister
// with a 200 ohm resistor at the base.  Current can be adjusted by
// replacing R3 with a 100 ohm potentiometer:
//
//   http://forums.adafruit.com/viewtopic.php?f=41&t=23586&start=43
//
// For a dim display, I suggest setting OCR0A_MIN to 30 and
// OCR0A_SCALE to 14.  If the fuse becomes warm during operation,
// reduce OCR0A_SCALE or install a higher power fuse.
//
//
#define OCR0A_MIN   20
#define OCR0A_SCALE  7
#define OCR0A_MAX OCR0A_MIN + 10 * OCR0A_SCALE


// DEBUGGING FEATURES
//
// The following macro enables debugging.  When enabled, debugging
// information may be sent over USART via the DUMPINT() and DUMPSTR()
// macros defined in usart.h.  The default speed is 9600 baud.
//
//
// #define DEBUG


// USART BAUD RATE
//
// The USART baud rate is used for both the debugging features and GPS
// timekeeping and may be changed below.
//
//
#define USART_BAUDRATE 9600


#endif  // CONFIG_H

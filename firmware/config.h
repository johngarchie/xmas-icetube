#ifndef CONFIG_H
#define CONFIG_H

// XMAS-ICETUBE CLOCK DESIGN
//
// The xmas-icetube redesign of the Ice Tube Clock requires a few
// firmware modifications.  The XMAS_DESIGN macro enables these
// modifications, but breaks compatibility with the Adafruit Ice Tube
// Clock v1.1.
//
// When configuring this firmware for use with the xmas-icetube clock
// design, the configuration macros for the following features should
// also be enabled: the automatic dimmer hack, software temperature
// compensated timekeeping, and the IV-18 to-spec hack.
//
//
// #define XMAS_DESIGN


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
// #define ADAFRUIT_BUTTONS


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


// SOFTWARE TEMPERATURE COMPENSATED TIMEKEEPING
//
// This hack requires attaching a DS18B20 OneWire temperature sensor
// to ATMEGA328P PC1 pin, but unfortunately the internal temperature
// of the clock is above the ambiant temperature.  As a result, this
// modification is not useful for displaying the current temperature.
//
// This modification is useful for temperature compensated
// timekeeping, however.  The XTAL_TURNOVER_TEMP macro specifies the
// temperature at which the crystal oscillates at maximum frequency in
// units of deg C / 16.  The XTAL_FREQUENCY_COEF macro specifies the
// parabolic temperature dependence of the crystal in units -ppb /
// (deg C)^2.  For a turnover temperature of 25 deg C and a frequency
// coefficient of -0.034 ppm / (deg C)^2, XTAL_TURNOVER_TEMP should be
// defined as 400 (25 * 16), and XTAL_FREQUENCY_COEF should be defined
// as 34 (-0.034 * -1000).
//
// This technique for temperature compensation is described in the
// following thread:
//
//   http://forums.adafruit.com/viewtopic.php?f=41&t=43998
//
// WARNING:  If your clock is modified for the original extended
// battery hack, which shorts the ATMEGA328P PC1 pin to ground,
// enabling the macros below might damage your clock!  The original
// extended battery hack is described in the following thread:
//
//   http://forums.adafruit.com/viewtopic.php?t=36697
// 
//
// #define TEMPERATURE_SENSOR
// #define XTAL_TURNOVER_TEMP  400  // deg C / 16
// #define XTAL_FREQUENCY_COEF 34   // -ppb / (deg C)^2


// IV-18 TO-SPEC HACK
//
// The Ice Tube Clock does not drive the IV-18 VFD tube to
// specifications, but with some rewiring and additional circuitry,
// enabling the following macros will drive the IV-18 tube as it was
// designed.  The following thread on the Adafruit Clocks forum
// describes the required hardware modifications:
//
//   http://forums.adafruit.com/viewtopic.php?f=41&t=41811
//
//
// #define VFD_TO_SPEC
// #define OCR0A_VALUE 128


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
#ifndef VFD_TO_SPEC
#define OCR0A_MIN   20
#define OCR0A_SCALE 7
#define OCR0A_MAX OCR0A_MIN + 10 * OCR0A_SCALE
#endif  //  !VFD_TO_SPEC


// BIRTHDAY ALARM
//
// If BDAY_ALARM_MONTH and BDAY_ALARM_DAY are defined, the alarm sound
// on that day will always be "For He's a Jolly Good Fellow,"
// regardless of the alarm sound set in the menus.
//
//
// #define BDAY_ALARM_MONTH 5
// #define BDAY_ALARM_DAY   1


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

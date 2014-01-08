#ifndef CONFIG_H
#define CONFIG_H

// XMAS-ICETUBE CLOCK DESIGN
//
// The xmas-icetube revision of the Ice Tube Clock requires a few
// firmware modifications.  The XMAS_DESIGN macro below enables these
// modifications, but breaks compatibility with the Adafruit Ice Tube
// Clock v1.1.
//
// When configuring this firmware for use with the xmas-icetube
// revision, the configuration macros for the following features
// should also be enabled: the automatic dimmer hack, software
// temperature compensated timekeeping, and the IV-18 to-spec hack.
//
//
// #define XMAS_DESIGN


// ADAFRUIT-STYLE BUTTONS
//
// By default, the xmas-icetube firmware uses a unique button scheme:
// The menu button means "enter menu" or "exit menu."  Meanwhile, the
// plus button means "next menu item," "next selected option," or
// "increment selected number."  The set button means "set".
//
// The following macro enables Adafruit-style buttons: The menu button
// means "enter menu," "next menu item," or "exit menu".  The plus
// button means "next selected option," "increment selected number,"
// or "exit menu."  The set button means "set" or "exit menu."
//
//
// #define ADAFRUIT_BUTTONS


// LOW BATTERY THRESHOLD
//
// After sleeping for ten minutes, system voltage should fall to whatever
// the battery is providing.  After that time, the microcontroller will
// check the system voltage.  If system voltage is below the following
// threshold, the clock will display a low battery warning ("bad batt")
// upon waking.
//
//
#define LOW_BATTERY_VOLTAGE 2600  // millivolts


// AUTOMATIC DIMMER HACK
//
// Defining the following macro enables support for Automatic dimming
// when a pull-up resister and CdS photoresistor are installed in R3
// and CT1, adjacent to the ATMEGA's PC5 pin.  This hack also allows
// for the clock to be disabled at night (when dark).
//
// I suggest using a 5.6k pull-up resistor with an Advanced Photonix
// Inc PDV-P8001 photoresistor.  Note that photoresistors purchased
// from Adafruit should behave like the PDV-P8001 and are acceptable
// substitutes.  For more details, visit the Adafruit Clocks forum:
//
//   http://forums.adafruit.com/viewtopic.php?f=41&t=12932
//   http://forums.adafruit.com/viewtopic.php?p=219736#p219736
//
#define AUTOMATIC_DIMMER


// GPS TIMEKEEPING
//
// Defining the following macro enables GPS detection on the ATMEGA's
// RX pin.  The connection speed is 9600 baud by default for
// compatibility with the Adafruit Ultimate GPS Module.  For more
// information, visit the following pages:
//
//   http://forums.adafruit.com/viewtopic.php?f=41&t=36873
//   http://www.ladyada.net/make/icetube/mods.html
//   http://forums.adafruit.com/viewtopic.php?f=41&t=32660
//
//
#define GPS_TIMEKEEPING


// USART BAUD RATE
//
// The USART baud rate is used for both the debugging features and GPS
// timekeeping and may be changed below.
//
//
#define USART_BAUDRATE 9600


// TEMPERATURE COMPENSATED CRYSTAL OSCILLATOR
//
// The following macro enables support for an external 32.768 kHz
// clock source such as a Maxim DS32kHz.  This hack is discussed in
// detail on the Adafruit Clocks forum:
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
// modification is not useful for displaying temperature.
//
// This modification is, however, useful for temperature compensated
// timekeeping, however.  The XTAL_TURNOVER_TEMP macro specifies the
// temperature at which the crystal oscillates at maximum frequency in
// units of deg C / 16.  The XTAL_FREQUENCY_COEF macro specifies the
// parabolic temperature dependence of the crystal in units -ppb /
// (deg C)^2.  For a turnover temperature of 25 deg C and a frequency
// coefficient of -0.034 ppm / (deg C)^2, XTAL_TURNOVER_TEMP should be
// defined as 400 (25 * 16), and XTAL_FREQUENCY_COEF should be defined
// as 34 (-0.034 * -1000).
//
// The technique for temperature compensation is described in the
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


// BIRTHDAY ALARM
//
// If BDAY_ALARM_MONTH and BDAY_ALARM_DAY are defined, the alarm sound
// on that day will always be "For He's a Jolly Good Fellow,"
// regardless of the alarm sound set in the menus.
//
//
// #define BDAY_ALARM_MONTH 5
// #define BDAY_ALARM_DAY   1


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
// in before that point.  Thus the hardware will ensure that the
// grid/segment voltage maximum is never exceeded.
//
// For a dim display, I suggest setting OCR0A_MIN to 30 and
// OCR0A_SCALE to 14.  If the fuse becomes warm during operation,
// reduce OCR0A_SCALE or install a higher power fuse.
//
//
#define OCR0A_MIN   20
#define OCR0A_SCALE  7
#define OCR0A_MAX OCR0A_MIN + 10 * OCR0A_SCALE


// DISPLAY MULTIPLEXING ALGORITHM
//
// The following multiplexing options define how the display should be
// multiplexed.
//
// Digit multiplexing displays each digit in rapid succession.
// Although this is the standard way to do multiplexing, there might
// be slight ghosting, especially of decimals at higher boost voltage.
//
// I recommend digit multiplexing for use with the Adafruit Ice Tube
// Clock v1.1 without the to-spec hack.
//
// Subdigit multiplexing is like digit multiplexing, but displays each
// digit twice--once showing only segments B, C, and H (those lit when
// displaying "1.") and once showing only the other segments.  This
// method eliminates ghosting and reduces the overall brightness.
//
// I recommend subdigit multiplexing for use with the to-spec hack and
// the xmas-icetube hardware revision.  I find the reduction in
// overall brightness to be a benefit here, as the minimum brightness
// attainable with PWM brightness control and simple digit
// multiplexing is a bit too bright for my taste.  And the maximum
// brightness with plain PWM and simple digit multiplexing is
// unnecessarally bright.
//
// Segment multiplexing displays each segment (on all digits where
// that segment is displayed) in rapid succession.  This method
// eliminates ghosting, and the maximum brightness is similar to that
// with digit multiplexing.  But the per-digit brightness adjustment
// is not available when using this method.
//
// I do not recommend segment multiplexing, but left the feature in
// the code in case anyone wants to play with it.  The problem with
// segment multiplexing is that resistance through the MAX6921
// limits current on each segment, so if one segment is displayed on
// many digits, that segment will be dimmer than if one segment is
// displayed on only a few digits.  Also, more current will flow
// through segments with the least resistance (at the right of the
// display), so those digits will appear slightly brighter.
//
#define DIGIT_MULTIPLEXING
// #define SUBDIGIT_MULTIPLEXING
// #define SEGMENT_MULTIPLEXING


// IV-18 TO-SPEC HACK
//
// The Ice Tube Clock does not drive the IV-18 VFD tube to
// specifications, but with some rewiring and additional circuitry,
// enabling the following macros will drive the IV-18 tube as it was
// designed.  The following thread on the Adafruit Clocks forum
// describes the required hardware modifications for this hack:
//
//   http://forums.adafruit.com/viewtopic.php?f=41&t=41811
//
//
// #define VFD_TO_SPEC


// TO-SPEC BRIGHTNESS ADJUSTMENT METHOD
//
// These options should only be used with the to-spec hack above.
//
// With the to-spec hack, brightness may be controlled with boost
// voltage, pulse width modulation (PWM), or both.  I recommend
// controlling brightness with PWM only.
//
// If neither OCR0A_VALUE nor OCR0B_PWM_DISABLE is defined, brightness
// will be controlled by both boost voltage and PWM.
//
// If only OCR0A_VALUE is defined, the boost voltage will be fixed,
// and brightness will be controlled by PWM (recommended).
//
// If only OCR0B_PWM_DISABLE is defined, brightness will be controlled
// by the boost voltage.
//
// Defining OCR0A_VALUE and OCR0B_PWM_DISABLE is possible, but doing
// so will lock the display to a constant brightness and break the
// menu-configurable brightness adjustment.
//
// If D1 is a power blocking diode (as in the xmas-icetube hardware
// revision), I suggest an OCR0A_VALUE of 192.  If D1 is a Schottky
// diode (as in the original Adafruit design), I suggest an
// OCR0A_VALUE value of 128.  Ideally, the exact value should be tuned
// for your particular clock:  OCR0A_VALUE should be large enough such
// that the boost circuit generates just over 50v with the display
// installed and at maximum brightness, but OCR0A_VALUE should be no
// larger than necessary, as that would waste electricity.
//
//
// #define OCR0A_VALUE 128
// #define OCR0B_PWM_DISABLE


// TO-SPEC FILAMENT DRIVER METHOD
//
// The options below should only be used with the to-spec hack above.
// To drive the filament to specifications, do not define any of the
// filament current or voltage macros.
//
// Some tubes may make a humming or whining sound when driven at with
// AC.  If that noise is unacceptable, it might be worth trying to
// divide the frequency with the FILAMENT_FREQUENCY_DIV macro.  For
// example, if FILAMENT_FREQUENCY_DIV is defined as 3, the AC
// frequency will be 12/3 = 4 kHz.  The AC frequency may be divided by
// any value below 256.
//
// Although not to-spec, a sure-fire way to eliminate hum is to simply
// drive the filament with DC.  With a high boost voltage, there
// should not be a noticable brightness gradient.
//
// Defining the FILAMENT_DRIVE_DC_FWD macro will drive the display
// with direct current instead of alternating current.  Current will
// flow from the right to left of the display.  Defining the
// FILAMENT_DRIVE_DC_REV macro will use direct current flowing from
// the left to right of the display.
//
// Defining either FILAMENT_VOLTAGE_3_3 or FILAMENT_VOLTAGE_2_5 will
// reduce the filament voltage to either 3.3 or 2.5 volts,
// respectively.  Voltage is reduced by reducing the duty cycle from
// 100% to 66% or 50%.  These macros also affect drive frequency; if
// the FILAMENT_FREQUENCY_DIV macro is undefined (or defined as 1),
// the frequency will be approximately
//
//              ALTERNATING CURRENT    DIRECT CURRENT
// 5.0 VOLTS          13.0 kHz              --
// 3.3 VOLTS           9.0 kHz             9.0 kHz
// 2.5 VOLTS           6.5 kHz            13.0 kHz
//
// And again, defining any of the voltage or current macros below
// deliberately runs the filament outside the IV-18 specifications.
// These macros are mainly provided for testing purposes.  But if the
// reddish glow of the filament is bothersome, reducing the filament
// voltage might be worthwhile even though doing so may cause cathode
// poisoning.  Another option to reduce the reddish glow is to have
// the case made from blue tinted acrylic.
//
//
// #define FILAMENT_FREQUENCY_DIV 3
//
// #define FILAMENT_CURRENT_DC_FWD
// #define FILAMENT_CURRENT_DC_REV
//
// #define FILAMENT_VOLTAGE_3_3
// #define FILAMENT_VOLTAGE_2_5


// DEBUGGING FEATURES
//
// The following macro enables debugging.  When enabled, debugging
// information may be sent over USART via the DUMPINT() and DUMPSTR()
// macros defined in usart.h.  The default speed is 9600 baud.
//
//
// #define DEBUG


#endif  // CONFIG_H

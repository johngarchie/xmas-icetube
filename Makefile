# This Makefile defines the following targets:
#
# all (default):   compiles program
# install:         installs flash and eeprom
# install-all:     installs fuse bits, flash, eeprom, and lock bit
# install-fuse:    sets fuse bits
# install-flash:   uploads flash memory
# install-eeprom:  uploads eeprom memory
# install-lock:    sets lock bits
# clean:	   removes build files

# project name
PROJECT ?= icetube

# object files
OBJECTS ?= icetube.o system.o time.o alarm.o piezo.o \
	   display.o buttons.o mode.o usart.o gps.o

# avr microcontroller processing unit
AVRMCU ?= atmega328p

# avr system clock speed
AVRCLOCK ?= 8000000 

# avr in-system programmer
AVRISP ?= usbtiny
#AVRISP ?= arduino
#AVRISP ?= dragon_isp

# avr programming utilities
AVRCPP     ?= avr-gcc
AVRSIZE    ?= avr-size
AVRDUDE    ?= avrdude
AVROBJCOPY ?= avr-objcopy

# options for avr programming utilities
AVRCPPFLAGS   ?= -I. -mmcu=$(AVRMCU) -std=gnu99 -Os -Wall -DF_CPU=$(AVRCLOCK)
#AVRCPPFLAGS   += -gstabs -Wa,-ahlmsd=$*.lst  # for assembler listings
AVRSIZEOPT    ?= -A
AVRDUDEOPT    ?= -B 4 -P usb -c $(AVRISP) -p $(AVRMCU)  # usbtiny & dragon_isp
#AVRDUDEOPT    ?= -b 19200 -P /dev/ttyACM0 -c $(AVRISP) -p $(AVRMCU) # arduino
AVROBJCOPYOPT ?=

# explicitly specify a bourne-compatable shell
SHELL ?= /bin/sh

# build project and print memory usage
all: $(addprefix $(PROJECT),.elf _flash.hex _eeprom.hex _fuse.hex _lock.hex)
	-@echo
	-@$(AVRSIZE) $(AVRSIZEOPT) $<

# make program binary by linking object files
$(PROJECT).elf: $(OBJECTS)
	$(AVRCPP) $(AVRCPPFLAGS) -o $@ $^

# make time.o using system time the default clock time
time.o: time.c optgen.pl
	./optgen.pl time | xargs $(AVRCPP) -c $(AVRCPPFLAGS) -o $@ $<
	./optgen.pl time | xargs $(AVRCPP) -MM $(AVRCPPFLAGS) $< > $*.d

# make object files and dependency lists from source code
%.o: %.c Makefile
	$(AVRCPP) -c $(AVRCPPFLAGS) -o $@ $<
	$(AVRCPP) -MM $(AVRCPPFLAGS) $< > $*.d

# extract fuse bits from compiled code
$(PROJECT)_fuse.hex: $(PROJECT).elf
	$(AVROBJCOPY) $(AVROBJCOPYOPT) -j.fuse -O ihex $< $@

# extract program instructions from compiled code
$(PROJECT)_flash.hex: $(PROJECT).elf
	$(AVROBJCOPY) $(AVROBJCOPYOPT) -R.eeprom -R.fuse -R.lock -O ihex $< $@

# extract eeprom data from compiled code
$(PROJECT)_eeprom.hex: $(PROJECT).elf
	$(AVROBJCOPY) $(AVROBJCOPYOPT) -j .eeprom -O ihex $< $@

# extract lock bits from compiled code
$(PROJECT)_lock.hex: $(PROJECT).elf
	$(AVROBJCOPY) $(AVROBJCOPYOPT) -j.lock -O ihex $< $@

# uploads flash memory and eeprom memory
install: $(addprefix $(PROJECT),_fuse.hex _flash.hex _eeprom.hex _lock.hex)
	$(AVRDUDE) $(AVRDUDEOPT) -U flash:w:$(PROJECT)_flash.hex:i \
				 -U eeprom:w:$(PROJECT)_eeprom.hex:i

# install everything--fuses, flash, eeprom, and lock
install-all: $(addprefix $(PROJECT),_fuse.hex _flash.hex _eeprom.hex _lock.hex)
	-@echo $(AVRDUDE) $(AVRDUDEOPT) \
		`./optgen.pl fuse < $(PROJECT)_fuse.hex` \
		-U flash:w:$(PROJECT)_flash.hex:i \
		-U eeprom:w:$(PROJECT)_eeprom.hex:i \
		`./optgen.pl lock < $(PROJECT)_lock.hex`
	-@$(AVRDUDE) $(AVRDUDEOPT) \
		`./optgen.pl fuse < $(PROJECT)_fuse.hex` \
		-U flash:w:$(PROJECT)_flash.hex:i \
		-U eeprom:w:$(PROJECT)_eeprom.hex:i \
		`./optgen.pl lock < $(PROJECT)_lock.hex`

# set fuse bits on avr chip
install-fuse: $(PROJECT)_fuse.hex optgen.pl
	-@echo $(AVRDUDE) $(AVRDUDEOPT) `./optgen.pl fuse < $<`
	-@     $(AVRDUDE) $(AVRDUDEOPT) `./optgen.pl fuse < $<`

# install executable code to avr chip
install-flash: $(PROJECT)_flash.hex
	$(AVRDUDE) $(AVRDUDEOPT) -U flash:w:$<:i

# install eeprom data to avr chip
install-eeprom: $(PROJECT)_eeprom.hex
	$(AVRDUDE) $(AVRDUDEOPT) -U eeprom:w:$<:i

# set lock bits on avr chip
install-lock: $(PROJECT)_lock.hex optgen.pl
	-@echo $(AVRDUDE) $(AVRDUDEOPT) `./optgen.pl lock < $<`
	-@     $(AVRDUDE) $(AVRDUDEOPT) `./optgen.pl lock < $<`

# delete build files
clean:
	-rm -f $(addprefix $(PROJECT),.elf _flash.hex _eeprom.hex \
	    				   _fuse.hex _lock.hex) \
	       $(OBJECTS) $(OBJECTS:.o=.d) $(OBJECTS:.o=.lst)

# include auto-generated source code dependencies
-include $(OBJECTS:.o=.d)

.PHONY: all install install-all \
        install-fuse install-flash install-eeprom install-lock

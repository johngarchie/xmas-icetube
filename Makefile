# This Makefile defines the following targets:
#
# all (default):   compiles program
# install:	   uploads flash and eeprom memory
# install-fuse:    burns fuse settings
# install-flash:   uploads flash memory
# install-eeprom:  uploads eeprom memory
# clean:	   removes build files

# project name
PROJECT ?= icetube

# object files
OBJECTS ?= icetube.o system.o time.o alarm.o display.o buttons.o mode.o usart.o

# avr microcontroller processing unit
#AVRMCU ?= atmega168
AVRMCU ?= atmega328p

# avr system clock speed
AVRCLOCK ?= 8000000 

# avr in-system programmer
AVRISP ?= usbtiny

# explicitly specify a bourne-compatable shell
SHELL=/bin/sh

# avr programming utilities
AVRCPP     ?= avr-gcc
AVRSIZE    ?= avr-size
AVRDUDE    ?= avrdude
AVROBJCOPY ?= avr-objcopy

# options for avr programming utilities
AVRCPPFLAGS   ?= -I. -mmcu=$(AVRMCU) -std=gnu99 -Os -Wall -DF_CPU=$(AVRCLOCK)
#AVRSIZEOPT    ?= -C --mcu=$(AVRMCU)
AVRSIZEOPT    ?= -A
AVRDUDEOPT    ?= -p $(AVRMCU) -c $(AVRISP)
AVROBJCOPYOPT ?=

# fuse options for avrdude as implied by $(AVRMCU)
ifeq ($(AVRMCU),atmega168)
    FUSEOPT ?= -u -U lfuse:w:0xE2:m -u -U hfuse:w:0xD6:m -u -U efuse:w:0xF9:m
endif

ifeq ($(AVRMCU),atmega328p)
    FUSEOPT ?= -u -U lfuse:w:0x62:m -u -U hfuse:w:0xD1:m -u -U efuse:w:0xFE:m
endif

ifndef FUSEOPT
    $(error must provide fuse options in $$(FUSEOPT) for avrdude)
endif

# build project and print memory usage
all: $(PROJECT).elf $(PROJECT)_flash.hex $(PROJECT)_eeprom.hex
	-@echo
	-@$(AVRSIZE) $(AVRSIZEOPT) $<

# time.o must always be remade since time always changes
time.o: time.c timedef.pl ALWAYS
	./timedef.pl | xargs $(AVRCPP) -c $(AVRCPPFLAGS) -o $@ $<
	./timedef.pl | xargs $(AVRCPP) -MM $(AVRCPPFLAGS) $< > $*.d

# make program binary by linking object files
$(PROJECT).elf: $(OBJECTS)
	$(AVRCPP) $(AVRCPPFLAGS) -o $@ $^

# make object files and dependency lists from source code
%.o: %.c
	$(AVRCPP) -c $(AVRCPPFLAGS) -o $@ $<
	$(AVRCPP) -MM $(AVRCPPFLAGS) $< > $*.d

# convert executable code to intel hex format
$(PROJECT)_flash.hex: $(PROJECT).elf
	$(AVROBJCOPY) $(AVROBJCOPYOPT) -R .eeprom -O ihex $< $@

# convert eeprom data to intel hex format
$(PROJECT)_eeprom.hex: $(PROJECT).elf
	$(AVROBJCOPY) $(AVROBJCOPYOPT) -j .eeprom -O ihex $< $@

# install everything but fuse settings
install: install-flash install-eeprom

# install executable code to avr chip
install-flash: $(PROJECT)_flash.hex
	$(AVRDUDE) $(AVRDUDEOPT) -U flash:w:$(PROJECT)_flash.hex:i

# install eeprom data to avr chip
install-eeprom: $(PROJECT)_eeprom.hex
	$(AVRDUDE) $(AVRDUDEOPT) -U eeprom:w:$(PROJECT)_eeprom.hex:i

# burn fuse settings to avr chip
install-fuse:
	$(AVRDUDE) $(AVRDUDEOPT) $(FUSEOPT)

# delete build files
clean:
	-rm -f $(addprefix $(PROJECT),.elf _flash.hex _eeprom.hex) \
	       $(OBJECTS) $(OBJECTS:.o=.d) timedef.h

# include auto-generated source code dependencies
-include $(OBJECTS:.o=.d)

# always remake files depending on this target
ALWAYS:

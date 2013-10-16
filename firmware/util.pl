#!/usr/bin/env perl

use warnings;
use strict;

use constant FLASH_AVAIL  => 32768;
use constant RAM_AVAIL    => 2048 ;
use constant EEPROM_AVAIL => 1024 ;

if(@ARGV && $ARGV[0] eq "fuse") {
    # read fuse settings
    my $line = <STDIN>;
    $line = <STDIN>;
    chomp $line;

    # parse fuse settings
    $line =~ m/^:........(..)(..)(..)..\s*$/;
    my $low_fuse      = hex $1;
    my $high_fuse     = hex $2;
    my $extended_fuse = hex $3;

    # strip reserved fuse bits
    $extended_fuse &= 0x07;

    # print avrdude fuse bit options
    printf "-u -U lfuse:w:0x%02X:m\n", $low_fuse;
    printf "-u -U hfuse:w:0x%02X:m\n", $high_fuse;
    printf "-u -U efuse:w:0x%02X:m\n", $extended_fuse;
} elsif(@ARGV && $ARGV[0] eq "lock") {
    # read lock byte
    my $line = <STDIN>;
    $line = <STDIN>;
    chomp $line;

    # parse fuse settings
    $line =~ m/^:........(..)..\s*$/;
    my $lock_byte = hex $1;

    # strip reserved lock bits
    $lock_byte &= 0x3F;

    # print avrdude lock bit options
    printf "-U lock:w:0x%02X:m\n", $lock_byte;
} elsif(@ARGV && $ARGV[0] eq "time") {
    my @timeFields = qw/SECOND MINUTE HOUR MDAY MONTH YEAR WDAY YDAY DST/;
    my @timeData = localtime(time);

    # start month numbering at one, not zero
    ++$timeData[4];

    # make years past centry start, not past 1900
    $timeData[5] %= 100;

    for(my $i = 0; defined($timeFields[$i]); ++$i) {
	print "-DTIME_DEFAULT_$timeFields[$i]=$timeData[$i]$/";
    }
} elsif(@ARGV && $ARGV[0] eq "memusage") {
    my %usage;

    while(<STDIN>) {
	if(m/^(\.[a-z]+)\s+(\d+)\s+\d+\s*$/) {
	    $usage{$1} = $2;
	}
    }
    my $flash_usage  = $usage{".text"} + $usage{".data"};
    my $ram_usage    = $usage{".data"} + $usage{".bss"};
    my $eeprom_usage = $usage{".eeprom"};

    printf "ATMEGA328P MEMORY USAGE SUMMARY$/$/";

    printf "Program memory (FLASH):  %3d%%    (%5d/%5d)$/",
	   (100 * $flash_usage / FLASH_AVAIL),
	   $flash_usage, FLASH_AVAIL;
    printf "Preallocated SRAM:       %3d%%    (%5d/%5d)$/",
	   (100 * $ram_usage / RAM_AVAIL),
	   $ram_usage, RAM_AVAIL;
    printf "Allocated EEPROM:        %3d%%    (%5d/%5d)$/",
	   (100 * $eeprom_usage / EEPROM_AVAIL),
	   $eeprom_usage, EEPROM_AVAIL;
} else {
    die "Usage:  $0 [time|fuse|lock|memusage]\n";
}

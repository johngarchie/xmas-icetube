#!/usr/bin/env perl

use warnings;
use strict;

use Time::Local;

use constant FLASH_AVAIL  => 32768;
use constant RAM_AVAIL    => 2048 ;
use constant EEPROM_AVAIL => 1024 ;


my @timeFields = qw/SECOND MINUTE HOUR MDAY MONTH YEAR WDAY YDAY DST/;
my %timeFields = map { $timeFields[$_] => $_ } 0..$#timeFields;

sub time_offset();
sub time_is_dst_usa();
sub time_is_dst_eu();


if(@ARGV && ($ARGV[0] eq "fuse" || $ARGV[0] eq "vfuse")) {
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
    if($ARGV[0] eq "fuse") {
	printf "-u -U lfuse:w:0x%02X:m\n", $low_fuse;
	printf "-u -U hfuse:w:0x%02X:m\n", $high_fuse;
	printf "-u -U efuse:w:0x%02X:m\n", $extended_fuse;
    } elsif($ARGV[0] eq "vfuse") {
	printf "-u -U lfuse:v:0x%02X:m\n", $low_fuse;
	printf "-u -U hfuse:v:0x%02X:m\n", $high_fuse;
	printf "-u -U efuse:v:0x%02X:m\n", $extended_fuse;
    }
} elsif(@ARGV && ($ARGV[0] eq "lock" || $ARGV[0] eq "vlock")) {
    # read lock byte
    my $line = <STDIN>;
    $line = <STDIN>;
    chomp $line;

    # parse lock data
    $line =~ m/^:........(..)..\s*$/;
    my $lock_byte = hex $1;

    # strip reserved lock bits
    $lock_byte &= 0x3F;

    # print avrdude lock bit options
    if($ARGV[0] eq "lock") {
	printf "-U lock:w:0x%02X:m\n", $lock_byte;
    } elsif($ARGV[0] eq "vlock") {
	printf "-U lock:v:0x%02X:m\n", $lock_byte;
    }
} elsif(@ARGV && $ARGV[0] eq "time") {
    my @timeData = localtime(time);

    # start month numbering at one, not zero
    ++$timeData[4];

    # make years past centry start, not past 1900
    $timeData[5] %= 100;

    for(my $i = 0; defined($timeFields[$i]); ++$i) {
	print "-DTIME_DEFAULT_$timeFields[$i]=$timeData[$i]$/";
    }

    my($offset_hours, $offset_minutes) = time_offset();
    print "-DTIME_DEFAULT_UTC_OFFSET_HOURS=$offset_hours$/";
    print "-DTIME_DEFAULT_UTC_OFFSET_MINUTES=$offset_minutes$/";
    if(time_is_dst_usa()) {
	print "-DTIME_DEFAULT_AUTODST=TIME_AUTODST_USA$/";
    } elsif(time_is_dst_eu()) {
	if($offset_hours == 0) {
	    print "-DTIME_DEFAULT_AUTODST=TIME_AUTODST_EU_GMT$/";
	} elsif($offset_hours == 1) {
	    print "-DTIME_DEFAULT_AUTODST=TIME_AUTODST_EU_CET$/";
	} elsif($offset_hours == 2) {
	    print "-DTIME_DEFAULT_AUTODST=TIME_AUTODST_EU_EET$/";
	} else {
	    print "-DTIME_DEFAULT_AUTODST=0$/";
	}
    } else {
	print "-DTIME_DEFAULT_AUTODST=0$/";
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

    printf "ATMEGA328P/ATMEGA328 MEMORY USAGE SUMMARY$/$/";

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


# returns the current hours and minutes relative to gmt
# (ignores daylight saving time)
sub time_offset() {
    # lookup current year
    my @local_time = localtime(time);
    my $year = $local_time[$timeFields{"YEAR"}];

    # lookup winter time
    my @winter_time = (0, 0, 0, 1, 0, $year);

    my $offset_seconds = timegm(@winter_time) - timelocal(@winter_time);
    my $offset_minutes = int($offset_seconds / 60);
    my $offset_hours   = int($offset_minutes / 60);

    $offset_seconds %= 60;
    $offset_minutes %= 60;

    return ($offset_hours, $offset_minutes);
}


# returns true if following USA daylight saving rules
sub time_is_dst_usa() {
    # lookup current year
    my @local_time = localtime(time);
    my $year = $local_time[$timeFields{"YEAR"}];

    # check the dst start
    my @dst_start_month = localtime(timelocal(0, 0, 12, 1, 2, $year)); 
    my $dst_start_day = ( $dst_start_month[$timeFields{"WDAY"}] == 0 
			  ? 8 : 15 - $dst_start_month[$timeFields{"WDAY"}] );

    my $before_dst_start = timelocal(59, 59, 1, $dst_start_day, 2, $year);
    my $after_dst_start  = timelocal(1,   0, 3, $dst_start_day, 2, $year);

    return 0 unless $after_dst_start - $before_dst_start == 2;

    # check the dst end
    my @dst_end_month = localtime(timelocal(0, 0, 12, 1, 10, $year)); 

    my $dst_end_day = ( $dst_end_month[$timeFields{"WDAY"}] == 0 
			? 1 : 8 - $dst_end_month[$timeFields{"WDAY"}] );

    my $before_dst_end = timelocal(59, 59, 1, $dst_end_day, 10, $year);
    my $after_dst_end  = timelocal(1,   0, 2, $dst_end_day, 10, $year);

    return 0 unless $after_dst_end - $before_dst_end == 3602;

    return 1;
}


# returns true if following EU daylight saving rules
sub time_is_dst_eu() {
    # lookup current year
    my @local_time = localtime(time);
    my $year = $local_time[$timeFields{"YEAR"}];

    # check the dst start
    my @dst_start_month = localtime(timelocal(0, 0, 12, 31, 2, $year)); 
    my $dst_start_day   = 31 - $dst_start_month[$timeFields{"WDAY"}];

    my $before_dst_start=timelocal(59, 59, 0, $dst_start_day, 2, $year);
    my $after_dst_start =timelocal( 1,  0, 4, $dst_start_day, 2, $year);

    $after_dst_start - $before_dst_start == 2 * 60**2 + 2 or return 0;

    # check the dst end
    my @dst_end_month = localtime(timelocal(0, 0, 12, 31, 9, $year)); 
    my $dst_end_day   = 31 - $dst_end_month[$timeFields{"WDAY"}];

    my $before_dst_end = timelocal(59, 59, 0, $dst_end_day, 9, $year);
    my $after_dst_end  = timelocal( 1,  0, 4, $dst_end_day, 9, $year);

    $after_dst_end - $before_dst_end == 4 * 60**2 + 2 or return 0;

    return 1;
}

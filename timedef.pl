#!/usr/bin/perl

use warnings;
use strict;

my @timeFields = qw/SECOND MINUTE HOUR MDAY MONTH YEAR WDAY YDAY DST/;
my @timeData = localtime(time);

# start month numbering at one, not zero
++$timeData[4];

# make years past centry start, not past 1900
$timeData[5] %= 100;

for(my $i = 0; defined($timeFields[$i]); ++$i) {
    print "-DTIME_DEFAULT_$timeFields[$i]=$timeData[$i]$/";
}

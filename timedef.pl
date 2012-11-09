#!/usr/bin/perl

use warnings;
use strict;

my @timeFields = qw/SECOND MINUTE HOUR MDAY MONTH YEAR WDAY YDAY DST/;
my @timeData = localtime(time);

# start month numbering at one, not zero
++$timeData[4];

# make years past centry start, not past 1900
$timeData[5] %= 100;


print "#ifndef TIMEDEFS_H$/";
print "#define TIMEDEFS_H$/";

for(my $i = 0; defined($timeFields[$i]); ++$i) {
    printf "#define TIME_DEFAULT_%-6s %d$/", $timeFields[$i], $timeData[$i];
}

print "#endif$/";

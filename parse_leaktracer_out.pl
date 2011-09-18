#!/usr/bin/perl

my $exe_name = shift (@ARGV);
my $log_name = shift (@ARGV);

if (!$exe_name || !$log_name) {
   print "Usage: parse_leaktracer.out <PROGRAM> <LEAKTRACER LOG>\n";
   exit (1);
}

print "Processing \"$log_name\" log for \"$exe_name\"\n";

print "Matching addresses to \"$exe_name\"\n";

my %stacks;
my %addresses;
my $lines = 0;

open (LOG, $log_name) || die("failed to read from \"$log_name\"");

while (<LOG>) {
   chomp;
   my $line = $_;
   if ($line =~ /^Size=(\d*), stack=([\w ]*), data=.*/) {
      $lines ++;

      my $id = $2;
      $stacks{$id}{COUNTER} ++;
      $stacks{$id}{SIZE} += $1;

      my @ptrs = split(/ /, $id);
      foreach $ptr (@ptrs) {
         $addresses{$ptr} = "unknown";
      }
   }
}
close (LOG);
printf "found $lines leak(s)\n";
if ($lines == 0) { exit 0; }

# resolving addresses
my @unique_addresses = keys (%addresses);
my $addr_list = "";
foreach $addr (@unique_addresses) { $addr_list .= " $addr"; }

if (!open(ADDRLIST, "addr2line -e $exe_name $addr_list |")) { die "Failed to resolve addresses"; }
my $addr_idx = 0;
while (<ADDRLIST>) {
   chomp;
   $addresses{$unique_addresses[$addr_idx]} = $_;
   $addr_idx++;
}
close (ADDRLIST);

# printing allocations
while (($stack, $info) = each(%stacks)) {
   print $info->{SIZE}." bytes lost in ".$info->{COUNTER}." blocks, from following call stack:\n";
   @stack = split(/ /, $stack);
   foreach $addr (@stack) { print "\t".$addresses{$addr}."\n"; }
}

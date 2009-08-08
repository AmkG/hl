#!/usr/bin/perl

## execute all the tests
## usage: ./dotest.pl ./test1 ./tests

use warnings;
use strict;

my $test_prog = $ARGV[0];
my $test_dir = $ARGV[1];
my @tests;
my @results;

opendir TEST_DIR, $test_dir or die "Couldn't open $test_dir";

while ($_ = readdir TEST_DIR) {
    if (-f "$test_dir/$_" && $_ =~ /.*\.test$/) {
        open IN, "$test_dir/$_";
        my @lines = <IN>;
        close IN;
        my @current; 
        foreach (@lines) {
            ## lines of type "; *** ..." divide a file in different tests
            if ($_ =~ /^\; \*{3,}/) {
                my @cp = @current;
                push @tests, \@cp;
                @current  = ();
            } elsif ($_ =~ /^\;(.*)/) {
                ## lines of type "; regex" are the expected result of the test
                push @results, $1;
            }
            else {
                push @current, $_;           
            }
        }
        push @tests, \@current if (@current);
    }
}

my $tmp_name = "tempfile";

for my $i (0..$#tests) {
    open OUT, "> $tmp_name";
    print OUT foreach (@{$tests[$i]});
    close OUT;
    my $out = `$test_prog $tmp_name 2>&1`;
    unlink $tmp_name;
    print "Test $i ...";
    if ($out =~ $results[$i]) {
        print "ok.\n";
    } else {
        print "\nError in test:";
        print foreach (@{$tests[$i]});
        print "\nexpected $results[$i] but got $out\n";
    }
}

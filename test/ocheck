#!/usr/bin/perl
use warnings;
use strict;

die "Usage: $0 exit-code program [args...]\n"
unless @ARGV;

my $exp = shift;
if($exp !~ /^[0-9]+$/){
	die "$0: $exp not numeric";
}

if(!-x $ARGV[0]){
	# we've been passed a source file
	my($cmd, @args) = @ARGV;

	my $ucc = $ENV{UCC};
	die "$0: no \$UCC" unless $ucc;

	my $tmp = "$ENV{UCC_TESTDIR}/$$.retcheck-exe";

	my @cmd = ($ucc, '-o', $tmp, $cmd, @args);
	print "$0: run: @cmd\n" if exists $ENV{UCC_VERBOSE};
	die "$0: couldn't compile\n" if system(@cmd);

	@ARGV = ($tmp);
}

my $r = system(@ARGV) >> 8;

if($exp != $r){
	die "$0: expected $exp, got $r, from @ARGV\n";
}

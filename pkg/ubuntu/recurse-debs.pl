#!/usr/bin/env perl

# Copyright (c) 2026 Zededa, Inc.
# SPDX-License-Identifier: Apache-2.0

use 5.12.0;
use strict;
use File::Copy;
use File::Basename;

my @pkgs = @ARGV;

# expand the requested packages with their transitive hull
my %seen = map { $_ => 1 } @pkgs;
my $is_added=1;
while ($is_added) {
	$is_added=0;
	open(my $dh, "-|", 'apt-cache', 'depends', @pkgs)
		or die "could not run apt-cache: $!";
	while (<$dh>) {
		if (m/^\s+(?:Pre)?Depends:\s+([^\s<]\S*)/ or m/^<?(\[^ >]+)/) {
			$is_added=1 unless $seen{$1};
			push @pkgs, $1 unless $seen{$1}++;
		}
	}
	close($dh) or die "could not finish apt-cache: $!";
}
print("@pkgs\n");


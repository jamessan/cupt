package Cupt::Core;

INIT { require Carp; $SIG{__WARN__} = \&Carp::confess; $SIG{__DIE__} = \&Carp::confess; }

use 5.10.0;
use warnings;
use strict;

use Exporter qw(import);
our @EXPORT = qw(
	&myprint &mywarn &myerr &myredie &mydie &myinternaldie &mycatch &mydebug
	$package_name_regex $version_string_regex);

use Locale::gettext;

#sub __ {
#	return gettext(shift);
#}

textdomain("cupt");

sub myprint {
	print sprintf(gettext(shift), @_);
}

sub mywarn {
	print "W: ";
	myprint @_;
	print "\n";
}

sub myerr {
	print "E: ";
	myprint @_;
	print "\n";
}

sub myredie() {
	die 'Cupt::Error';
}

sub mydie {
	myerr @_;
	myredie();
}

sub myinternaldie {
	print "E: ", gettext("internal error: ");
	myprint @_;
	print "\n";
	exit 255;
}

sub mycatch() {
	# This subroutine is inspired from David Golden's Exception::Class::TryCatch::&catch
    my $err = $@;
    if ($err =~ m/^Cupt::Error/) {
        return 1;
	} elsif ($err ne "") {
		# some other error received, propagate it
		die $err;
    } else {
        return 0;
    }
}

sub mydebug {
	print "D: ";
	say @_;
}

our $package_name_regex = qr/[a-z_0-9.+-]+/;

our $version_string_regex =
	qr/ (?<EPOCH> # name of this match is 'EPOCH'
			\d+: # epoch
		)? # which is non-mandatory
		(
			(?:
				(?(<EPOCH>):|0) # allow ':' if there was valid epoch, otherwise something neutral
					| # or
				[a-zA-Z+0-9~.-] # upstream version allowed characters
			)+? # '?' to not eat last '-' before debian revision
		)
		(
			-
			[a-zA-Z+0-9~.]+ # debian revision
		)? # which is non-mandatory
	/x;

sub compare_version_strings($$) {
	# version part can be epoch, version and debian revision
	my $compare_version_part = sub {
		my $compare_symbol = sub {
			my ($left, $right) = @_;
			my $left_ord = ord($left);
			my $right_ord = ord($right);
			$left_ord = 0 if $left eq '~';
			$right_ord = 0 if $right eq '~';
			return $left_ord <=> $right_ord;
		};

		my ($left, $right) = @_;

		# add "empty" characters to make strings char-comparable
		# 1 will be less than all but '~' character
		if (length($left) > length($right)) {
			$right .= chr(1) x (length($left) - length($right));
		} elsif (length($left) < length($right)) {
			$left .= chr(1) x (length($right) - length($left));
		}

		my $len = length($left);
		my $last_char_idx = $len - 1;
		foreach my $idx (0 .. $last_char_idx) {
			my $left_char = substr($left, $idx, 1);
			my $right_char = substr($right, $idx, 1);
			my $char_comparison_result = $compare_symbol->($left_char, $right_char);

			if ($char_comparison_result == 0) {
				# draw for now
				next;
			} else {
				# no possible draw here, one will be the winner
				if ($left_char =~ m/[0-9]/ && $right_char =~ m/[0-9]/) {
					# then we have to check lengthes of futher numeric parts
					# if some numeric part have greater length, then it will be
					# the winner, otherwise previous comparison result is used
					# examples: 'abc120de' < 'abc123', but 'abc1200' > 'abc123'
					my $left_num_pos = $idx;
					while ($left_num_pos+1 < $len and
							substr($left, $left_num_pos+1, 1) =~ m/[0-9]/)
					{
						++$left_num_pos;
					}

					my $right_num_pos = $idx;
					while ($right_num_pos+1 < $len and
							substr($right, $right_num_pos+1, 1) =~ m/[0-9]/)
					{
						++$right_num_pos;
					}

					my $num_pos_comparison_result = ($left_num_pos <=> $right_num_pos);
					return $num_pos_comparison_result unless $num_pos_comparison_result == 0;

					# the same number the digits
					return $char_comparison_result;
				} else {
					# some char is not a digit, so general rule applies
					return $char_comparison_result;
				}
			}
		}
		# oh, we are out of strings here... well, they are equal then
		return 0;
	};

	my ($left, $right) = @_;
	my ($left_epoch, $left_upstream, $left_revision) = ($left =~ /^$version_string_regex$/);
	my ($right_epoch, $right_upstream, $right_revision) = ($right =~ /^$version_string_regex$/);

	if (!defined($left_epoch)) {
		$left_epoch = '0';
	} else {
		chop($left_epoch);
	}
	if (!defined($right_epoch)) {
		$right_epoch = '0';
	} else {
		chop($right_epoch);
	}
	if (!defined($left_revision)) {
		$left_revision = '-0';
	}
	if (!defined($right_revision)) {
		$right_revision = '-0';
	}

	my $epoch_comparison_result = $left_epoch <=> $right_epoch;
	return $epoch_comparison_result unless $epoch_comparison_result == 0;

	my $upstream_comparison_result = $compare_version_part->($left_upstream, $right_upstream);
	return $upstream_comparison_result unless $upstream_comparison_result == 0;

	my $revision_comparison_result = $compare_version_part->($left_revision, $right_revision);
	return $revision_comparison_result;
}

1;


package t::lib::DummyTiedHash;

use strict;
use warnings;
use Tie::Hash;

our @ISA = qw(Tie::StdHash);

sub TIEHASH {
    my ($class, $init) = @_;

    return bless {%{$init || {}}}, $class;
}

1;

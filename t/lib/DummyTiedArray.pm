package t::lib::DummyTiedArray;

use strict;
use warnings;
use Tie::Array;

our @ISA = qw(Tie::StdArray);

sub TIEARRAY {
    my ($class, $init) = @_;

    return bless [@{$init // []}], $class;
}

1;

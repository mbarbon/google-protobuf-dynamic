package t::lib::DummyTiedScalar;

use strict;
use warnings;
#use Tie::Array;

#our @ISA = qw(Tie::StdArray);

sub TIESCALAR {
    my ($class, $init) = @_;

    return bless $init, $class;
}

sub FETCH {
    return ${$_[0]};
}

1;

package t::lib::DummyTiedArray;

use strict;
use warnings;
use Tie::Array;

our @ISA = qw(Tie::Array);

sub TIEARRAY {
    my ($class, $init) = @_;

    return bless { value => ($init || []), count => 0 }, $class;
}

sub FETCH {
    $_[0]->{count}++;

    return $_[0]->{value}[$_[1]];
}

sub FETCHSIZE {
    return scalar @{$_[0]->{value}};
}

sub fetch_count {
    return {
        count => $_[0]->{count},
        inner => t::lib::DummyTiedScalar::inner_fetch_count($_[0]->{value}),
    };
}

1;

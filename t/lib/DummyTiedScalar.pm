package t::lib::DummyTiedScalar;

use strict;
use warnings;

sub TIESCALAR {
    my ($class, $init) = @_;

    return bless { value => $init, count => 0 }, $class;
}

sub FETCH {
    ++$_[0]->{count};

    return ${$_[0]->{value}};
}

sub fetch_count {
    return reduce_fetch({
        count => $_[0]->{count},
        inner => t::lib::DummyTiedScalar::inner_fetch_count(${$_[0]->{value}}),
    });
}

sub reduce_fetch {
    return $_[0]->{inner} == -1 ? $_[0]->{count} : $_[0];
}

sub inner_fetch_count {
    return tied($_[0])->fetch_count if tied($_[0]);
    if (ref $_[0] eq 'HASH') {
        return tied(%{$_[0]})->fetch_count if tied(%{$_[0]});
        return {
            map +($_ => inner_fetch_count($_[0]->{$_})), keys %{$_[0]}
        };
    } elsif (ref $_[0] eq 'ARRAY') {
        return tied(@{$_[0]})->fetch_count if tied(@{$_[0]});
        return [map inner_fetch_count($_), @{$_[0]}]
    } else {
        return -1;
    }
}

1;

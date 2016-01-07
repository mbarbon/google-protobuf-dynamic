package t::lib::Test;

use strict;
use warnings;
use parent 'Test::Builder::Module';

use Test::More;
use Test::Differences;
use Test::Exception;

use Google::ProtocolBuffers::Dynamic;

our @EXPORT = (
    @Test::More::EXPORT,
    @Test::Differences::EXPORT,
    @Test::Exception::EXPORT,
    qw(
    )
);

sub import {
    unshift @INC, 't/lib';

    strict->import;
    warnings->import;

    goto &Test::Builder::Module::import;
}

1;

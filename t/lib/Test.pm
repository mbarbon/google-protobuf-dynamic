package t::lib::Test;

use strict;
use warnings;
use parent 'Test::Builder::Module';

use Test::More;
use Test::Differences;
use Test::Exception;
use Test::Warn;

use t::lib::DummyTiedScalar;
use t::lib::DummyTiedArray;
use t::lib::DummyTiedHash;

use Google::ProtocolBuffers::Dynamic;
use Config;

our @EXPORT = (
    @Test::More::EXPORT,
    @Test::Differences::EXPORT,
    @Test::Exception::EXPORT,
    @Test::Warn::EXPORT,
    qw(
          maybe_bigint
          tie_scalar
          tie_array
          tied_array
          tied_hash

          tied_fetch_count

          decode_eq_or_diff
          decode_throws_ok
          decoder_functions

          encode_eq_or_diff
          encode_throws_ok
          encoder_functions
    )
);

sub import {
    unshift @INC, 't/lib';

    strict->import;
    warnings->import;

    if (@_ > 1 && $_[1] eq 'proto3') {
        splice @_, 1, 1;

        @_ = ($_[0], "skip_all", "Protocol Buffers v3 required")
            unless Google::ProtocolBuffers::Dynamic::is_proto3();
    } elsif (@_ > 1 && $_[1] eq 'proto3_optional') {
        splice @_, 1, 1;

        @_ = ($_[0], "skip_all", "Protocol Buffers v3.12 required")
            unless Google::ProtocolBuffers::Dynamic::has_proto3_optional();
    }

    goto &Test::Builder::Module::import;
}

sub maybe_bigint {
    return $_[0] + 0 if $Config{ivsize} >= 8;

    require Math::BigInt;
    my $bi = Math::BigInt->new($_[0]);

    return $bi > -2147483648 && $bi < 2147483647 ? 0 + $_[0] : $bi;
}

sub tie_scalar {
    tie $_[0], 't::lib::DummyTiedScalar', \$_[1];
}

sub tie_array {
    tie @{$_[0]}, 't::lib::DummyTiedArray', $_[1];
}

sub tied_array {
    my @result;

    tie @result, 't::lib::DummyTiedArray', [@_];

    return \@result;
}

sub tied_hash {
    my %result;

    tie %result, 't::lib::DummyTiedHash', {@_};

    return \%result;
}

sub tied_fetch_count {
    if (tied $_[0]) {
        return tied($_[0])->fetch_count;
    } else {
        return t::lib::DummyTiedScalar::inner_fetch_count($_[0]);
    }
}

sub decoder_functions {
    return qw(decode_upb decode_bbpb);;
}

sub decode_eq_or_diff {
    my ($package, $bytes, $expected, $description) = @_;

    local $Test::Builder::Level = $Test::Builder::Level + 1;

    eq_or_diff($package->decode_upb($bytes), $expected, $description ? "$description (decode_upb)" : '(decode_upb)');
    eq_or_diff($package->decode_bbpb($bytes), $expected, $description ? "$description (decode_bbpb)" : '(decode_bbpb)');
}

sub decode_throws_ok {
    my ($package, $bytes, $expected_upb, $expected_bbpb, $description) = @_;

    local $Test::Builder::Level = $Test::Builder::Level + 1;

    throws_ok(sub { $package->decode_upb($bytes) }, $expected_upb, $description ? "$description (decode_upb)" : '(decode_upb)');
    throws_ok(sub { $package->decode_bbpb($bytes) }, $expected_bbpb, $description ? "$description (decode_bbpb)" : '(decode_bbpb)');
}

sub encoder_functions {
    return qw(encode);;
}

sub encode_eq_or_diff {
    # don't touch $_[1] to avoid a tie fetch
    my ($package, $expected, $description) = @_[0, 2, 3];

    local $Test::Builder::Level = $Test::Builder::Level + 1;

    eq_or_diff($package->encode($_[1]), $expected, $description ? "$description (encode_upb)" : '(encode_upb)');
}

sub encode_throws_ok {
    my ($package, $object, $expected_upb, $description) = @_;

    local $Test::Builder::Level = $Test::Builder::Level + 1;

    throws_ok(sub { $package->encode($object) }, $expected_upb, $description ? "$description (encode_upb)" : '(encode_upb)');
}


1;

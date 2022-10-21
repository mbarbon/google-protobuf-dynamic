use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("wkt/scalar.proto");
$d->map_message("test.Basic", "Test::Basic");
$d->map_wkts({ decode_blessed => 0 });
$d->resolve_references();

my %scalar_values = (
    timestamp_f => [{ seconds => 15, nanos => 17 }, "\x0a\x04\x08\x0f\x10\x11"],
    duration_f  => [{ seconds => 15, nanos => 17 }, "\x12\x04\x08\x0f\x10\x11"],
    double_f    => [{ value => 0.125 }, "\x1a\x09\x09\x00\x00\x00\x00\x00\x00\xc0?"],
    float_f     => [{ value => 0.125 }, "\x22\x05\x0d\x00\x00\x00\x3e"],
    int64_f     => [{ value => maybe_bigint('4294967296') }, "\x2a\x06\x08\x80\x80\x80\x80\x10"],
    uint64_f    => [{ value => maybe_bigint('1099511627776') }, "\x32\x07\x08\x80\x80\x80\x80\x80\x20"],
    int32_f     => [{ value => 2147483647 }, "\x3a\x06\x08\xff\xff\xff\xff\x07"],
    uint32_f    => [{ value => 4294967295 }, "\x42\x06\x08\xff\xff\xff\xff\x0f"],
    bool_f      => [{ value => 1 }, "\x4a\x02\x08\x01"],
    string_f    => [{ value => "abc" }, "\x52\x05\x0a\x03abc"],
    bytes_f     => [{ value => "abc" }, "\x5a\x05\x0a\x03abc"],
);

{
    for my $field (sort keys %scalar_values) {
        my ($value, $encoded) = @{$scalar_values{$field}};
        my $bytes = Test::Basic->encode({ $field => $value });

        eq_or_diff($bytes, $encoded,
                   "scalar $field - encoded value");
        decode_eq_or_diff('Test::Basic', $bytes, Test::Basic->new({ $field => $value }),
                          "scalar $field - round trip");
    }
}

done_testing();

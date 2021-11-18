use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("wkt/scalar.proto");
$d->map_message("test.Basic", "Test::Basic");
$d->map_wkts({ decode_blessed => 0 });
$d->resolve_references();

my %scalar_values = (
    timestamp_f => [{ seconds => 123, nanos => 777888999 }, '{"timestampF":"1970-01-01T00:02:03.777888999Z"}'],
    duration_f  => [{ seconds => 123, nanos => 777888999 }, '{"durationF":"123.777888999s"}'],
    double_f    => [{ value => 1.25 }, '{"doubleF":1.25}'],
    float_f     => [{ value => 2.125 }, '{"floatF":2.125}'],
    int64_f     => [{ value => maybe_bigint('4294967296') }, '{"int64F":"4294967296"}'],
    uint64_f    => [{ value => maybe_bigint('1099511627776') }, '{"uint64F":"1099511627776"}'],
    int32_f     => [{ value => 2147483647 }, '{"int32F":2147483647}'],
    uint32_f    => [{ value => 4294967295 }, '{"uint32F":4294967295}'],
    bool_f      => [{ value => 1 }, '{"boolF":true}'],
    string_f    => [{ value => "abc" }, '{"stringF":"abc"}'],
    bytes_f     => [{ value => "abc" }, '{"bytesF":"YWJj"}'],
);

{
    for my $field (sort keys %scalar_values) {
        my ($value, $encoded) = @{$scalar_values{$field}};
        my $bytes = Test::Basic->encode_json({ $field => $value });
        my $decoded = Test::Basic->decode_json($bytes);

        eq_or_diff($bytes, $encoded,
                   "scalar $field - encoded value");
        eq_or_diff($decoded, Test::Basic->new({ $field => $value }),
                   "scalar $field - round trip");
    }
}

done_testing();

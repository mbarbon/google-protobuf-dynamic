use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("scalar.proto");
$d->load_file("repeated.proto");
$d->load_file("map_proto2.proto");
$d->map_message("test.Basic", "Test::Basic");
$d->map_message("test.Repeated", "Test::Repeated");
$d->map_message("test.Maps", "Test::Maps", { implicit_maps => 1 });
$d->map_message("test.Item", "Test::Item");
$d->resolve_references();

my %scalar_values = (
    double_f    => [0.125, '{"doubleF":0.125}'],
    float_f     => [0.125, '{"floatF":0.125}'],
    int32_f     => [2147483647, '{"int32F":2147483647}'],
    int64_f     => [maybe_bigint('4294967296'), '{"int64F":4294967296}'],
    uint32_f    => [4294967295, '{"uint32F":4294967295}'],
    uint64_f    => [maybe_bigint('1099511627776'), '{"uint64F":1099511627776}'],
    bool_f      => [1, '{"boolF":true}'],
    string_f    => ["\x{101f}", "{\"stringF\":\"\xe1\x80\x9f\"}"],
    bytes_f     => ["\xe1\x80\x9f", '{"bytesF":"4YCf"}'],
    enum_f      => [2, '{"enumF":"SECOND"}'],
);

my %repeated_values = (
    double_f    => [[0.125, 0.5], '{"doubleF":[0.125,0.5]}'],
    float_f     => [[0.125, 0.5], '{"floatF":[0.125,0.5]}'],
    int32_f     => [[2147483647, 1], '{"int32F":[2147483647,1]}'],
    int64_f     => [[maybe_bigint('4294967296'), 1], '{"int64F":[4294967296,1]}'],
    uint32_f    => [[4294967295, 1], '{"uint32F":[4294967295,1]}'],
    uint64_f    => [[maybe_bigint('1099511627776'), 1], '{"uint64F":[1099511627776,1]}'],
    bool_f      => [[1, ''], '{"boolF":[true,false]}'],
    string_f    => [["\x{101f}", "\x{101e}"], "{\"stringF\":[\"\xe1\x80\x9f\",\"\xe1\x80\x9e\"]}"],
    bytes_f     => [["\xe1\x80\x9f", "\xe1\x80\x9e"], '{"bytesF":["4YCf","4YCe"]}'],
    enum_f      => [[2, 3], '{"enumF":["REPEATED_SECOND","REPEATED_THIRD"]}'],
);

my %map_values = (
    string_int32_map    => [
        {
            a => 7,
            b => 8,
        },
        'stringInt32Map',
        {
            a => '"a":7',
            b => '"b":8',
        }
    ],
    bool_int32_map      => [
        {
            "" => 3,
            1  => 4,
        },
        'boolInt32Map',
        {
            "" => '"false":3',
            1  => '"true":4',
        },
    ],
    int32_message_map   => [
        {
            1234567890 => Test::Item->new({ one_value => 14 }),
        },
        "int32MessageMap",
        {
            1234567890 => '"1234567890":{"oneValue":14}',
        },
    ],
    int64_int32_map     => [
        {
            4321234567890 => 1234567890,
        },
        "int64Int32Map",
        {
            4321234567890 => '"4321234567890":1234567890',
        },
    ],
    uint32_enum_map     => [
        {
            3234567890 => 2,
        },
        "uint32EnumMap",
        {
            3234567890 => '"3234567890":"MAP_SECOND"',
        },
    ],
    uint64_int32_map    => [
        {
            4321234567890 => -1234567890,
        },
        "uint64Int32Map",
        {
            4321234567890 => '"4321234567890":-1234567890',
        },
    ],
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

{
    for my $field (sort keys %repeated_values) {
        my ($value, $encoded) = @{$repeated_values{$field}};
        my $bytes = Test::Repeated->encode_json({ $field => $value });
        my $decoded = Test::Repeated->decode_json($bytes);

        eq_or_diff($bytes, $encoded,
                   "repeated $field - encoded value");
        eq_or_diff($decoded, Test::Repeated->new({ $field => $value }),
                   "repeated $field - round trip");
    }
}

{
    sub encode {
        my ($field, $map, $parts) = @_;

        return sprintf '{"%s":{%s}}',
                       $field,
                       join(',', map $parts->{$_}, keys %$map);
    }

    for my $field (sort keys %map_values) {
        my ($value, $encoded_field, $encoded) = @{$map_values{$field}};
        my $bytes = Test::Maps->encode_json({ $field => $value });
        my $decoded = Test::Maps->decode_json($bytes);

        eq_or_diff($bytes, encode($encoded_field, $value, $encoded),
                   "map $field - encoded value");
        eq_or_diff($decoded, Test::Maps->new({ $field => $value }),
                   "map $field - round trip");
    }
}

done_testing();

use t::lib::Test 'proto3';

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("map.proto");
$d->map_message("test.Maps", "Maps");
$d->map_message("test.Item", "Item");
$d->map_message("test.StringMap", "StringMap");
$d->resolve_references();

my %values = (
    string_int32_map => [
        {
            "a" => 1,
            "b" => 7,
            ""  => 0,
        },
        {
            a  => "\x0a\x05\x0a\x01a\x10\x01",
            b  => "\x0a\x05\x0a\x01b\x10\x07",
            "" => "\x0a\x04\x0a\x00\x10\x00"
        },
    ],
    int32_string_map => [
        {
            2 => "a",
            3 => "b",
            0 => "",
        },
        {
            2 => "\x12\x05\x08\x02\x12\x01a",
            3 => "\x12\x05\x08\x03\x12\x01b",
            0 => "\x12\x04\x08\x00\x12\x00",
        },
    ],
    int32_bool_map => [
        {
            2 => "",
            3 => 1,
            0 => "",
        },
        {
            2 => "\x1a\x04\x08\x02\x10\x00",
            3 => "\x1a\x04\x08\x03\x10\x01",
            0 => "\x1a\x04\x08\x00\x10\x00",
        },
    ],
    bool_int32_map => [
        {
            1 => 0,
            '' => 2,
        },
        {
            1  => "\x22\x04\x08\x01\x10\x00",
            '' => "\x22\x04\x08\x00\x10\x02",
        },
    ],
    int32_enum_map => [
        {
            2 => 1,
            3 => 2,
            0 => 0,
        },
        {
            2 => "\x2a\x04\x08\x02\x10\x01",
            3 => "\x2a\x04\x08\x03\x10\x02",
            0 => "\x2a\x04\x08\x00\x10\x00",
        },
    ],
    int32_message_map => [
        {
            2 => Item->new({ one_value => 7 }),
            3 => Item->new({ another_value => "X" }),
            0 => Item->new,
        },
        {
            2 => "\x32\x06\x08\x02\x12\x02\x08\x07",
            3 => "\x32\x07\x08\x03\x12\x03\x12\x01X",
            0 => "\x32\x04\x08\x00\x12\x00",
        },
    ],
    string_string_map_map => [
        {
            'x' => StringMap->new({ string_int32_map => { 'b' => 2 } }),
        },
        {
            x => "\x3a\x0c\x0a\x01x\x12\x07\x0a\x05\x0a\x01b\x10\x02",
        },
    ],
    'uint32_bool_map' => [
        {
            1 => '',
            (1 << 31) => 1,
        },
        {
            1 => "\x42\x04\x08\x01\x10\x00",
            (1 << 31) => "\x42\x08\x08\x80\x80\x80\x80\x08\x10\x01",
        },
    ],
    'uint64_bool_map' => [
        {
            1 => '',
            (1 << 63) => 1,
        },
        {
            1 => "\x4a\x04\x08\x01\x10\x00",
            (1 << 63) => "\x4a\x0d\x08\x80\x80\x80\x80\x80\x80\x80\x80\x80\x01\x10\x01",
        },
    ],
);

sub encode {
    my ($map, $parts) = @_;

    return join('', map $parts->{$_}, keys %$map);
}

for my $field (sort keys %values) {
    my ($values, $encoded) = @{$values{$field}};

    encode_eq_or_diff('Maps', { $field => $values }, encode($values, $encoded),
                      "$field - encoded value");
    decode_eq_or_diff('Maps', encode($values, $encoded), Maps->new({ $field => $values }),
                      "$field - round trip");
}

done_testing();

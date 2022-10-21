use t::lib::Test;

my $d1 = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d1->load_file("map_proto2.proto");
$d1->map_message("test.Maps", "Maps", { implicit_maps => 1 });
$d1->map_message("test.Item", "Item");
$d1->resolve_references();

my $d2 = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d2->load_file("map_proto2.proto");
$d2->map_message("test.Maps", "NoMaps", { implicit_maps => 0, encode_defaults => 1 });
$d2->map_message("test.Item", "NoItem");
$d2->resolve_references();

my %values = (
    string_int32_map => [
        {
            "a" => 1,
            "b" => 7,
        },
        {
            a => "\x0a\x05\x0a\x01a\x10\x01",
            b => "\x0a\x05\x0a\x01b\x10\x07",
        },
    ],
    bool_int32_map => [
        {
            ""  => 1,
            "1" => 7,
        },
        {
            "" => "\x12\x04\x08\x00\x10\x01",
            1  => "\x12\x04\x08\x01\x10\x07",
        },
    ],
    int32_message_map => [
        {
            4 => Item->new({ one_value => 77 }),
            5 => Item->new({ another_value => "abc" }),
        },
        {
            4 => "\x1a\x06\x08\x04\x12\x02\x08\x4d",
            5 => "\x1a\x09\x08\x05\x12\x05\x12\x03abc",
        },
    ],
    int64_int32_map => [
        {
            4294967296 => 9,
        },
        {
            4294967296 => "\x22\x08\x08\x80\x80\x80\x80\x10\x10\x09",
        },
    ],
    uint32_enum_map => [
        {
            4294967295 => 2,
        },
        {
            4294967295 => "\x2a\x08\x08\xff\xff\xff\xff\x0f\x10\x02",
        },
    ],
    uint64_int32_map => [
        {
            1099511627776 => 7,
        },
        {
            1099511627776 => "\x32\x09\x08\x80\x80\x80\x80\x80\x20\x10\x07",
        },
    ],
);

sub encode {
    my ($map, $parts) = @_;

    return join('', map $parts->{$_}, keys %$map);
}

sub to_array {
    my ($map) = @_;

    return [map +{ key => $_, value => $map->{$_} }, keys %$map];
}

for my $field (sort keys %values) {
    my ($values, $encoded) = @{$values{$field}};
    my $bytes = Maps->encode({ $field => $values });
    my $bytes_nomap = NoMaps->encode({ $field => to_array($values) });

    eq_or_diff($bytes, encode($values, $encoded),
               "$field - encoded value");
    eq_or_diff($bytes, $bytes_nomap,
               "$field - encoded value (pair array)");
    decode_eq_or_diff('Maps', $bytes, Maps->new({ $field => $values }),
                      "$field - round trip");
}

my $bytes = "\xb2";
my $string = "\xb2";
my $encoded = "\x0a\x06\x0a\x02\xc2\xb2\x10\x01";

utf8::downgrade($bytes);
utf8::upgrade($string);

eq_or_diff(Maps->encode({ string_int32_map => { $bytes => 1 } }), $encoded,
       "bytes -> UTF-8 upgrade");
eq_or_diff(NoMaps->encode({ string_int32_map => [{ key => $bytes, value => 1 }] }), $encoded,
       "bytes -> UTF-8 upgrade (pair array)");
eq_or_diff(Maps->encode({ string_int32_map => { $string => 1 } }), $encoded,
       "UTF-8 string");
eq_or_diff(NoMaps->encode({ string_int32_map => [{ key => $string, value => 1 }] }), $encoded,
       "UTF-8 string (pair array)");

for my $method (decoder_functions) {
    my $method_desc = "($method)";

    # warnings are tested separately
    local $SIG{__WARN__} = sub { };

    my $broken1 = Maps->$method(NoMaps->encode({
        string_int32_map => [
            { key => 'a' },
            { key => 'b', value => 1 },
            { key => 'c' },
        ],
    }));

    eq_or_diff([sort keys %{$broken1->{string_int32_map}}], [qw(a b c)], $method_desc);
    eq_or_diff($broken1->{string_int32_map}{a}, 0, $method_desc);
    eq_or_diff($broken1->{string_int32_map}{b}, 1, $method_desc);
    eq_or_diff($broken1->{string_int32_map}{c}, 0, $method_desc);

    my $broken2 = Maps->$method(NoMaps->encode({
        string_int32_map => [
            {             value => 2 },
            { key => 'b', value => 1 },
            {             value => 4 },
        ],
    }));

    eq_or_diff([sort keys %{$broken2->{string_int32_map}}], [qw(b)], $method_desc);
    eq_or_diff($broken2->{string_int32_map}{b}, 1, $method_desc);
}

done_testing();

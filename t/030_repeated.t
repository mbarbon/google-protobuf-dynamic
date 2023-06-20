use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("repeated.proto");
$d->map_message("test.Repeated", "Repeated");
$d->map_message("test.Packed", "Packed");
$d->resolve_references();

my %values = (
    double_f    => [[0.125, 0.5, 0.0], "\x09\x00\x00\x00\x00\x00\x00\xc0?\x09\x00\x00\x00\x00\x00\x00\xe0?\x09\x00\x00\x00\x00\x00\x00\x00\x00"],
    float_f     => [[0.125, 0.5, 0.0], "\x15\x00\x00\x00\x3e\x15\x00\x00\x00\x3f\x15\x00\x00\x00\x00"],
    int32_f     => [[2147483647, 1, 0], "\x18\xff\xff\xff\xff\x07\x18\x01\x18\x00"],
    int64_f     => [[maybe_bigint('4294967296'), 1, 0], "\x20\x80\x80\x80\x80\x10\x20\x01\x20\x00"],
    uint32_f    => [[4294967295, 1, 0], "\x28\xff\xff\xff\xff\x0f\x28\x01\x28\x00"],
    uint64_f    => [[maybe_bigint('1099511627776'), 1, 0], "\x30\x80\x80\x80\x80\x80\x20\x30\x01\x30\x00"],
    bool_f      => [[1, ''], "\x38\x01\x38\x00"],
    string_f    => [["\x{101f}", "\x{101e}", ""], "\x42\x03\xe1\x80\x9f\x42\x03\xe1\x80\x9e\x42\x00"],
    bytes_f     => [["\xe1\x80\x9f", "\xe1\x80\x9e", ""], "\x4a\x03\xe1\x80\x9f\x4a\x03\xe1\x80\x9e\x4a\x00"],
    enum_f      => [[2, 3], "\x50\x02\x50\x03"],
);

my %packed_values = (
    double_f    => [[0.125, 0.5], "\x0a\x10\x00\x00\x00\x00\x00\x00\xc0?\x00\x00\x00\x00\x00\x00\xe0?"],
    float_f     => [[0.125, 0.5], "\x12\x08\x00\x00\x00\x3e\x00\x00\x00\x3f"],
    int32_f     => [[2147483647, 1], "\x1a\x06\xff\xff\xff\xff\x07\x01"],
    int64_f     => [[maybe_bigint('4294967296'), 1], "\x22\x06\x80\x80\x80\x80\x10\x01"],
    uint32_f    => [[4294967295, 1], "\x2a\x06\xff\xff\xff\xff\x0f\x01"],
    uint64_f    => [[maybe_bigint('1099511627776'), 1], "\x32\x07\x80\x80\x80\x80\x80\x20\x01"],
    bool_f      => [[1, ''], "\x3a\x02\x01\x00"],
    enum_f      => [[2, 3], "\x52\x02\x02\x03"],
);

for my $field (sort keys %values) {
    my ($values, $encoded) = @{$values{$field}};

    my $array = [(undef) x @$values];
    my $tied = { $field => [] };
    tie_scalar($array->[$_], $values->[$_]) for 0 .. $#$array;
    tie_array($tied->{$field}, $array);

    encode_eq_or_diff('Repeated', { $field => $values }, $encoded,
                      "$field - encoded value");
    encode_eq_or_diff('Repeated', $tied, $encoded,
                      "$field - encoded value with tied elements");
    decode_eq_or_diff('Repeated', $encoded, Repeated->new({ $field => $values }),
                      "$field - round trip");
    eq_or_diff(tied_fetch_count($tied), { $field => {
        count => scalar @$values,
        inner => [(1) x scalar @$values],
    } }, "$field - tied fetch count");
}

for my $field (sort keys %packed_values) {
    my ($values, $encoded) = @{$packed_values{$field}};

    encode_eq_or_diff('Packed', { $field => $values }, $encoded,
                      "$field - packed value");
    decode_eq_or_diff('Packed', $encoded, Packed->new({ $field => $values }),
                      "$field - round trip");
}

# unusual, but the spec excplicitly mentions them
decode_eq_or_diff('Repeated', "\x18\x01\x38\x01\x18\x02\x38\x00", Repeated->new({
    int32_f => [1, 2],
    bool_f  => [1, ''],
}), "non-contiguous repeated fields");
decode_eq_or_diff('Packed', "\x1a\x02\x01\x02\x1a\x02\x03\x04", Packed->new({
    int32_f => [1, 2, 3, 4],
}), "packed repeated field in multiple chunks");

# "A packed repeated field containing zero elements does not appear in the
# encoded message."
encode_eq_or_diff(
    'Packed', { int32_f => [] },
    '', # expect empty
    "empty packed repeated field");

done_testing();

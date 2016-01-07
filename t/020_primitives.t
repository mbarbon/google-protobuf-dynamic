use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("scalar.proto");
$d->map_message("test.Basic", "Basic");
$d->map_message("test.Default", "Default");
$d->resolve_references();

my %values = (
    double_f    => [0.125, "\x09\x00\x00\x00\x00\x00\x00\xc0?"],
    float_f     => [0.125, "\x15\x00\x00\x00\x3e"],
    int32_f     => [2147483647, "\x18\xff\xff\xff\xff\x07"],
    int64_f     => [4294967296, "\x20\x80\x80\x80\x80\x10"],
    uint32_f    => [4294967295, "\x28\xff\xff\xff\xff\x0f"],
    uint64_f    => [1099511627776, "\x30\x80\x80\x80\x80\x80\x20"],
    bool_f      => [1, "\x38\x01"],
    string_f    => ["\x{101f}", "\x42\x03\xe1\x80\x9f"],
    bytes_f     => ["\xe1\x80\x9f", "\x4a\x03\xe1\x80\x9f"],
    enum_f      => [2, "\x50\x02"],
);

my %default_defaults = (
    double_f    => 0,
    float_f     => 0,
    int32_f     => 0,
    int64_f     => 0,
    uint32_f    => 0,
    uint64_f    => 0,
    bool_f      => '',
    string_f    => '',
    bytes_f     => '',
    enum_f      => 1,
);

my %test_defaults = (
    double_f    => 1.0,
    float_f     => 2.0,
    int32_f     => 3,
    int64_f     => 4,
    uint32_f    => 5,
    uint64_f    => 6,
    bool_f      => 1,
    string_f    => "a string",
    bytes_f     => "some bytes",
    enum_f      => 3,
);

for my $field (sort keys %values) {
    my ($value, $encoded) = @{$values{$field}};
    my $bytes = Basic->encode_from_perl({ $field => $value });
    my $decoded = Basic->decode_to_perl($bytes);

    eq_or_diff($bytes, $encoded,
               "$field - encoded value");
    eq_or_diff($decoded, { %default_defaults, $field => $value },
               "$field - round trip");
}

eq_or_diff(Basic->decode_to_perl(''), \%default_defaults, "implicit defaults");
eq_or_diff(Default->decode_to_perl(''), \%test_defaults, "explicit defaults");

done_testing();

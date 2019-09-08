use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("scalar.proto");
$d->map_message("test.Basic", "Basic", { explicit_defaults => 1 });
$d->map_message("test.Default", "Default", { explicit_defaults => 1 });
$d->resolve_references();

my %values = (
    double_f    => [0.125, "\x09\x00\x00\x00\x00\x00\x00\xc0?"],
    float_f     => [0.125, "\x15\x00\x00\x00\x3e"],
    int32_f     => [2147483647, "\x18\xff\xff\xff\xff\x07"],
    int64_f     => [maybe_bigint('4294967296'), "\x20\x80\x80\x80\x80\x10"],
    uint32_f    => [4294967295, "\x28\xff\xff\xff\xff\x0f"],
    uint64_f    => [maybe_bigint('1099511627776'), "\x30\x80\x80\x80\x80\x80\x20"],
    bool_f      => [1, "\x38\x01"],
    string_f    => ["\x{101f}", "\x42\x03\xe1\x80\x9f"],
    bytes_f     => ["\xe1\x80\x9f", "\x4a\x03\xe1\x80\x9f"],
    enum_f      => [2, "\x50\x02"],
    sint32_f    => [-6, "\x58\x0b"],
    sint64_f    => [-5, "\x60\x09"],
    fixed32_f   => [3, "\x6d\x03\x00\x00\x00"],
    sfixed32_f  => [-3, "\x75\xfd\xff\xff\xff"],
    fixed64_f   => [4, "\x79\x04\x00\x00\x00\x00\x00\x00\x00"],
    sfixed64_f  => [-4, "\x81\x01\xfc\xff\xff\xff\xff\xff\xff\xff"],
);

my %default_defaults = (
    double_f    => '0',
    float_f     => '0',
    int32_f     => 0,
    int64_f     => 0,
    uint32_f    => 0,
    uint64_f    => 0,
    bool_f      => '',
    string_f    => '',
    bytes_f     => '',
    enum_f      => 1,
    fixed32_f   => 0,
    fixed64_f   => 0,
    sfixed32_f  => 0,
    sfixed64_f  => 0,
    sint32_f    => 0,
    sint64_f    => 0,
);
bless \%default_defaults, 'Basic';

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
    fixed32_f   => 9,
    fixed64_f   => 11,
    sfixed32_f  => -10,
    sfixed64_f  => -12,
    sint32_f    => -7,
    sint64_f    => -8,
);
bless \%test_defaults, 'Default';

for my $field (sort keys %values) {
    my ($value, $encoded) = @{$values{$field}};
    my $bytes = Basic->encode({ $field => $value });
    my $decoded = Basic->decode($bytes);

    my $tied = { $field => undef };
    tie_scalar($tied->{$field}, $value);
    my $tied_bytes = Basic->encode($tied);

    eq_or_diff($bytes, $encoded,
               "$field - encoded value");
    eq_or_diff($tied_bytes, $encoded,
               "$field - encoded tied value");
    eq_or_diff($decoded, Basic->new({ %default_defaults, $field => $value }),
               "$field - round trip");
    eq_or_diff(tied_fetch_count($tied), { $field => 1 }, "$field - tied fetch count");
}

eq_or_diff(Basic->encode({bool_f => ''}), "", "bool false");
eq_or_diff(Default->encode({bool_f => ''}), "\x38\x00", "bool false");
eq_or_diff(Basic->decode(''), \%default_defaults, "implicit defaults");
eq_or_diff(Default->decode(''), \%test_defaults, "explicit defaults");

done_testing();

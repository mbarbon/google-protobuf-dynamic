use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("scalar.proto");
$d->map({ package => 'test', prefix => 'Test' });

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

my %get_values = (
    double_f    => 11.0,
    float_f     => 12.0,
    int32_f     => 23,
    int64_f     => 34,
    uint32_f    => 45,
    uint64_f    => 56,
    bool_f      => 1,
    string_f    => "a get string",
    bytes_f     => "some get bytes",
    enum_f      => 2,
);

my %set_values = (
    double_f    => 11.0,
    float_f     => 12.0,
    int32_f     => 13,
    int64_f     => 14,
    uint32_f    => 15,
    uint64_f    => 16,
    bool_f      => 1,
    string_f    => "a set string",
    bytes_f     => "some set bytes",
    enum_f      => 2,
);

my $default_defaults = Test::Default->decode('');
my $basic_defaults = Test::Basic->decode('');

for my $field (sort keys %default_defaults) {
    my $with_field = bless { $field => $get_values{$field} }, 'Test::Basic';
    my $cleared = bless { $field => $get_values{$field} }, 'Test::Basic';
    my $empty = bless {}, 'Test::Basic';
    my $has = "has_$field";
    my $get = "get_$field";
    my $set = "set_$field";
    my $clear = "clear_$field";

    $empty->$set($set_values{$field});
    $cleared->$clear;

    ok(!$default_defaults->$has, "field '$field' is not present");
    ok($with_field->$has, "field '$field' is present");
    is($with_field->$get, $get_values{$field}, "getter for '$field' works");
    is($empty->$get, $set_values{$field}, "setter for '$field' works");
    is($default_defaults->$get, $test_defaults{$field}, "default default for '$field'");
    is($basic_defaults->$get, $default_defaults{$field}, "custom default for '$field'");
    ok(!$cleared->$has, "cleared '$field'");
}

throws_ok(
    sub { Test::Basic->new->set_enum_f(77) },
    qr/Invalid value 77 for enumeration field 'test.Basic.enum_f'/,
    'invalid enum value'
);

done_testing();

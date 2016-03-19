use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("repeated.proto");
$d->map({ package => 'test', prefix => 'Test' });

my %initial_values = (
    double_f    => 11.0,
    float_f     => 12.0,
    int32_f     => 23,
    int64_f     => 34,
    uint32_f    => 45,
    uint64_f    => 56,
    bool_f      => 1,
    string_f    => "a string",
    bytes_f     => "some bytes",
    enum_f      => 2,
);

my %added_values = (
    double_f    => 13.0,
    float_f     => 14.0,
    int32_f     => 25,
    int64_f     => 36,
    uint32_f    => 47,
    uint64_f    => 58,
    bool_f      => '',
    string_f    => "another string",
    bytes_f     => "some other bytes",
    enum_f      => 3,
);

for my $field (keys %initial_values) {
    my $with_field = bless { $field => [$initial_values{$field}] }, 'Test::Repeated';
    my $cleared = bless { $field => [$initial_values{$field}] }, 'Test::Repeated';
    my $empty = bless {}, 'Test::Repeated';
    my $size = "$field\_size";
    my $get = "get_$field";
    my $set = "set_$field";
    my $add = "add_$field";
    my $clear = "clear_$field";
    my $get_list = "get_$field\_list";
    my $set_list = "set_$field\_list";

    is($with_field->$size, 1, 'non-empty list size');
    is($empty->$size, 0, 'empty list size');
    is($empty->$get_list, undef, 'empty list is empty');

    $with_field->$add($added_values{$field});
    $empty->$add($added_values{$field});

    is($with_field->$size, 2, 'non-empty list after add');
    is($empty->$size, 1, 'empty list after add');

    eq_or_diff($with_field->$get_list, [$initial_values{$field}, $added_values{$field}], 'value added to non-empty list');
    eq_or_diff($empty->$get_list, [$added_values{$field}], 'value added to empty list');
    is($with_field->$get(1), $added_values{$field}, 'get item');
    is($with_field->$get(-2), $initial_values{$field}, 'get negative item');
    $with_field->$set(1, $initial_values{$field});
    is($with_field->$get(1), $initial_values{$field}, 'set item');

    $empty->$set_list([$initial_values{$field}, $added_values{$field}]);
    eq_or_diff($empty->$get_list, [$initial_values{$field}, $added_values{$field}], 'value set to empty list');

    $cleared->$clear;
    is($cleared->$size, 0, 'list cleared');
    is($cleared->$get_list, undef, 'cleared list is empty');
}

throws_ok(
    sub { Test::Repeated->new->add_enum_f(77) },
    qr/Invalid value 77 for enumeration field 'test.Repeated.enum_f'/,
    'invalid enum value'
);

throws_ok(
    sub { Test::Repeated->new->set_double_f_list(12) },
    qr/Value for field 'test.Repeated.double_f' is not an array reference/,
    'not an array',
);

throws_ok(
    sub { Test::Repeated->new({ double_f => 1 })->add_double_f(12) },
    qr/Value of field 'test.Repeated.double_f' is not an array reference/,
    'not an array',
);

throws_ok(
    sub { Test::Repeated->new->get_double_f(0) },
    qr/Accessing unset array field 'test.Repeated.double_f'/,
    'access item of empty array',
);

throws_ok(
    sub { Test::Repeated->new({ double_f => [] })->get_double_f(0) },
    qr/Accessing empty array field 'test.Repeated.double_f'/,
    'access item of empty array',
);

throws_ok(
    sub { Test::Repeated->new({ double_f => [2, 3, 4] })->get_double_f(3) },
    qr/Accessing out-of-bounds index 3 for field 'test.Repeated.double_f'/,
    'access out of bounds, too large',
);

throws_ok(
    sub { Test::Repeated->new({ double_f => [2, 3, 4] })->get_double_f(-4) },
    qr/Accessing out-of-bounds index -4 for field 'test.Repeated.double_f'/,
    'access aout of bounds, too small',
);

done_testing();

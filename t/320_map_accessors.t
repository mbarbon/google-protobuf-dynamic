use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("map_proto2.proto");
$d->map({ package => 'test', prefix => 'Test', options => { implicit_maps => 1 } });

my %initial_values = (
    string_int32_map    => { a => 7 },
    bool_int32_map      => { "" => 8 },
    int32_message_map   => { 7 => { one_value => 7 } },
    int64_int32_map     => { 9 => 17 },
    uint32_enum_map     => { 44 => 2 },
    uint64_int32_map    => { 12 => 8 },
);

my %added_values = (
    string_int32_map    => { b => 8 },
    bool_int32_map      => { 1 => 9 },
    int32_message_map   => { 8 => { one_value => 8 } },
    int64_int32_map     => { 10 => 18 },
    uint32_enum_map     => { 45 => 3 },
    uint64_int32_map    => { 13 => 8 },
);

for my $field (keys %initial_values) {
    my $with_field = bless { $field => { %{$initial_values{$field}} } }, 'Test::Maps';
    my $cleared = bless { $field => { %{$initial_values{$field}} } }, 'Test::Maps';
    my $empty = bless {}, 'Test::Maps';
    my $get = "get_$field";
    my $set = "set_$field";
    my $clear = "clear_$field";
    my $get_map = "get_$field\_map";
    my $set_map = "set_$field\_map";
    my ($initial_k, $initial_v) = %{$initial_values{$field}};
    my ($add_k, $add_v) = %{$added_values{$field}};

    is($empty->$get_map, undef, 'empty list is empty');

    $with_field->$set(%{$added_values{$field}});
    $empty->$set(%{$added_values{$field}});

    eq_or_diff($with_field->$get_map, { %{$initial_values{$field}}, %{$added_values{$field}} }, 'value added to non-empty map');
    eq_or_diff($empty->$get_map, $added_values{$field}, 'value added to empty map');
    is($with_field->$get($add_k), $add_v, 'get item');
    $with_field->$set($add_k, $initial_v);
    is($with_field->$get($add_k), $initial_v, 'set item');

    $empty->$set_map({%{$initial_values{$field}}, %{$added_values{$field}}});
    eq_or_diff($empty->$get_map, {%{$initial_values{$field}}, %{$added_values{$field}} }, 'value set to non-empty list');

    $cleared->$clear;
    is($cleared->$get_map, undef, 'cleared list is empty');
}

throws_ok(
    sub { Test::Maps->new->set_uint32_enum_map(4, 77) },
    qr/Invalid value 77 for enumeration field 'test.Maps.uint32_enum_map'/,
    'invalid enum value'
);

throws_ok(
    sub { Test::Maps->new->set_string_int32_map_map(12) },
    qr/Value for field 'test.Maps.string_int32_map' is not a hash reference/,
    'not a hash',
);

throws_ok(
    sub { Test::Maps->new({ string_int32_map => 1 })->set_string_int32_map("c", 7) },
    qr/Value of field 'test.Maps.string_int32_map' is not a hash reference/,
    'not a hash',
);

throws_ok(
    sub { Test::Maps->new->get_string_int32_map("0") },
    qr/Accessing unset map field 'test.Maps.string_int32_map'/,
    'access item of empty hash',
);

throws_ok(
    sub { Test::Maps->new({ string_int32_map => {} })->get_string_int32_map("a") },
    qr/Accessing empty map field 'test.Maps.string_int32_map'/,
    'access item of empty hash',
);

throws_ok(
    sub { Test::Maps->new({ string_int32_map => { b => 14 } })->get_string_int32_map("a") },
    qr/Accessing non-existing key 'a' for field 'test.Maps.string_int32_map'/,
    'access non-existing hash item',
);

done_testing();

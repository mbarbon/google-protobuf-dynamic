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

    $with_field->$add($added_values{$field});
    $empty->$add($added_values{$field});

    is($with_field->$size, 2, 'non-empty list after add');
    is($empty->$size, 1, 'empty list after add');

    eq_or_diff($with_field->$get_list, [$initial_values{$field}, $added_values{$field}], 'value added to non-empty list');
    eq_or_diff($empty->$get_list, [$added_values{$field}], 'value added to empty list');
    is($with_field->$get(1), $added_values{$field}, 'get item');
    $with_field->$set(1, $initial_values{$field});
    is($with_field->$get(1), $initial_values{$field}, 'set item');

    $empty->$set_list([$initial_values{$field}, $added_values{$field}]);
    eq_or_diff($empty->$get_list, [$initial_values{$field}, $added_values{$field}], 'value added to empty list');

    $cleared->$clear;
    is($cleared->$size, 0, 'list cleared');
}

done_testing();

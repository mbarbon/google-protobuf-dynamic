use t::lib::Test 'proto3_optional';

use Google::ProtocolBuffers::Dynamic qw(:descriptor :values);

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("optional_proto3.proto");
$d->map({ package => 'test', prefix => 'Test' });

my $basic = Test::Optional1->message_descriptor();

is($basic->field_count, 22);
is($basic->oneof_count, 11);

my $fields = [@{$basic->fields}[2, 12, 20]];
my $oneofs = [@{$basic->oneofs}[3, 0]];

is($fields->[0]->name, 'int32_f');
is($fields->[0]->has_presence, '');
is($fields->[1]->name, 'int32_pf');
is($fields->[1]->has_presence, 1);
is($fields->[2]->name, 'int32_of');
is($fields->[2]->has_presence, 1);

{
    my $oneof = $oneofs->[0];

    is($oneof->name, '_int32_pf');
    is($oneof->is_synthetic, 1);
    is($oneof->field_count, 1);
    eq_or_diff([qw(int32_pf)], [map $_->name, @{$oneof->fields}]);
}

{
    my $oneof = $oneofs->[1];

    is($oneof->name, 'test');
    is($oneof->is_synthetic, '');
    is($oneof->field_count, 2);
    eq_or_diff([qw(int32_of string_of)], [map $_->name, @{$oneof->fields}]);

    is($oneof->find_field_by_number(68)->name, 'string_of');
    is($oneof->find_field_by_number(42), undef);

    is($oneof->find_field_by_name('int32_of')->number, 67);
    is($oneof->find_field_by_name('int64_pf'), undef);
}

is($fields->[0]->containing_oneof, undef);
is($fields->[1]->containing_oneof->name, '_int32_pf');
is($fields->[1]->real_containing_oneof, undef);
is($fields->[2]->containing_oneof->name, 'test');
is($fields->[2]->real_containing_oneof->name, 'test');

done_testing();

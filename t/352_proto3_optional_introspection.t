use t::lib::Test 'proto3_optional';

use Google::ProtocolBuffers::Dynamic qw(:descriptor :values);

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("optional_proto3.proto");
$d->map({ package => 'test', prefix => 'Test' });

my $basic = Test::Optional1->message_descriptor();

is($basic->field_count, 21);
is($basic->oneof_count, 11);

my $fields = [@{$basic->fields}[2, 12, 20]];
my $oneofs = [@{$basic->oneofs}[2, 10]];

is($fields->[0]->name, 'int32_f');
is($fields->[0]->has_presence, '');
is($fields->[1]->name, 'int32_pf');
is($fields->[1]->has_presence, 1);
is($fields->[2]->name, 'int32_of');
is($fields->[2]->has_presence, 1);

is($oneofs->[0]->name, '_int32_pf');
is($oneofs->[0]->is_synthetic, 1);
is($oneofs->[1]->name, 'test');
is($oneofs->[1]->is_synthetic, '');

is($fields->[0]->containing_oneof, undef);
is($fields->[1]->containing_oneof->name, '_int32_pf');
is($fields->[1]->real_containing_oneof, undef);
is($fields->[2]->containing_oneof->name, 'test');
is($fields->[2]->real_containing_oneof->name, 'test');

done_testing();

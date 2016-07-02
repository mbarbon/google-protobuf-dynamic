use t::lib::Test 'proto3';

use Google::ProtocolBuffers::Dynamic qw(:descriptor :values);

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("map.proto");
$d->map({ package => 'test', prefix => 'Test' });

my $maps = Test::Maps->message_descriptor;
my $item = Test::Item->message_descriptor;
my $string_int32_map = $maps->find_field_by_name('string_int32_map');
my $one_value = $item->find_field_by_name('one_value');

ok($string_int32_map->is_map, 'string_int32_map field marked as map');
ok($string_int32_map->message_type->is_map_entry,
   'key/value pair is marked as a map entry');
ok(!$one_value->is_map, 'one_value field not marked as map');

done_testing();

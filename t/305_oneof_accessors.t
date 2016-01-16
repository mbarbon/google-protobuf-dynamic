use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("oneof.proto");
$d->map({ package => 'test', prefix => 'Test' });

my $obj = Test::OneOf1->new;

is($obj->get_value2, '');
is($obj->get_value3, 0);

$obj->set_value2('abc');

is($obj->get_value2, 'abc');
is($obj->get_value3, 0);

$obj->set_value3(7);

is($obj->get_value2, '');
is($obj->get_value3, 7);

$obj->set_value2('abc');

is($obj->get_value2, 'abc');
is($obj->get_value3, 0);

done_testing();

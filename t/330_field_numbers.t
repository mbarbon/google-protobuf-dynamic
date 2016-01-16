use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("scalar.proto");
$d->map({ package => 'test', prefix => 'Test' });

is(Test::Basic::DOUBLE_F_FIELD_NUMBER(), 1);
is(Test::Basic::ENUM_F_FIELD_NUMBER(), 10);

done_testing();

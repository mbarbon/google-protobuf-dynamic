use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("packed_proto3.proto");
$d->map_package("test", "Test");
$d->resolve_references();

eq_or_diff(Test::Packed->encode({ int32_f => [6, 7, 8] }), "\x1a\x03\x06\x07\x08");
eq_or_diff(Test::Unpacked->encode({ int32_f => [6, 7, 8] }), "\x18\x06\x18\x07\x18\x08");

done_testing();

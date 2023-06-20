use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("packed_proto3.proto");
$d->map_package("test", "Test");
$d->resolve_references();

encode_eq_or_diff('Test::Packed', { int32_f => [6, 7, 8] }, "\x1a\x03\x06\x07\x08");
encode_eq_or_diff('Test::Unpacked', { int32_f => [6, 7, 8] }, "\x18\x06\x18\x07\x18\x08");

done_testing();

use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_string("person.proto", <<'EOT');
syntax = "proto2";

package humans;

message Person {
  repeated SomeSubMessage foo = 6;
  message SomeSubMessage {
    repeated int32 arrayOfInts = 2;
  }
}
EOT

$d->map_package_prefix('humans', 'Humans');
$d->resolve_references();

encode_eq_or_diff('Humans::Person', {foo => [{}]}, "\x32\x00", 'sanity check');
encode_throws_ok(
    'Humans::Person', {foo => [{arrayOfInts => undef}]},
    qr/Not an array reference when encoding field/,
    'encoding failure in inner message',
);
encode_eq_or_diff('Humans::Person', {foo => [{}]}, "\x32\x00", "works after error");

done_testing();

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

eq_or_diff(Humans::Person->encode({foo => [{}]}), "\x32\x00", 'sanity check');
throws_ok(
    sub { Humans::Person->encode({foo => [{arrayOfInts => undef}]}) },
    qr/Not an array reference when encoding field/,
    'encoding failure in inner message',
);
eq_or_diff(Humans::Person->encode({foo => [{}]}), "\x32\x00", "works after error");

done_testing();

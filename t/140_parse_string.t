use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new;
$d->load_string("person.proto", <<'EOT');
syntax = "proto2";

package test;

message Person {
  required string name = 1;
  required int32 id = 2;
  optional string email = 3;
}
EOT

$d->map({ package => 'test', prefix => 'Test' });

my $p = Test::Person->decode("\x0a\x03foo\x10\x1f");

eq_or_diff($p, Test::Person->new({ id => 31, name => 'foo' }));

done_testing();

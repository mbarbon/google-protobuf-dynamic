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

decode_eq_or_diff('Test::Person', "\x0a\x03foo\x10\x1f",
                  Test::Person->new({ id => 31, name => 'foo' }));

done_testing();

use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_string("odd_enums.proto", <<'EOT');
syntax = "proto3";

package enums;

enum Keywords {
  DEFAULT = 0;
  STDIN   = 1;
  STDOUT  = 2;
  STDERR  = 3;
  ARGV    = 4;
  ARGVOUT = 5;
  ENV     = 6;
  INC     = 7;
  SIG     = 8;
  _       = 9;
  BEGIN   = 10;
  CHECK   = 11;
  END     = 12;
  INIT    = 13;
  UNITCHECK=14;
}
EOT

$d->map({ package => 'enums', prefix => 'Enums'});

my $values = Enums::Keywords->enum_descriptor->values;

for my $name (sort keys %$values) {
    # could be done without eval "", but this is simpler...
    my $code = sprintf <<'EOT', $name, $name, $name, $values->{$name}, $name;
    is(*%s{CODE}, undef, '%s - did not define a sub in main');
    is(Enums::Keywords::%s(), %d, '%s - did define the constant');

    1;
EOT

    eval $code or do {
        my $err = $@ // "Zombie error";
        fail 'eval failed';
        diag $err;
    };
}

done_testing();

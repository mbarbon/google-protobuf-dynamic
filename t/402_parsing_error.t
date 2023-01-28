use t::lib::Test;

my $error_proto = <<'EOT';
syntax = "proto3";

message Wrong {
    int32 int32_f;
}
EOT

my $d = Google::ProtocolBuffers::Dynamic->new;

throws_ok(sub { $d->load_string('error.proto', $error_proto) },
          qr/Error during protobuf parsing: error.proto:3:17: Missing field number/);

done_testing();

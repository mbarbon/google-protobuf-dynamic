use t::lib::Test;

my $error_proto = <<'EOT';
syntax = "proto3";

message Wrong {
    int32 int32_f;
}
EOT

my $warning_proto = <<'EOT';
message Wrong {
    optional int32 int32_f = 1;
}
EOT

my $d = Google::ProtocolBuffers::Dynamic->new;

throws_ok(sub { $d->load_string('error.proto', $error_proto) },
          qr/Error during protobuf parsing: error.proto:3:17: Missing field number/);

warning_like(sub { $d->load_string('warning.proto', $warning_proto) },
             qr/No syntax specified for the proto file: warning.proto./);

done_testing();

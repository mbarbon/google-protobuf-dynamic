use t::lib::Test;
use Alien::ProtoBuf;

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

if (Alien::ProtoBuf->atleast_version('5.31.0')) {
    warning_like(sub { $d->load_string('warning.proto', $warning_proto) },
                 qr/No edition or syntax specified for the proto file: warning.proto./);
} else {
    warning_like(sub { $d->load_string('warning.proto', $warning_proto) },
                 qr/No syntax specified for the proto file: warning.proto./);
}

done_testing();

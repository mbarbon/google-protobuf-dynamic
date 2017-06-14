use t::lib::Test;

use MIME::Base64;

# obtained from the test case in 63e04f32bf2cae65697222a0773597b5ba9a3740, before the fix
my $broken_descriptor = <<'EOD';
CooCCglmb28ucHJvdG8SA2ZvbyIvCgNNb28SDgoDbW9vGAEoCVIDbW9vEg4KA2JvbxgCKAlSA2Jv
b0IICgZvbmVfb2ZKvgEKBhIEAAAJAQoICgEMEgMAABIKCAoBAhIDAggLCgoKAgQAEgQEAAkBCgoK
AwQAARIDBAgLCgwKBAQACAASBAUICAkKDAoFBAAIAAESAwUOFAoLCgQEAAIAEgMGDh0KDAoFBAAC
AAUSAwYOFAoMCgUEAAIAARIDBhUYCgwKBQQAAgADEgMGGxwKCwoEBAACARIDBw4dCgwKBQQAAgEF
EgMHDhQKDAoFBAACAQESAwcVGAoMCgUEAAIBAxIDBxscYgZwcm90bzM=
EOD
my $d = Google::ProtocolBuffers::Dynamic->new;

throws_ok(
    sub { $d->load_serialized_string(MIME::Base64::decode_base64($broken_descriptor)) },
    qr/Oneof must have at least one field./
);

lives_ok(
    sub { undef $d },
    'Leaves the descriptor pool in a consistent state',
);

done_testing();

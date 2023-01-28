package Protoc::WKT::Scalar;

use strict;
use warnings;
# @@protoc_insertion_point(after_pragmas)
use MIME::Base64 qw();
use Google::ProtocolBuffers::Dynamic;

my $gpd = Google::ProtocolBuffers::Dynamic->new;

$gpd->load_serialized_string(MIME::Base64::decode_base64(<<'EOD'));
CvkFChh0L3Byb3RvL3drdC9zY2FsYXIucHJvdG8SBHRlc3QaHmdvb2dsZS9wcm90b2J1Zi9kdXJh
dGlvbi5wcm90bxofZ29vZ2xlL3Byb3RvYnVmL3RpbWVzdGFtcC5wcm90bxoeZ29vZ2xlL3Byb3Rv
YnVmL3dyYXBwZXJzLnByb3RvIu0ECgVCYXNpYxI7Cgt0aW1lc3RhbXBfZhgBIAEoCzIaLmdvb2ds
ZS5wcm90b2J1Zi5UaW1lc3RhbXBSCnRpbWVzdGFtcEYSOAoKZHVyYXRpb25fZhgCIAEoCzIZLmdv
b2dsZS5wcm90b2J1Zi5EdXJhdGlvblIJZHVyYXRpb25GEjcKCGRvdWJsZV9mGAMgASgLMhwuZ29v
Z2xlLnByb3RvYnVmLkRvdWJsZVZhbHVlUgdkb3VibGVGEjQKB2Zsb2F0X2YYBCABKAsyGy5nb29n
bGUucHJvdG9idWYuRmxvYXRWYWx1ZVIGZmxvYXRGEjQKB2ludDY0X2YYBSABKAsyGy5nb29nbGUu
cHJvdG9idWYuSW50NjRWYWx1ZVIGaW50NjRGEjcKCHVpbnQ2NF9mGAYgASgLMhwuZ29vZ2xlLnBy
b3RvYnVmLlVJbnQ2NFZhbHVlUgd1aW50NjRGEjQKB2ludDMyX2YYByABKAsyGy5nb29nbGUucHJv
dG9idWYuSW50MzJWYWx1ZVIGaW50MzJGEjcKCHVpbnQzMl9mGAggASgLMhwuZ29vZ2xlLnByb3Rv
YnVmLlVJbnQzMlZhbHVlUgd1aW50MzJGEjEKBmJvb2xfZhgJIAEoCzIaLmdvb2dsZS5wcm90b2J1
Zi5Cb29sVmFsdWVSBWJvb2xGEjcKCHN0cmluZ19mGAogASgLMhwuZ29vZ2xlLnByb3RvYnVmLlN0
cmluZ1ZhbHVlUgdzdHJpbmdGEjQKB2J5dGVzX2YYCyABKAsyGy5nb29nbGUucHJvdG9idWYuQnl0
ZXNWYWx1ZVIGYnl0ZXNGYgZwcm90bzM=

EOD


# @@protoc_insertion_point(after_loading)

$gpd->map(
   +{
      'package' => 'test',
      'prefix' => 'Protoc::WKT::Scalar::Test'
    },
);

# @@protoc_insertion_point(after_mapping)

undef $gpd;

1;

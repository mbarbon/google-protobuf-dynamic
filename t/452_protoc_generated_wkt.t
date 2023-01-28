use t::lib::Test;

use lib 't/lib/generated';

# protoc -include_imports includes all dependencies in the serialized
# output, while the set of descriptors passed to plugins does not contain
# WKTs. Hence the need for a separate test.

# protoc --perl-gpd_out=package=Protoc.WKT.Scalar:t/lib/generated/ t/proto/wkt/scalar.proto
use_ok('Protoc::WKT::Scalar');

done_testing();

use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("edge.proto");
$d->map_package("test", "Test1", { decode_blessed => 0 });
$d->resolve_references();

# inner message
{
    # empty message
    decode_eq_or_diff('Test1::EmptyFields', "\x0a\x00", { empty_message => {} }, 'empty string');
    # wrong wire type for the field
    decode_eq_or_diff('Test1::EmptyFields', "\x08\x00", {}, 'string field, varint wire type');
    decode_eq_or_diff('Test1::EmptyFields', "\x09\x00\x00\x00\x00\x00\x00\x00\x00", {}, 'string field, fixed64 wire type');
    decode_eq_or_diff('Test1::EmptyFields', "\x0d\x00\x00\x00\x00", {}, 'string field, fixed32 wire type');
}

# string
{
    # empty string
    decode_eq_or_diff('Test1::EmptyFields', "\x32\x00", { some_string => '' }, 'empty message');
    # wrong wire type for the field
    decode_eq_or_diff('Test1::EmptyFields', "\x30\x00", {}, 'message field, varint wire type');
    decode_eq_or_diff('Test1::EmptyFields', "\x31\x00\x00\x00\x00\x00\x00\x00\x00", {}, 'message field, fixed64 wire type');
    decode_eq_or_diff('Test1::EmptyFields', "\x35\x00\x00\x00\x00", {}, 'message field, fixed32 wire type');
}

# packed varint
{
    # correct field
    decode_eq_or_diff('Test1::EmptyFields', "\x18\x00", { packed_int32 => [0] }, 'varint field, varint wire type');
    decode_eq_or_diff('Test1::EmptyFields', "\x1a\x01\x00", { packed_int32 => [0] }, 'varint field, packed field');
    # wrong wire type for the field
    decode_eq_or_diff('Test1::EmptyFields', "\x19\x00\x00\x00\x00\x00\x00\x00\x00", {}, 'varint field, fixed64 wire type');
    decode_eq_or_diff('Test1::EmptyFields', "\x1d\x00\x00\x00\x00", {}, 'varint field, fixed32 wire type');
}

# packed fixed32
{
    # correct field
    decode_eq_or_diff('Test1::EmptyFields', "\x25\x00\x00\x00\x00", { packed_fixed32 => [0] }, 'fixed32 field, fixed32 wire type');
    decode_eq_or_diff('Test1::EmptyFields', "\x22\x04\x00\x00\x00\x00", { packed_fixed32 => [0] }, 'fixed32 field, packed field');
    # wrong wire type for the field
    decode_eq_or_diff('Test1::EmptyFields', "\x20\x00", {}, 'fixed32 field, varint wire type');
    decode_eq_or_diff('Test1::EmptyFields', "\x21\x00\x00\x00\x00\x00\x00\x00\x00", {}, 'fixed32 field, fixed64 wire type');
}

# packed fixed64
{
    # correct field
    decode_eq_or_diff('Test1::EmptyFields', "\x29\x00\x00\x00\x00\x00\x00\x00\x00", { packed_fixed64 => [0] }, 'fixed64 field, fixed64 wire type');
    decode_eq_or_diff('Test1::EmptyFields', "\x2a\x08\x00\x00\x00\x00\x00\x00\x00\x00", { packed_fixed64 => [0] }, 'fixed64 field, packed field');
    # wrong wire type for the field
    decode_eq_or_diff('Test1::EmptyFields', "\x28\x00", {}, 'fixed64 field, varint wire type');
    decode_eq_or_diff('Test1::EmptyFields', "\x2d\x00\x00\x00\x00", {}, 'fixed64 field, fixed32 wire type');
}

# unknown fields
{
    decode_eq_or_diff('Test1::UnknownFields', "\x08\x01\x10\x02\x18\x04\xc0\x3e\x03", { field2 => 2, field1000 => 3 }, 'unknown fields are skipped');
}

done_testing();

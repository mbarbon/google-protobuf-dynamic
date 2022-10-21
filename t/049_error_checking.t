use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("edge.proto");
$d->map_package("test", "Test1", { decode_blessed => 0 });
$d->resolve_references();

# unknown wire type
decode_throws_ok('Test1::EmptyFields', "\x1f\x00",
                 qr/Invalid wire type/,
                 qr/Unrecognized protocol buffer wire type/,
                 'packed field with 0 size at end of message');

# packed field with 0 length
{
    decode_throws_ok('Test1::EmptyFields', "\x1a\x00",
                     qr/Unexpected EOF inside delimited string/,
                     qr/Packed field with size 0/,
                     'packed field with 0 size at end of message');
    decode_throws_ok('Test1::EmptyFields', "\x1a\x00\x0a\x00",
                     qr/Submessage ended in the middle of a value or group/,
                     qr/Packed field with size 0/,
                     'packed field with 0 size in the middle of a message');
}

# Truncated field tag
{
    decode_throws_ok('Test1::EmptyFields', "\x80",
                     qr/Unexpected EOF: decoder still has buffered unparsed data/,
                     qr/Invalid\/truncated field tag/,
                     'truncated varint tag in main message');

    decode_throws_ok('Test1::EmptyFields', "\x12\x01\x80",
                     qr/Submessage ended in the middle of a value or group/,
                     qr/Invalid\/truncated field tag/,
                     'truncated varint tag in sub message');

    decode_throws_ok('Test1::EmptyFields', "\x52\x01A\x81",
                     qr/Unexpected EOF: decoder still has buffered unparsed data/,
                     qr/Invalid\/truncated field tag/,
                     'truncated varint tag in sub message');
}

# truncated varint field value
{
    decode_throws_ok('Test1::EmptyFields', "\x38\x81",
                     qr/Unexpected EOF: decoder still has buffered unparsed data/,
                     qr/Invalid\/truncated varint field/,
                     'truncated varint value in main message');

    decode_throws_ok('Test1::OuterMessage', "\x0a\x02\x38\x81\x01",
                     qr/Submessage ended in the middle of a value or group/,
                     qr/Invalid\/truncated varint field/,
                     'truncated varint value in sub message');
}

# truncated length-delimited field length
{
    decode_throws_ok('Test1::EmptyFields', "\x32\x81",
                     qr/Unexpected EOF: decoder still has buffered unparsed data/,
                     qr/Invalid\/truncated field length/,
                     'truncated length-delimited length in main message');

    decode_throws_ok('Test1::OuterMessage', "\x0a\x02\x32\x81\x01",
                     qr/Submessage ended in the middle of a value or group/,
                     qr/Invalid\/truncated field length/,
                     'truncated length-delimited length in sub message');
}

# truncated length-delimited field value
{
    decode_throws_ok('Test1::EmptyFields', "\x32\x02X",
                     qr/Unexpected EOF inside delimited string/,
                     qr/Truncated length-delimited field/,
                     'truncated length-delimited value in main message');

    decode_throws_ok('Test1::OuterMessage', "\x0a\x03\x32\x02XY",
                     qr/Submessage end extends past enclosing submessage/,
                     qr/Truncated length-delimited field/,
                     'truncated length-delimited value in sub message');
}

# truncated fixed64 field value
{
    decode_throws_ok('Test1::EmptyFields', "\x41\x00\x00\x00\x00\x00\x00\x00",
                     qr/Unexpected EOF: decoder still has buffered unparsed data/,
                     qr/Truncated 64-bit fixed size field/,
                     'truncated fixed64 value in main message');

    decode_throws_ok('Test1::OuterMessage', "\x0a\x08\x41\x00\x00\x00\x00\x00\x00\x00\x00",
                     qr/Submessage ended in the middle of a value or group/,
                     qr/Truncated 64-bit fixed size field/,
                     'truncated fixed64 value in sub message');
}

# truncated fixed32 field value
{
    decode_throws_ok('Test1::EmptyFields', "\x4d\x00\x00\x00",
                     qr/Unexpected EOF: decoder still has buffered unparsed data/,
                     qr/Truncated 32-bit fixed size field/,
                     'truncated fixed32 value in main message');

    decode_throws_ok('Test1::OuterMessage', "\x0a\x04\x4d\x00\x00\x00\x00",
                     qr/Submessage ended in the middle of a value or group/,
                     qr/Truncated 32-bit fixed size field/,
                     'truncated fixed32 value in sub message');
}

# truncated packed varint field value
{
    decode_throws_ok('Test1::EmptyFields', "\x1a\x03\x01\x81\x81\x01",
                     qr/Submessage ended in the middle of a value or grou/,
                     qr/Invalid\/truncated varint packed field value/,
                     'truncated varint value in packed field');

    decode_throws_ok('Test1::OuterMessage', "\x0a\x05\x1a\x03\x01\x81\x81\x01",
                     qr/Submessage ended in the middle of a value or group/,
                     qr/Invalid\/truncated varint packed field value/,
                     'truncated varint value in packed field in sub message');
}

# truncated packed fixed64 field value
{
    decode_throws_ok('Test1::EmptyFields', "\x2a\x07\x00\x00\x00\x00\x00\x00\x00\x00",
                     qr/Submessage ended in the middle of a value or grou/,
                     qr/Invalid\/truncated 64-bit fixed size packed field value/,
                     'truncated fixed64 value in packed field');

    decode_throws_ok('Test1::OuterMessage', "\x0a\x0a\x2a\x07\x08\x08\x00\x00\x00\x00\x00\x00",
                     qr/Submessage ended in the middle of a value or group/,
                     qr/Invalid\/truncated 64-bit fixed size packed field value/,
                     'truncated fixed64 value in packed field in sub message');
}

# truncated packed fixed32 field value
{
    decode_throws_ok('Test1::EmptyFields', "\x22\x03\x00\x00\x00\x00",
                     qr/Submessage ended in the middle of a value or grou/,
                     qr/Invalid\/truncated 32-bit fixed size packed field value/,
                     'truncated fixed32 value in packed field');

    decode_throws_ok('Test1::OuterMessage', "\x0a\x05\x22\x03\x00\x00\x00\x00",
                     qr/Submessage ended in the middle of a value or group/,
                     qr/Invalid\/truncated 32-bit fixed size packed field value/,
                     'truncated fixed32 value in packed field in sub message');
}

done_testing();

use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("oneof.proto");
$d->map_message("test.OneOf1", "OneOf1");
$d->resolve_references();

{
    my $encoded = "\x08\x03\x12\x03foo";
    my $decoded = OneOf1->new({
        value1 => 3,
        value2 => 'foo',
    });

    eq_or_diff(OneOf1->decode_to_perl($encoded), $decoded);
    eq_or_diff(OneOf1->encode_from_perl($decoded), $encoded);
}

{
    my $encoded = "\x08\x03\x18\x04";
    my $decoded = OneOf1->new({
        value1 => 3,
        value3 => 4,
    });

    eq_or_diff(OneOf1->decode_to_perl($encoded), $decoded);
    eq_or_diff(OneOf1->encode_from_perl($decoded), $encoded);
}

{
    my $encoded = "\x08\x03\x12\x03foo\x18\x04";
    my $decoded = OneOf1->new({
        value1 => 3,
        value3 => 4,
    });

    eq_or_diff(OneOf1->decode_to_perl($encoded), $decoded);
}

{
    my $encoded = "\x08\x03\x18\x04\x12\x03foo";
    my $decoded = OneOf1->new({
        value1 => 3,
        value2 => 'foo',
    });

    eq_or_diff(OneOf1->decode_to_perl($encoded), $decoded);
}

{
    my $encoded = "\x08\x03\x12\x03foo";
    my $decoded = OneOf1->new({
        value1 => 3,
        value2 => 'foo',
        value3 => 4,
    });

    eq_or_diff(OneOf1->encode_from_perl($decoded), $encoded);
}

done_testing();

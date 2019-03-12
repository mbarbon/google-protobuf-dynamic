use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("oneof.proto");
$d->map_message("test.OneOf1", "OneOf1");
$d->resolve_references();

{
    my $encoded = "\x10\x03\x1a\x03foo";
    my $decoded = OneOf1->new({
        value1 => 3,
        value2 => 'foo',
    });

    eq_or_diff(OneOf1->decode($encoded), $decoded);
    eq_or_diff(OneOf1->encode($decoded), $encoded);
}

{
    my $encoded = "\x08\x04\x10\x03";
    my $decoded = OneOf1->new({
        value1 => 3,
        value3 => 4,
    });

    eq_or_diff(OneOf1->decode($encoded), $decoded);
    eq_or_diff(OneOf1->encode($decoded), $encoded);
}

{
    my $encoded = "\x10\x03\x1a\x03foo\x08\x04";
    my $decoded = OneOf1->new({
        value1 => 3,
        value3 => 4,
    });

    eq_or_diff(OneOf1->decode($encoded), $decoded);
}

{
    my $encoded = "\x10\x03\x08\x04\x1a\x03foo";
    my $decoded = OneOf1->new({
        value1 => 3,
        value2 => 'foo',
    });

    eq_or_diff(OneOf1->decode($encoded), $decoded);
}

{
    my $encoded = "\x08\x04\x10\x03";
    my $decoded = OneOf1->new({
        value1 => 3,
        value2 => 'foo',
        value3 => 4,
    });

    eq_or_diff(OneOf1->encode($decoded), $encoded);
}

done_testing();

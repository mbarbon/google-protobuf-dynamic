use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("recurse.proto");
$d->map_message("test.List", "List");
$d->resolve_references();

{
    my $encoded = "\x08\x02";
    my $decoded = List->new({
        value => 2,
    });

    decode_eq_or_diff('List', $encoded, $decoded);
    eq_or_diff(List->encode($decoded), $encoded);
}

{
    my $encoded = "\x08\x02\x12\x02\x08\x01";
    my $decoded = List->new({
        value => 2,
        next  => List->new({
            value => 1,
        }),
    });

    decode_eq_or_diff('List', $encoded, $decoded);
    eq_or_diff(List->encode($decoded), $encoded);
}

done_testing();

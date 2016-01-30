use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("message.proto");
$d->map_message("test.Inner", "Inner", { explicit_defaults => 1 });
$d->map_message("test.OuterWithMessage", "OuterWithMessage", { explicit_defaults => 1 });
$d->map_message("test.OuterWithGroup", "OuterWithGroup", { explicit_defaults => 1 });
$d->resolve_references();

{
    my $encoded = "\x0a\x02\x08\x02\x12\x02\x08\x03\x12\x02\x08\x04";
    my $decoded = OuterWithMessage->new({
        optional_inner => Inner->new({
            value => 2,
            other => 0,
        }),
        repeated_inner => [
            Inner->new({ value => 3, other => 0 }),
            Inner->new({ value => 4, other => 0 }),
        ],
    });
    my $for_encode = OuterWithMessage->new({
        optional_inner => {
            value => 2,
        },
        repeated_inner => [
            { value => 3 },
            { value => 4 },
        ],
    });

    eq_or_diff(OuterWithMessage->decode($encoded), $decoded);
    eq_or_diff(OuterWithMessage->encode($for_encode), $encoded);
}

{
    my $encoded = "\x0b\x08\x02\x0c\x0b\x08\x03\x0c";
    my $decoded = OuterWithGroup->new({
        inner => [
            OuterWithGroup::Inner->new({ value => 2 }),
            OuterWithGroup::Inner->new({ value => 3 }),
        ],
    });

    eq_or_diff(OuterWithGroup->decode($encoded), $decoded);
    eq_or_diff(OuterWithGroup->encode($decoded), $encoded);
}

# unusual, but the spec excplicitly mentions this
{
    my $encoded = "\x0a\x02\x08\x02\x0a\x02\x10\x07";
    my $reencoded = "\x0a\x04\x08\x02\x10\x07";
    my $decoded = OuterWithMessage->new({
        optional_inner => Inner->new({
            value => 2,
            other => 7,
        }),
    });

    eq_or_diff(OuterWithMessage->decode($encoded), $decoded);
    eq_or_diff(OuterWithMessage->encode($decoded), $reencoded);
}

done_testing();

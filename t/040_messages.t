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
    my $for_encode = {
        optional_inner => {
            value => 2,
        },
        repeated_inner => [
            { value => 3 },
            { value => 4 },
        ],
    };

    my $tied;
    {
        my ($v2, $v3, $v4) = (2, 3, 4);
        my $value_2 = { value => undef };
        my $value_3 = { value => undef };
        my $value_4 = { value => undef };
        my $repeated = [undef, undef];
        my $value = {
            optional_inner => undef,
            repeated_inner => undef,
        };

        tie_scalar($value_2->{value}, $v2);
        tie_scalar($value_3->{value}, $v3);
        tie_scalar($value_4->{value}, $v4);
        tie_scalar($repeated->[0], $value_3);
        tie_scalar($repeated->[1], $value_4);
        tie_scalar($value->{optional_inner}, $value_2);
        tie_scalar($value->{repeated_inner}, $repeated);
        tie_scalar($tied, $value);
    };

    eq_or_diff(OuterWithMessage->decode($encoded), $decoded);
    eq_or_diff(OuterWithMessage->encode($for_encode), $encoded);
    eq_or_diff(OuterWithMessage->encode($tied), $encoded);
    eq_or_diff(tied_fetch_count($tied), {
        count => 1,
        inner => {
            optional_inner => {
                count => 1,
                inner => {
                    value => 1,
                },
            },
            repeated_inner => {
                count => 1,
                inner => [
                    {
                        count => 1,
                        inner => {
                            value => 1,
                        },
                    },
                    {
                        count => 1,
                        inner => {
                            value => 1,
                        },
                    },
                ],
            },
        },
    });
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

# unusual, but the spec explicitly mentions this
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

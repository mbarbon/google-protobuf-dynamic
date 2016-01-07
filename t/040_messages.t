use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("message.proto");
$d->map_message("test.Inner", "Inner");
$d->map_message("test.OuterWithMessage", "OuterWithMessage");
$d->map_message("test.OuterWithGroup", "OuterWithGroup");
$d->resolve_references();

{
    my $encoded = "\x0a\x02\x08\x02\x12\x02\x08\x03\x12\x02\x08\x04";
    my $decoded = {
        optional_inner => {
            value => 2,
            other => 0,
        },
        repeated_inner => [
            { value => 3, other => 0 },
            { value => 4, other => 0 },
        ],
    };
    my $for_encode = {
        optional_inner => {
            value => 2,
        },
        repeated_inner => [
            { value => 3 },
            { value => 4 },
        ],
    };

    eq_or_diff(OuterWithMessage->decode_to_perl($encoded), $decoded);
    eq_or_diff(OuterWithMessage->encode_from_perl($for_encode), $encoded);
}

{
    my $encoded = "\x0b\x08\x02\x0c\x0b\x08\x03\x0c";
    my $decoded = {
        inner => [
            { value => 2 },
            { value => 3 },
        ],
    };

    eq_or_diff(OuterWithGroup->decode_to_perl($encoded), $decoded);
    eq_or_diff(OuterWithGroup->encode_from_perl($decoded), $encoded);
}

# unusual, but the spec excplicitly mentions this
{
    my $encoded = "\x0a\x02\x08\x02\x0a\x02\x10\x07";
    my $reencoded = "\x0a\x04\x08\x02\x10\x07";
    my $decoded = {
        optional_inner => {
            value => 2,
            other => 7,
        },
    };

    eq_or_diff(OuterWithMessage->decode_to_perl($encoded), $decoded);
    eq_or_diff(OuterWithMessage->encode_from_perl($decoded), $reencoded);
}

done_testing();

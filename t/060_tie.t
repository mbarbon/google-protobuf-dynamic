use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("scalar.proto");
$d->load_file("repeated.proto");
$d->load_file("message.proto");
$d->map_message("test.Basic", "Basic", { check_enum_values => 0 });
$d->map_message("test.Repeated", "Repeated");
$d->map_message("test.Inner", "Inner");
$d->map_message("test.OuterWithMessage", "OuterWithMessage");
$d->resolve_references();

{
    my $tied = tied_hash();

    encode_eq_or_diff('Basic', $tied, "");
    eq_or_diff(tied_fetch_count($tied), {
        count => 0,
        inner => {},
    });
}

{
    my $tied = tied_hash(
        bool_f  => 1,
        int32_f => 2,
    );

    encode_eq_or_diff('Basic', $tied, "\x18\x02\x38\x01");
    eq_or_diff(tied_fetch_count($tied), {
        count => 4,
        inner => {
            bool_f  => -1,
            int32_f => -1,
        },
    });
}

{
    my $tied = {
        bool_f => tied_array(0, 1, 1),
    };

    encode_eq_or_diff('Repeated', $tied, "\x38\x00\x38\x01\x38\x01");
    eq_or_diff(tied_fetch_count($tied), {
        bool_f => {
            count => 6,
            inner => [-1, -1, -1],
        },
    });
}

{
    my $tied = {
        optional_inner => tied_hash(value => 3),
        repeated_inner => tied_array(
            tied_hash(value => 4),
            tied_hash(value => 5),
        ),
    };

    encode_eq_or_diff('OuterWithMessage', $tied,
                      "\x0a\x02\x08\x03\x12\x02\x08\x04\x12\x02\x08\x05");
    eq_or_diff(tied_fetch_count($tied), {
        optional_inner => {
            count => 2,
            inner => {
                value => -1,
            },
        },
        repeated_inner => {
            count => 4,
            inner => [
                {
                    count => 2,
                    inner => {
                        value => -1,
                    },
                },
                {
                    count => 2,
                    inner => {
                        value => -1,
                    },
                },
            ],
        },
    });
}

done_testing();


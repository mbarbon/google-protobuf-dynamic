use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("scalar.proto");
$d->load_file("repeated.proto");
$d->load_file("map.proto");
$d->load_file("message.proto");
$d->map_message("test.Basic", "Basic", { check_enum_values => 0 });
$d->map_message("test.Repeated", "Repeated");
$d->map_message("test.Inner", "Inner");
$d->map_message("test.OuterWithMessage", "OuterWithMessage");
$d->map_message("test.StringMap", "StringMap");
$d->map_message("test.Item", "MapItem");
$d->map_message("test.Maps", "Maps");
$d->resolve_references();

{
    my $tied = tied_hash();

    eq_or_diff(Basic->encode($tied), "");
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

    eq_or_diff(Basic->encode($tied), "\x18\x02\x38\x01");
    eq_or_diff(tied_fetch_count($tied), {
        count => 2,
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

    eq_or_diff(Repeated->encode($tied), "\x38\x00\x38\x01\x38\x01");
    eq_or_diff(tied_fetch_count($tied), {
        bool_f => {
            count => 3,
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

    eq_or_diff(OuterWithMessage->encode($tied),
               "\x0a\x02\x08\x03\x12\x02\x08\x04\x12\x02\x08\x05");
    eq_or_diff(tied_fetch_count($tied), {
        optional_inner => {
            count => 1,
            inner => {
                value => -1,
            },
        },
        repeated_inner => {
            count => 2,
            inner => [
                {
                    count => 1,
                    inner => {
                        value => -1,
                    },
                },
                {
                    count => 1,
                    inner => {
                        value => -1,
                    },
                },
            ],
        },
    });
}

{
    my $tied = {
        string_int32_map => tied_hash(a => 19),
    };

    eq_or_diff(Maps->encode($tied), "\x0a\x05\x0a\x01a\x10\x13");
    eq_or_diff(tied_fetch_count($tied), {
        string_int32_map => {
            count => 3, # firstkey + nextkey + fetch
            inner => {
                a     => -1,
            },
        }
    });
}

done_testing();

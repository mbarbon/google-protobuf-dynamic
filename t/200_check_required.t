use t::lib::Test;

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("options.proto");
    $d->map({ package => 'test', prefix => 'Test1', options => { check_required_fields => 1 } });

    eq_or_diff(
        Test1::Required->decode("\x08\x02"),
        Test1::Required->new({ value => 2 }),
    );

    throws_ok(
        sub { Test1::Required->decode("") },
        qr/Deserialization failed: Missing required field test.Required.value/,
        "Missing required field",
    );
}

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("options.proto");
    $d->map({ package => 'test', prefix => 'Test2', options => { check_required_fields => 0 } });

    eq_or_diff(
        Test2::Required->decode("\x08\x02"),
        Test2::Required->new({ value => 2 }),
    );

    lives_ok(
        sub { Test2::Required->decode("") },
        "Missing required field not checked",
    );
    eq_or_diff(
        Test2::Required->decode(""),
        Test2::Required->new({}),
    );
}

done_testing();

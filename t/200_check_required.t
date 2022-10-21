use t::lib::Test;

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("options.proto");
    $d->map({ package => 'test', prefix => 'Test1', options => { check_required_fields => 1 } });

    decode_eq_or_diff(
        'Test1::Required', "\x08\x02",
        Test1::Required->new({ value => 2 }),
    );

    decode_throws_ok(
        'Test1::Required', "",
        qr/Deserialization failed: Missing required field test.Required.value/,
        qr/Deserialization failed: Missing required field test.Required.value/,
        "Missing required field",
    );
}

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("options.proto");
    $d->map({ package => 'test', prefix => 'Test2', options => { check_required_fields => 0 } });

    decode_eq_or_diff(
        'Test2::Required', "\x08\x02",
        Test2::Required->new({ value => 2 }),
    );

    decode_eq_or_diff(
        'Test2::Required', "",
        Test2::Required->new({}),
    );
}

done_testing();

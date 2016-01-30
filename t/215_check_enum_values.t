use t::lib::Test;

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("enum.proto");
    $d->map({ package => 'test', prefix => 'Test1', options => { check_enum_values => 1 } });

    eq_or_diff(
        Test1::MessageBefore->decode(Test1::MessageAfter->encode({ value => 3, array => [3, 2, 1] })),
        Test1::MessageBefore->new({ array => [1, 2, 1] }),
        "unknown enum value uses default in deserialization",
    );

    throws_ok(
        sub { Test1::MessageBefore->encode({ value => 3 }) },
        qr/Invalid enumeration value 3 for field 'test.MessageBefore.value'/,
        "unknown enum value croaks in serialization"
    );

    throws_ok(
        sub { Test1::MessageBefore->encode({ array => [3, 2] }) },
        qr/Invalid enumeration value 3 for field 'test.MessageBefore.array'/,
        "unknown enum value croaks in serialization"
    );

    throws_ok(
        sub { Test1::MessageBefore->new->set_value(3) },
        qr/Invalid value 3 for enumeration field 'test.MessageBefore.value'/,
        "unknown enum value croaks on set"
    );
}

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("enum.proto");
    $d->map({ package => 'test', prefix => 'Test2', options => { check_enum_values => 0 } });

    eq_or_diff(
        Test2::MessageBefore->decode(Test2::MessageAfter->encode({ value => 3, array => [3, 2, 1] })),
        Test2::MessageBefore->new({ value => 3, array => [3, 2, 1] }),
    );

    eq_or_diff(
        Test2::MessageBefore->encode({ value => 3 }),
        "\x08\x03",
    );

    eq_or_diff(
        Test2::MessageBefore->encode({ array => [3, 2] }),
        "\x10\x03\x10\x02",
    );

    lives_ok(
        sub { Test2::MessageBefore->new->set_value(3) },
        "unknown enum value croaks in serialization"
    );
}

done_testing();

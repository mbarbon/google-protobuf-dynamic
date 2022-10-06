use t::lib::Test;

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto/mapping');

    $d->load_file("test1.proto");
    $d->map_message("test1.Message1", "Test1::FirstMessage");
    $d->map_message("test1.Message2", "Test1::AnotherMessage");
    $d->map_message("test1.Message3", "Test1::CompositeMessage");
    $d->map_message("test1.Message4.Message5", "Test1::InnerMessage");
    $d->map_message("test1.Message4", "Test1::OuterMessage");
    $d->map_enum('test1.Enum', 'Test1::Enumeration');
    $d->resolve_references();

    eq_or_diff(Test1::FirstMessage->decode("\x08\x01"), Test1::FirstMessage->new({
        test1_message1 => 1,
    }), "simple message 2");
    eq_or_diff(Test1::AnotherMessage->decode("\x08\x01"), Test1::AnotherMessage->new({
        test1_message2 => 1,
    }), "simple message 2");
    eq_or_diff(Test1::CompositeMessage->decode("\x0a\x02\x08\x01\x12\x02\x08\x01"), Test1::CompositeMessage->new({
        test1_message3_message1 => Test1::FirstMessage->new({
            test1_message1 => 1,
        }),
        test1_message3_message2 => Test1::AnotherMessage->new({
            test1_message2 => 1,
        }),
    }), "composite message");
    eq_or_diff(Test1::OuterMessage->decode("\x12\x02\x08\x0f"), Test1::OuterMessage->new({
        inner => Test1::InnerMessage->new({
            value => 15,
        }),
    }), "inner message");

    is(Test1::Enumeration::VALUE1(), 2, 'enum value');
    is(Test1::Enumeration::VALUE2(), 7, 'enum value');
    is(Test1::Enumeration::VALUE3(), 12, 'enum value');

    is(Test1::OuterMessage::Enum::VALUE1(), 3, 'inner enum value');
    is(Test1::OuterMessage::Enum::VALUE2(), 2, 'inner enum value');
    is(Test1::OuterMessage::Enum::VALUE3(), 1, 'inner enum value');
}

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto/mapping');

    $d->load_file("test1.proto");
    $d->map_message("test1.Message1", "Test2::FirstMessage");

    throws_ok(
        sub { $d->map_message("test1.Message1", "Test2::FirstMessageDup") },
        qr/Message 'test1\.Message1' has already been mapped/,
        "duplicate message mapping",
    );

    throws_ok(
        sub { $d->map_message("test1.Message2", "Test2::FirstMessage") },
        qr/Package 'Test2::FirstMessage' has already been used in a mapping/,
        "duplicate package mapping",
    );

    throws_ok(
        sub { $d->map_enum('test1.Enum', 'Test2::FirstMessage') },
        qr/Package 'Test2::FirstMessage' has already been used in a mapping/,
        "enum/message clash",
    );

    $d->map_enum('test1.Enum', 'Test2::Enumeration');

    throws_ok(
        sub { $d->map_enum('test1.Enum', 'Test2::OtherEnum') },
        qr/test1.Enum' has already been mapped/,
        "duplicate enum mapping",
    );
}

done_testing();

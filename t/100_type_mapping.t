use t::lib::Test;
no warnings 'redefine';

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto/mapping');

    $d->load_file("test1.proto");
    $d->map_message("test1.Message1", "Test1::FirstMessage");
    $d->map_message("test1.Message2", "Test1::AnotherMessage");
    $d->map_message("test1.Message3", "Test1::CompositeMessage");
    $d->map_message("test1.Message4.Message5", "Test1::InnerMessage");
    $d->map_message("test1.Message4", "Test1::OuterMessage");
    $d->resolve_references();

    eq_or_diff(Test1::FirstMessage->decode_to_perl("\x08\x01"), {
        test1_message1 => 1,
    }, "simple message 2");
    eq_or_diff(Test1::AnotherMessage->decode_to_perl("\x08\x01"), {
        test1_message2 => 1,
    }, "simple message 2");
    eq_or_diff(Test1::CompositeMessage->decode_to_perl("\x0a\x02\x08\x01\x12\x02\x08\x01"), {
        test1_message3_message1 => {
            test1_message1 => 1,
        },
        test1_message3_message2 => {
            test1_message2 => 1,
        },
    }, "composite message");
    eq_or_diff(Test1::OuterMessage->decode_to_perl("\x12\x02\x08\x0f"), {
        inner => {
            value => 15,
        },
    }, "inner message");
}

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto/mapping');

    $d->load_file("test1.proto");
    $d->map_message("test1.Message1", "Test1::FirstMessage");

    throws_ok(
        sub { $d->map_message("test1.Message1", "Test1::FirstMessageDup") },
        qr/Message 'test1\.Message1' has already been mapped/,
        "duplicate message mapping",
    );

    throws_ok(
        sub { $d->map_message("test1.Message2", "Test1::FirstMessage") },
        qr/Package 'Test1::FirstMessage' has already been used in a message mapping/,
        "duplicate package mapping",
    );
}

done_testing();

use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("extensions.proto");
$d->map_message("test.Message1", "BaseMessage");
$d->map_message("test.Message2", "ExtensionMessage");
$d->resolve_references();

{
    my $encoded = "\x08\x02\xa0\x06\x03\xb0\x06\x04";
    my $decoded = BaseMessage->new({
        value               => 2,
        '[test.value]'      => 4,
        '[test.extension1]' => 3,
    });

    eq_or_diff(BaseMessage->decode_to_perl($encoded), $decoded);
    eq_or_diff(BaseMessage->encode_from_perl($decoded), $encoded);

    is(BaseMessage::VALUE_FIELD_NUMBER(), 1);
    is(ExtensionMessage::VALUE_FIELD_NUMBER(), 1);
    is(BaseMessage::TEST_EXTENSION1_FIELD_NUMBER(), 100);
    is(BaseMessage::TEST_VALUE_FIELD_NUMBER(), 102);
    is(BaseMessage::TEST_MESSAGE2_EXTENSION2_FIELD_NUMBER(), 101);

    is(BaseMessage::TEST_EXTENSION1_KEY(), '[test.extension1]');
    is(BaseMessage::TEST_VALUE_KEY(), '[test.value]');
    is(BaseMessage::TEST_MESSAGE2_EXTENSION2_KEY(), '[test.Message2.extension2]');
}

{
    my $encoded = "\x08\x02\xa0\x06\x04\xaa\x06\x02\x08\x05";
    my $decoded = BaseMessage->new({
        value                        => 2,
        '[test.extension1]'          => 4,
        '[test.Message2.extension2]' => ExtensionMessage->new({
            value   => 5,
        }),
    });

    eq_or_diff(BaseMessage->decode_to_perl($encoded), $decoded);
    eq_or_diff(BaseMessage->encode_from_perl($decoded), $encoded);
}

done_testing();

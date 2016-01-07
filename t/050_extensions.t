use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("extensions.proto");
$d->map_message("test.Message1", "BaseMessage");
$d->map_message("test.Message2", "ExtensionMessage");
$d->resolve_references();

{
    my $encoded = "\x08\x02\xa0\x06\x03";
    my $decoded = {
        value               => 2,
        '[test.extension1]' => 3,
    };

    eq_or_diff(BaseMessage->decode_to_perl($encoded), $decoded);
    eq_or_diff(BaseMessage->encode_from_perl($decoded), $encoded);
}

{
    my $encoded = "\x08\x02\xa0\x06\x04\xaa\x06\x02\x08\x05";
    my $decoded = {
        value                        => 2,
        '[test.extension1]'          => 4,
        '[test.Message2.extension2]' => {
            value   => 5,
        },
    };

    eq_or_diff(BaseMessage->decode_to_perl($encoded), $decoded);
    eq_or_diff(BaseMessage->encode_from_perl($decoded), $encoded);
}

done_testing();

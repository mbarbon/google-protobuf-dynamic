use t::lib::Test;

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto/mapping');

    $d->load_file("test1.proto");
    $d->load_file("test2.proto");
    $d->map(
        { message => 'test2.Message3', to => 'Test2::Composite' },
        { package => 'test1', prefix => 'Test1' },
        { package => 'test2', prefix => 'Test2' },
    );

    eq_or_diff(Test1::Message3->decode_to_perl("\x0a\x02\x08\x01\x12\x02\x08\x01"), {
        test1_message3_message1 => {
            test1_message1 => 1,
        },
        test1_message3_message2 => {
            test1_message2 => 1,
        },
    }, "composite message - multiple packages");

    eq_or_diff(Test2::Composite->decode_to_perl("\x0a\x02\x08\x01\x12\x02\x08\x01"), {
        test1_message3_message1 => {
            test2_message1 => 1,
        },
        test1_message3_message2 => {
            test1_message2 => 1,
        },
    }, "composite message - multiple packages");
}

done_testing();

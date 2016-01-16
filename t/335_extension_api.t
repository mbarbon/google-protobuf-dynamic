use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("extensions.proto");
$d->map({ package => 'test', prefix => 'Test' });

my $message = Test::Message1->new({
    value               => 2,
    '[test.value]'      => 4,
    '[test.extension1]' => 3,
    '[test.Message2.extension2]' => Test::Message2->new({
        value   => 5,
    }),
});

is($message->get_value, 2);
is($message->get_test_value, 4);
is($message->get_test_extension1, 3);
is($message->get_test_message2_extension2->get_value, 5);

done_testing();

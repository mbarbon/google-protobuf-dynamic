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

throws_ok(
    sub { $message->get_extension('value') },
    qr/Unknown extension field 'value' for message 'test.Message1'/,
    "not an extension",
);

is($message->get_extension('test.value'), 4);
is($message->get_extension('test.Message2.extension2')->get_value, 5);
is($message->get_extension(Test::Message1::TEST_VALUE_KEY()), 4);
is($message->get_extension(Test::Message1::TEST_MESSAGE2_EXTENSION2_KEY())->get_value, 5);

throws_ok(
    sub { $message->get_extension_item('test.value', 1) },
    qr/Extension field 'test.value' is a non-repeated field/,
    "scalar/repeated field mismatch",
);

done_testing();

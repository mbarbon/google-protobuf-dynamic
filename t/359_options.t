use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto/options');
$d->load_file("builtin_options.proto");
$d->map({ package => 'test', prefix => 'Test' });

{
    my $message = Test::MessageWithOptions->message_descriptor;
    my $options = $message->options;

    is($options->deprecated, 1);
    is($options->custom_option_by_name('not.there'), undef);
}

{
    my $message = Test::MessageWithoutOptions->message_descriptor;
    my $options = $message->options;

    is($options->deprecated, '');
    is($options->custom_option_by_name('not.there'), undef);
}

{
    my $file = Test::MessageWithOptions->message_descriptor->file;
    my $options = $file->options;

    is($options->deprecated, 1);
    is($options->java_package, 'org.test');
    is($options->custom_option_by_name('not.there'), undef);

    throws_ok(sub { $options->nope },
              qr/Unknown option 'nope'/);
}

done_testing();

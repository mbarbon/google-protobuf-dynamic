use t::lib::Test;

use Google::ProtocolBuffers::Dynamic;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto/options');
$d->load_file("use.proto");
$d->map({ package => 'test', prefix => 'Test' });

{
    my $message = Test::MessageWithOptions->message_descriptor();
    my $options = $message->options;

    is($options->custom_option_by_name('options.msg_string'), 'Custom message option');
    is($options->custom_option_by_name('options.msg_integer'), -2);
    is($options->custom_option_by_name('options.msg_nope'), undef);

    is($options->custom_option_by_number(51234), 'Custom message option');
    is($options->custom_option_by_number(52234), undef);
}

{
    my $message = Test::MessageWithOptions->message_descriptor();
    my $int32_f = $message->find_field_by_name('int32_f');
    my $options = $int32_f->options;

    is($options->custom_option_by_name('options.fld_string'), 'Custom field option');
    is($options->custom_option_by_name('options.fld_integer'), 0);
}

done_testing();

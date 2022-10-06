use t::lib::Test;

my ($dummy_file, $dummy_line);

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto/mapping');

    $d->load_file("test1.proto");
    dummy($d);

    is(Test1::Message->message_descriptor->full_name, 'test1.Message2');
}

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto/mapping');

    $d->load_file("test1.proto");
    throws_ok(sub {
        $d->map_message("test1.Message2", "Test1::Message");
    }, qr/Package 'Test1::Message' has already been mapped from $dummy_file line $dummy_line/, 'duplicate mapping from different GPD object');

    is(Test1::Message->message_descriptor->full_name, 'test1.Message2',
       'old mapping is still in effect');
}

done_testing();

sub dummy {
    $_[0]->map_message("test1.Message2", "Test1::Message"); BEGIN { ($dummy_file, $dummy_line) = (__FILE__, __LINE__) }
}

use t::lib::Test;

my ($dummy_file, $dummy_line);

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto/mapping');

    $d->load_file("test1.proto");
    dummy($d);

    is(Test1::Message->message_descriptor->full_name, 'test1.Message2');
}

{
    my ($this_file, $this_line);
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto/mapping');

    $d->load_file("test1.proto");
    throws_ok(sub {
        $d->map_message("test1.Message2", "Test1::Message"); BEGIN { ($this_file, $this_line) = (__FILE__, __LINE__) }
    }, qr/Package 'Test1::Message' is being remapped from $this_file line $this_line but has already been mapped from $dummy_file line $dummy_line/, 'duplicate mapping from different GPD object');

    is(Test1::Message->message_descriptor->full_name, 'test1.Message2',
       'old mapping is still in effect');
}

my ($mapping1_file, $mapping1_line, $mapping2_file, $mapping2_line);

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto/mapping');

    $d->load_file("test1.proto");
    $d->map({ package => 'test1', prefix => 'Test1_1' }); BEGIN { ($mapping1_file, $mapping1_line) = (__FILE__, __LINE__) }
}

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto/mapping');

    $d->load_file("test1.proto");
    throws_ok(sub {
        $d->map({ package => 'test1', prefix => 'Test1_1' }); BEGIN { ($mapping2_file, $mapping2_line) = (__FILE__, __LINE__) }
    }, qr/Package 'Test1_1::Message1' is being remapped from $mapping2_file line $mapping2_line but has already been mapped from $mapping1_file line $mapping1_line/, 'better file/line numbers from ->map');
}

done_testing();

sub dummy {
    $_[0]->map_message("test1.Message2", "Test1::Message"); BEGIN { ($dummy_file, $dummy_line) = (__FILE__, __LINE__) }
}

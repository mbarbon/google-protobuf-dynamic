use t::lib::Test;
no warnings 'redefine';

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto/mapping');

    $d->load_file("test3.proto");
    $d->map_message("test1.Message1", "Test1::FirstMessage");
    $d->map_message("test2.Message1", "Test2::FirstMessage");
    $d->map_service("test3.Service1", "Test3::AService", { client_services => 'noop' });
    $d->resolve_references();

    is(Test3::AService->service_descriptor->full_name, 'test3.Service1', 'explicit service mapping');
}

done_testing();

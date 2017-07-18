use t::lib::Test;

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto/mapping');

    $d->load_file("test1.proto");
    $d->load_file("test2.proto");
    $d->load_file("test3.proto");
    $d->map(
        { enum    => 'test1.Enum', to => 'Test1::Enumeration' },
        { message => 'test2.Message3', to => 'Test2::Composite' },
        { service => 'test3.Service1', to => 'Test3::AService', options => { client_services => 'noop' } },
        { package => 'test1', prefix => 'Test1' },
        { package => 'test2', prefix => 'Test2' },
        { package => 'test3', prefix => 'Test3', options => { client_services => 'noop' } },
    );

    eq_or_diff(Test1::Message3->decode("\x0a\x02\x08\x01\x12\x02\x08\x01"), Test1::Message3->new({
        test1_message3_message1 => Test1::Message1->new({
            test1_message1 => 1,
        }),
        test1_message3_message2 => Test1::Message2->new({
            test1_message2 => 1,
        }),
    }), "composite message - multiple packages");

    eq_or_diff(Test2::Composite->decode("\x0a\x02\x08\x01\x12\x02\x08\x01"), Test2::Composite->new({
        test1_message3_message1 => Test2::Message1->new({
            test2_message1 => 1,
        }),
        test1_message3_message2 => Test1::Message2->new({
            test1_message2 => 1,
        }),
    }), "composite message - multiple packages");

    is(Test1::Enumeration::VALUE1(), 2, 'enum value');
    is(Test1::Enumeration::VALUE2(), 7, 'enum value');
    is(Test1::Enumeration::VALUE3(), 12, 'enum value');

    is(Test3::AService->service_descriptor->full_name, 'test3.Service1', 'explicit service mapping');
    is(Test3::Service2->service_descriptor->full_name, 'test3.Service2', 'package service mapping');
}

done_testing();

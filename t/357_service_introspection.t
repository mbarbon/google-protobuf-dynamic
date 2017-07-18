use t::lib::Test 'proto3';

use Google::ProtocolBuffers::Dynamic qw(:descriptor :values);

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("grpc/greeter.proto");
    $d->map({ package => 'helloworld', prefix => 'TestNoop', options => {
        client_services => 'noop'
    } });

    check_mapping('TestNoop::Greeter', 'noop');
}

if (eval { require Grpc::XS }) {
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("grpc/greeter.proto");
    $d->map({ package => 'helloworld', prefix => 'TestGrpcXS', options => {
        client_services => 'noop'
    } });

    check_mapping('TestGrpcXS::Greeter', 'grpc_xs');
}

done_testing();

sub check_mapping {
    my ($package, $type) = @_;

    my $service = $package->service_descriptor;
    my $methods = $service->methods;

    note("Testing mapping for '$type'");
    is($service->full_name, 'helloworld.Greeter');
    is(scalar @$methods, 4);

    my @methods = sort { $a->name cmp $b->name } @$methods;

    eq_or_diff(method_attrs($methods[0]), {
        client_streaming    => 1,
        server_streaming    => '',
        name                => 'JoinedHello',
        service             => 'helloworld.Greeter',
        full_name           => 'helloworld.Greeter.JoinedHello',
        input               => 'helloworld.HelloRequest',
        output              => 'helloworld.HelloReply',
    });

    eq_or_diff(method_attrs($methods[1]), {
        client_streaming    => '',
        server_streaming    => '',
        name                => 'SayHello',
        service             => 'helloworld.Greeter',
        full_name           => 'helloworld.Greeter.SayHello',
        input               => 'helloworld.HelloRequest',
        output              => 'helloworld.HelloReply',
    });

    eq_or_diff(method_attrs($methods[2]), {
        client_streaming    => '',
        server_streaming    => 1,
        name                => 'SplitHello',
        service             => 'helloworld.Greeter',
        full_name           => 'helloworld.Greeter.SplitHello',
        input               => 'helloworld.HelloRequest',
        output              => 'helloworld.HelloReply',
    });

    eq_or_diff(method_attrs($methods[3]), {
        client_streaming    => 1,
        server_streaming    => 1,
        name                => 'WavingHello',
        service             => 'helloworld.Greeter',
        full_name           => 'helloworld.Greeter.WavingHello',
        input               => 'helloworld.HelloRequest',
        output              => 'helloworld.HelloReply',
    });
}

sub method_attrs {
    return {
        name                => $_[0]->name,
        full_name           => $_[0]->full_name,
        service             => $_[0]->containing_service->full_name,
        input               => $_[0]->input_type->full_name,
        output              => $_[0]->output_type->full_name,
        client_streaming    => $_[0]->client_streaming,
        server_streaming    => $_[0]->server_streaming,
    };
}

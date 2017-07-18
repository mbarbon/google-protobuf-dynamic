use t::lib::GrpcClient;

spawn_server('t/grpc/sayhello_stream_client.pl');

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("grpc/greeter.proto");
$d->map(
    { package => 'helloworld', prefix => 'Helloworld', options => { client_services => 'grpc_xs' } },
);

my $credentials = Grpc::XS::ChannelCredentials::createInsecure;
my $greeter = Helloworld::Greeter->new(
    server_address,
    credentials => $credentials,
);
my $call = $greeter->JoinedHello();
for my $char (split //, 'grpc-perl') {
    $call->write(Helloworld::HelloRequest->new({
        name => $char,
    }));
}
my $response = $call->wait;

ok($response, 'got a response');
is($response && $response->get_message, 'Hello, grpc-perl');

done_testing();

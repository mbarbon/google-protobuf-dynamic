use t::lib::GrpcClient;

spawn_server('t/grpc/sayhello_stream_bidi.pl');

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
my $call = $greeter->WavingHello();
my @chars = split //, 'grpc-perl';
my @responses;

for (;;) {
    my $response = $call->read;
    push @responses, $response if $response;
    last if !$response && !@chars;
    if (@chars) {
        $call->write(Helloworld::HelloRequest->new({
            name => shift @chars,
        }));
        $call->writesDone if !@chars;
    }
}

is(scalar @responses, 10);
is(join('', map $_->get_message, @responses), 'Hello, grpc-perl');

done_testing();

#!/usr/bin/env perl

use strict;
use warnings;

use Google::ProtocolBuffers::Dynamic;
use Grpc::XS;
use Grpc::Constants;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("grpc/greeter.proto");
$d->map(
    { package => 'helloworld', prefix => 'Helloworld' },
);

my $server = new Grpc::XS::Server();
my $port = $server->addHttp2Port('127.0.0.1:0');
my $channel = new Grpc::XS::Channel('localhost:'.$port);

$server->start();

print "port: $port\n";
flush STDOUT;

my $client_event = $server->requestCall();
my $server_call = $client_event->{call};

my $request_event = $server_call->startBatch(
    Grpc::Constants::GRPC_OP_RECV_MESSAGE() => 1,
);
my $request = Helloworld::HelloRequest->decode($request_event->{message});

$server_call->startBatch(
    Grpc::Constants::GRPC_OP_SEND_INITIAL_METADATA() => {},
);
for my $part ("Hello, ", split //, $request->get_name) {
    $server_call->startBatch(
        Grpc::Constants::GRPC_OP_SEND_MESSAGE() => {
            message => Helloworld::HelloReply->encode({ message => $part }),
        },
    );
}
$server_call->startBatch(
    Grpc::Constants::GRPC_OP_SEND_STATUS_FROM_SERVER() => {
        'metadata' => {},
        'code' => Grpc::Constants::GRPC_STATUS_OK(),
        'details' => 'Everything good',
    },
    Grpc::Constants::GRPC_OP_RECV_CLOSE_ON_SERVER() => 1,
);

exit 0;

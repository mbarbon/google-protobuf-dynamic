package t::lib::GrpcClient;

use strict;
use warnings;
use parent 'Test::Builder::Module';

use t::lib::Test;

use IPC::Open2;

our @EXPORT = (
    @t::lib::Test::EXPORT,
    qw(
          spawn_server
          server_address
    ),
);

sub import {
    unshift @INC, 't/lib';

    eval {
        require Grpc::XS;

        1;
    } or do {
        Test::More->import(skip_all => 'Grpc::XS not available');
    };

    strict->import;
    warnings->import;

    # for libgrpc
    delete @ENV{grep /https?_proxy/i, keys %ENV};

    goto &t::lib::Test::import;
}

my ($server_pid, $server_port);

sub spawn_server {
    my ($server_script) = @_;
    $server_pid = open2(my $child_in, my $child_out, $^X, '-Mblib', $server_script);
    close $child_out;
    my $port_line = readline($child_in);
    die "Invalid format '$port_line'" unless $port_line =~ /port: (\d+)$/;
    my $port = $1;

    $server_port = $port;
}

sub server_address {
    return "127.0.0.1:$server_port";
}

END {
    kill 3, $server_pid if $server_pid;
}

1;

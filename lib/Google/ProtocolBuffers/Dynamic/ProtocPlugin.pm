package Google::ProtocolBuffers::Dynamic::ProtocPlugin;

use strict;
use warnings;
use Google::ProtocolBuffers::Dynamic::ProtocInterface;

sub import {
    my ($class, %args) = @_;
    my $generator = $args{run};
    eval "require $generator; 1"
        or die "Error loding plugin '$generator': $@";

    binmode STDIN;
    binmode STDOUT;

    my $input = do {
        local $/;
        readline STDIN;
    };
    my $codegen_request = Google::ProtocolBuffers::Dynamic::ProtocInterface::CodeGeneratorRequest->decode($input);
    my $codegen_response;

    eval {
        my $code = $generator->generate_codegen_request($codegen_request);

        $codegen_response = Google::ProtocolBuffers::Dynamic::ProtocInterface::CodeGeneratorResponse->encode($code);

        1;
    } or do {
        my $error = $@ || "Zombie error";

        $codegen_response = Google::ProtocolBuffers::Dynamic::ProtocInterface::CodeGeneratorResponse->encode({
            error => $error,
        });
    };

    print STDOUT $codegen_response;
}

1;

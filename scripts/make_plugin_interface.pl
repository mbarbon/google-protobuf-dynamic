#!/usr/bin/perl

use strict;
use warnings;
use Google::ProtocolBuffers::Dynamic::MakeModule;

Google::ProtocolBuffers::Dynamic::MakeModule->generate_to(
    package             => 'Google::ProtocolBuffers::Dynamic::ProtocInterface',
    mappings            => [
        {
            package => 'google.protobuf',
            prefix  => 'Google::ProtocolBuffers::Dynamic::ProtocInterface',
            options => { generic_extension_methods => 0 },
        },
        {
            package => 'google.protobuf.compiler',
            prefix  => 'Google::ProtocolBuffers::Dynamic::ProtocInterface',
            options => { generic_extension_methods => 0 },
        },
    ],
    descriptor_files    => [qw(scripts/protoc/descriptor.pb scripts/protoc/plugin.pb)],
    file                => 'lib/Google/ProtocolBuffers/Dynamic/ProtocInterface.pm',
);

exit 0;

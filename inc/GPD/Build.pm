package GPD::Build;

use strict;
use warnings;
use parent 'Module::Build::WithXSpp';

use Alien::ProtoBuf;
use Alien::uPB::Core;
use Getopt::Long qw( :config pass_through );

# yes, doing this in a module is ugly; OTOH it's a private module
GetOptions(
    'g'         => \my $DEBUG,
);

sub new {
    my $class = shift;

    if (!Alien::ProtoBuf->atleast_version('3.0.0')) {
        warn sprintf "Protocol Buffers library version 3.0.0 required, %s installed\n", Alien::ProtoBuf->version;
        exit 0;
    }

    my $debug_flag = $DEBUG ? ' -g' : '';
    my $self = $class->SUPER::new(
        @_,
        extra_typemap_modules => {
            'ExtUtils::Typemaps::STL::String' => '0',
        },
        extra_linker_flags => [Alien::uPB::Core->libs, Alien::ProtoBuf->libs],
        extra_compiler_flags => [$debug_flag, Alien::uPB::Core->cflags, Alien::ProtoBuf->cflags, Alien::ProtoBuf->cxxflags, "-DPERL_NO_GET_CONTEXT"],
        script_files => [qw(
            scripts/protoc-gen-perl-gpd
            scripts/protoc-gen-perl-gpd-add-pragma
        )],
    );

    return $self;
}

1;

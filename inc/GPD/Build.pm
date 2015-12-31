package GPD::Build;

use strict;
use warnings;
use parent 'Module::Build::WithXSpp';

use Alien::ProtoBuf;
use Alien::uPB;

sub new {
    my $class = shift;
    my $self = $class->SUPER::new(
        @_,
        extra_typemap_modules => {
            'ExtUtils::Typemaps::STL::String' => '0',
        },
        extra_linker_flags => [Alien::uPB->libs, Alien::ProtoBuf->libs],
        extra_compiler_flags => [Alien::uPB->cflags, Alien::ProtoBuf->cflags],
    );

    return $self;
}

1;

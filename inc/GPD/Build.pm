package GPD::Build;

use strict;
use warnings;
use parent 'Module::Build::WithXSpp';

sub new {
    my $class = shift;
    my $self = $class->SUPER::new(
        @_,
        extra_typemap_modules => {
            'ExtUtils::Typemaps::STL::String' => '0',
        },
        # XXX fix this
        extra_linker_flags => [qw(-L/home/mattia/dev/upb/lib -lupb.bindings.googlepb -lupb.pb -lupb -lprotobuf)],
        extra_compiler_flags => [qw(-I/home/mattia/dev/upb -g)],
    );

    return $self;
}

1;

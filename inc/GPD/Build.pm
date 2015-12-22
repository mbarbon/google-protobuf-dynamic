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
        extra_linker_flags => [qw(-lprotobuf)],
    );

    return $self;
}

1;

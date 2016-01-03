package Google::ProtocolBuffers::Dynamic;
# ABSTRACT: fast and complete protocol buffer implementation

use strict;
use warnings;
use XSLoader;

# VERSION

XSLoader::load(__PACKAGE__);

sub map {
    my ($self, @mappings) = @_;

    for my $mapping (@mappings) {
        if (exists $mapping->{package}) {
            $self->map_package($mapping->{package}, $mapping->{prefix});
        } elsif (exists $mapping->{message}) {
            $self->map_message($mapping->{message}, $mapping->{to});
        } else {
            require Data::Dumper;

            die "Unrecognized mapping ", Data::Dumper::Dumper($mapping);
        }
    }

    $self->resolve_references;
}

1;

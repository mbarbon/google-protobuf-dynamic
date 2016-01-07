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
            $self->map_package($mapping->{package}, $mapping->{prefix}, $mapping->{options});
        } elsif (exists $mapping->{message}) {
            $self->map_message($mapping->{message}, $mapping->{to}, $mapping->{options});
        } else {
            require Data::Dumper;

            die "Unrecognized mapping ", Data::Dumper::Dumper($mapping);
        }
    }

    $self->resolve_references;
}

1;

__END__

=head1 SYNOPSIS

    my $dynamic = Google::ProtocolBuffers::Dynamic->new;
    $dynamic->load_string("person.proto", <<'EOT');
    package humans;

    message Person {
      required string name = 1;
      required int32 id = 2;
      optional string email = 3;
    }
    EOT

    $dynamic->map({ package => 'humans', prefix => 'Humans' });

    # { id => 31, name => 'foo', email => '' }
    my $person = Humans::Person->decode_to_perl("\x0a\x03foo\x10\x1f");

    my $bytes = Humans::Person->encode_from_perl($person);

=cut

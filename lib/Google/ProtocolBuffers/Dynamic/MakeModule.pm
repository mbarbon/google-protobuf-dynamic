package Google::ProtocolBuffers::Dynamic::MakeModule;

use strict;
use warnings;
use autodie qw(open close);

use Data::Dumper qw();
use MIME::Base64 qw();
use Google::ProtocolBuffers::Dynamic;

my $TEMPLATE = <<'EOT';
package ${package};

use strict;
use warnings;
use MIME::Base64 qw();
use Google::ProtocolBuffers::Dynamic;

my $gpd = Google::ProtocolBuffers::Dynamic->new;

${load_blobs}

$gpd->map(
${mappings});

undef $gpd;

1;
EOT

sub generate_to {
    my ($class, %args) = @_;

    open my $fh, '>', $args{file};
    print $fh $class->generate(%args);
    close $fh;
}

sub generate {
    my ($class, %args) = @_;
    my $mappings = do {
        package Data::Dumper;

        our ($Terse, $Indent, $Purity, $Deepcopy, $Pad, $Sortkeys);
        local $Terse = 1;
        local $Indent = 2;
        local $Purity = 0;
        local $Deepcopy = 1;
        local $Pad = '    ';
        local $Sortkeys = 1;
        join "", map {
            (my $dump = Dumper($_)) =~ s{$}{,}; $dump
        } @{$args{mappings}};
    };
    my @descriptors = @{$args{descriptors} || []};
    my $load = '';

    for my $descriptor_file (@{$args{descriptor_files} || []}) {
        push @descriptors, do {
            local $/;
            open my $fh, '<', $descriptor_file;
            binmode $fh;
            readline $fh;
        };
    }

    for my $descriptor (@descriptors) {
        $load .= sprintf <<'EOL', MIME::Base64::encode_base64($descriptor);
$gpd->load_serialized_string(MIME::Base64::decode_base64(<<'EOD'));
%s
EOD
EOL
    }

    my %replacement = (
        package     => $args{package},
        load_blobs  => $load,
        mappings    => $mappings,
    );
    (my $copy = $TEMPLATE) =~ s{\$\{(\w+)\}}{$replacement{$1}}ge;

    return $copy;
}

sub _to_perl_package {
    my ($mapping, $value) = @_;

    die;
}

my %boolean_options = map +($_ => [$_, 1], "no_$_" => [$_, 0]), qw(
    use_bigints
    check_required_fields
    explicit_defaults
    encode_defaults
    check_enum_values
    generic_extension_methods
);

sub _to_option {
    my ($options, $key, $value) = @_;

    return 0 unless my $boolean = $boolean_options{$key};
    $options->{$boolean->[0]} = $boolean->[1];

    return 1;
}

sub _perlify_package {
    return join '::', map ucfirst, split /\./, $_[0];
}

sub error {
    return { error => $_[0] };
}

sub generate_codegen_request {
    my ($class, $request) = @_;
    my %files = map +($_ => 1), @{$request->get_file_to_generate_list};
    my $descriptors = Google::ProtocolBuffers::Dynamic::ProtocInterface::FileDescriptorSet->new;
    my $response = Google::ProtocolBuffers::Dynamic::ProtocInterface::CodeGeneratorResponse->new;
    my ($package, @mappings, %pb_packages, %global_options);

    for my $file (@{$request->get_proto_file_list}) {
        next unless $files{$file->get_name};
        $pb_packages{$file->get_package} = 1;
        $descriptors->add_file($file);
    }

    # package=<package>,map_package=<package>,prefix=<prefix>,option,options
    my $mapping;
    for my $parameter (split /,/, $request->get_parameter) {
        my ($key, $value) = split /=/, $parameter;

        if ($key eq 'package') {
            $package = _perlify_package($value);
        } elsif ($key eq 'map_package') {
            push @mappings, $mapping = {};
            $mapping->{package} = $value;
        } elsif ($key eq 'prefix') {
            $mapping->{prefix} = _perlify_package($value);
        } elsif (!_to_option(($mapping ? $mapping->{options} : \%global_options), $key, $value)) {
            return error("Unrecognized option key '$key'");
        }
    }

    return error("Use '--perl-gpd_out=package=Foo.Bar:outdir' to specify output package name") unless $package;
    if (!@mappings) {
        for my $pb_package (sort keys %pb_packages) {
            push @mappings, {
                package => $pb_package,
                prefix  => $pb_package ? $package . '::' ._perlify_package($pb_package) :
                                         $package,
            };
        }
    }

    if (%global_options) {
        $_->{options} = { %global_options, %{$_->{options} // {}} }
            for @mappings;
    }

    (my $output_file = $package) =~ s{::}{/}g;
    my $code = $class->generate(
        mappings    => \@mappings,
        descriptors => [$descriptors->encode_from_perl],
        package     => $package,
    );

    $response->add_file(
        Google::ProtocolBuffers::Dynamic::ProtocInterface::CodeGeneratorResponse::File->new({
            name    => "$output_file.pm",
            content => $code,
        }),
    );

    return $response;
}

1;

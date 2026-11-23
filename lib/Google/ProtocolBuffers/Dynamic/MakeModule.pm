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
# @@protoc_insertion_point(after_pragmas)
use MIME::Base64 qw();
use Google::ProtocolBuffers::Dynamic;

my $gpd = Google::ProtocolBuffers::Dynamic->new;

${load_blobs}

# @@protoc_insertion_point(after_loading)

$gpd->map(
${mappings});

# @@protoc_insertion_point(after_mapping)

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
        package # hide from PAUSE indexer
            Data::Dumper;

        our ($Terse, $Indent, $Purity, $Deepcopy, $Pad, $Sortkeys, $Quotekeys);
        local $Terse = 1;
        local $Indent = 2;
        local $Purity = 0;
        local $Deepcopy = 1;
        local $Pad = '    ';
        local $Sortkeys = 1;
        local $Quotekeys = 0;
        join "", map {
            my $dump = Dumper($_);
            $dump =~ s{$}{,};
            # the '+' is to make sure Perl::Critic does not consider
            # this an anonymous subroutine
            $dump =~ s[^(\s+)\s(\{)][$1+$2]mg;
            $dump
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
%sEOD
EOL
    }
    chomp $load;

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
    decode_blessed
    implicit_maps
    use_bigints
    check_required_fields
    explicit_defaults
    encode_defaults
    encode_defaults_proto3
    check_enum_values
    fail_ref_coercion
    ignore_undef_fields
    generic_extension_methods
);

my %string_options = map { $_ => 1 } qw(
    accessor_style
    client_services
    boolean_values
);

sub _to_option {
    my ($options, $key, $value) = @_;

    if (exists $string_options{$key}) {
        $options->{$key} = $value;
        return 1;
    }
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
        $file->clear_source_code_info;
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
        } elsif ($key eq 'map_message') {
            push @mappings, $mapping = {};
            $mapping->{message} = $value;
        } elsif ($key eq 'pb_prefix') {
            push @mappings, $mapping = {};
            $mapping->{pb_prefix} = $value;
        } elsif ($key eq 'prefix' || $key eq 'to') {
            $mapping->{$key} = _perlify_package($value);
        } elsif (!_to_option(($mapping ? $mapping->{options} //= {} : \%global_options), $key, $value)) {
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
        descriptors => [$descriptors->encode],
        package     => $package,
    );

    $response->add_file(
        Google::ProtocolBuffers::Dynamic::ProtocInterface::CodeGeneratorResponse::File->new({
            name    => "$output_file.pm",
            content => $code,
        }),
    );
    $response->set_supported_features(Google::ProtocolBuffers::Dynamic::ProtocInterface::CodeGeneratorResponse::Feature::FEATURE_PROTO3_OPTIONAL);

    return $response;
}

1;

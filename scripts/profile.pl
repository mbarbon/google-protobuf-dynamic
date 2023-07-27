use strict;
use warnings;

use Benchmark qw(cmpthese);
use Sereal;
use JSON::XS ();
use JSON ();
use Google::ProtocolBuffers::Dynamic;
use Getopt::Long;

{
    my $d = Google::ProtocolBuffers::Dynamic->new('scripts/profile');

    $d->load_file("map.proto");
    $d->load_file("person.proto");
    $d->load_file("transform.proto");
    $d->map({
        package => 'profile',
        prefix  => 'GPD::Profile',
        options => { decode_blessed => 0 },
    });
    GPD::Profile::Any->set_decoder_options({
        fieldtable  => 1,
        transform   => $Google::ProtocolBuffers::Dynamic::Fieldtable::profile_decoder_transform,
    });
}
my $sereal_encoder = Sereal::Encoder->new;
my $sereal_decoder = Sereal::Decoder->new;
my $json_decoder = JSON::XS->new;

exit main();

sub main {
    my %suites = (
        decode_maps         => \&profile_decode_maps,
        decode_objects      => \&profile_decode_objects,
        decode_transform    => \&profile_decode_transform,
        encode_objects      => \&profile_encode_objects,
        encode_maps         => \&profile_encode_maps,
    );

    my %callgrind_benchmarks = (
        bbpb    => 'protobuf_bbpb',
        upb     => 'protobuf_upb',
        json    => 'json',
        sereal  => 'sereal',
    );

    my $error;
    my $arg_ok = GetOptions(
        'suite=s'     => \(my $suite),
        'callgrind=s' => \(my $callgrind_implementation),
        'help'        => \(my $help),
    );
    if ($callgrind_implementation && !exists $callgrind_benchmarks{$callgrind_implementation}) {
        $error = "Unknown value '$callgrind_implementation' for implementation";
    }
    if ($suite && !exists $suites{$suite}) {
        $error = "Unknown value '$suite' for suite";
    }
    if ($callgrind_implementation && !$suite) {
        $error = "Can't profile with callgrind without selecting a suite";
    }

    if (!$arg_ok || $help || $error) {
        help(!$arg_ok || !!$error, $error, \%suites);
    }

    my $with_callgrind = 0;
    my $repeat_count = $callgrind_implementation ? 2000 : -3;
    my $which_benchmarks = $callgrind_implementation ?
        [$callgrind_benchmarks{$callgrind_implementation}] :
        [qw(protobuf_bbpb protobuf_upb json sereal)];

    setup();
    for my $run_suite ($suite ? $suite : sort keys %suites) {
        print "\nRunning $run_suite\n\n";
        start_callgrind() if $callgrind_implementation;
        $suites{$run_suite}->($repeat_count, $which_benchmarks);
        stop_callgrind() if $callgrind_implementation;
    }
}

sub help {
    my ($is_error, $error_msg, $suites) = @_;

    my $suites_list = join '', map "        $_\n", keys %$suites;

    print $error_msg, "\n\n" if $error_msg;
    print <<EOT;
Usage: benchmark.pl [--help] [--callgrind=<implementation>] [--suite=<benchmark suite>]

    --help              This message

    --callgrind=[bbpb|upb|json|sereal]
        Only run the given implementation, enable callgrind while running
        the benchmark (the script needs to be run by using
        valgrind --tool=callgrind --instr-atstart=no ...)

    --suite=<suite>     Only run the given banchmark suite, which can be one of:
$suites_list
EOT
    exit $is_error;
}

my %benchmarks;

sub random_chars {
    my ($min, $max) = @_;
    my $num = rand($max - $min) + $min;

    return join '', map chr(rand(26) + 97), 1 .. $num;
}

sub setup {
    setup_encode_objects();
    setup_decode_objects();
    setup_encode_maps();
    setup_decode_maps();
    setup_decode_transform();
}

sub setup_decode_maps {
    my $maps = $benchmarks{encode}{maps};

    $benchmarks{decode}{maps}{protobuf} = GPD::Profile::Maps->encode($maps->{protobuf});
    $benchmarks{decode}{maps}{sereal} = $sereal_encoder->encode($maps->{plain});
    $benchmarks{decode}{maps}{json} = $json_decoder->encode($maps->{plain});
}

sub profile_decode_maps {
    my ($repeat_count, $which_benchmarks) = @_;
    my $data = $benchmarks{decode}{maps};

    cmpthese($repeat_count, filter_benchmarks($which_benchmarks, {
        protobuf_upb    => sub { GPD::Profile::Maps->decode_upb($data->{protobuf}) },
        protobuf_bbpb   => sub { GPD::Profile::Maps->decode_bbpb($data->{protobuf}) },
        sereal          => sub { $sereal_decoder->decode($data->{sereal}) },
        json            => sub { $json_decoder->decode($data->{json}) },
    }));
}

sub setup_decode_objects {
    my $persons = $benchmarks{encode}{objects};

    $benchmarks{decode}{objects}{protobuf} = GPD::Profile::PersonArray->encode($persons);
    $benchmarks{decode}{objects}{sereal} = $sereal_encoder->encode($persons);
    $benchmarks{decode}{objects}{json} = $json_decoder->encode($persons);
}

sub profile_decode_objects {
    my ($repeat_count, $which_benchmarks) = @_;
    my $data = $benchmarks{decode}{objects};

    cmpthese($repeat_count, filter_benchmarks($which_benchmarks, {
        protobuf_upb    => sub { GPD::Profile::PersonArray->decode_upb($data->{protobuf}) },
        protobuf_bbpb   => sub { GPD::Profile::PersonArray->decode_bbpb($data->{protobuf}) },
        sereal          => sub { $sereal_decoder->decode($data->{sereal}) },
        json            => sub { $json_decoder->decode($data->{json}) },
    }));
}

sub setup_decode_transform {
    my $make_random_any; $make_random_any = sub {
        my ($level) = @_;
        my $which = rand;

        if ($which < .10 / $level) {
            my ($map, $map_pb) = ({}, {});

            for my $i (0 .. rand(10)) {
                my $key = random_chars 2, 4;

                ($map->{$key}, $map_pb->{$key}) = $make_random_any->($level + 1);
            }

            return ($map, { map_value => $map_pb });
        } elsif ($which < .20 / $level) {
            my ($array, $array_pb) = ([], []);

            for my $i (0 .. rand(10)) {
                ($array->[$i], $array_pb->[$i]) = $make_random_any->($level + 1);
            }

            return ($array, { array_value => $array_pb });
        } elsif ($which < .50) {
            my $value = 1 + int(rand 100);

            return ($value, { int64_value => $value });
        } else {
            my $string = random_chars(6, 12);

            return ($string, { string_value => $string });
        }
    };

    my ($values, $values_pb);
    for my $i (1 .. 100) {
        ($values->{$i}, $values_pb->{$i}) = $make_random_any->(1);
    }

    $benchmarks{decode}{transform}{protobuf} = GPD::Profile::Values->encode({ values => $values_pb });
    $benchmarks{decode}{transform}{sereal} = $sereal_encoder->encode({ values => $values });
    $benchmarks{decode}{transform}{json} = $json_decoder->encode({ values => $values });
}

sub profile_decode_transform {
    my ($repeat_count, $which_benchmarks) = @_;
    my $data = $benchmarks{decode}{transform};

    cmpthese($repeat_count, filter_benchmarks($which_benchmarks, {
        protobuf_upb    => sub { GPD::Profile::Values->decode_upb($data->{protobuf}) },
        protobuf_bbpb   => sub { GPD::Profile::Values->decode_bbpb($data->{protobuf}) },
        sereal          => sub { $sereal_decoder->decode($data->{sereal}) },
        json            => sub { $json_decoder->decode($data->{json}) },
    }));
}

sub setup_encode_objects {
    my $make_random_person = sub {
        my ($id) = @_;

        return {
            id      => $id,
            name    => random_chars(3, 16) . ' ' . random_chars(6, 12),
            email   => random_chars(7, 12) . '@test.com',
        };
    };
    my $persons = {
        persons => [map $make_random_person->($_), 1 .. 100 ],
    };

    $benchmarks{encode}{objects} = $persons;
}

sub profile_encode_objects {
    my ($repeat_count, $which_benchmarks) = @_;
    my $data = $benchmarks{encode}{objects};

    cmpthese($repeat_count, filter_benchmarks($which_benchmarks, {
        protobuf_upb    => sub { GPD::Profile::PersonArray->encode($data) },
        sereal          => sub { $sereal_encoder->encode($data) },
        json            => sub { $json_decoder->encode($data) },
    }));
}

sub setup_encode_maps {
    my $make_string_int32_map = sub {
        return +{
            map +(random_chars(5, 15) => int(rand(1000) + 2)), (1 .. 30)
        };
    };
    my $make_string_string_map = sub {
        return +{
            map +(random_chars(5, 15) => random_chars(5, 15)), (1 .. 30)
        };
    };
    my $maps = {
        string_int32_maps => [
            map +($make_string_int32_map->()), (1 .. 10),
        ],
        string_string_maps => [
            map +($make_string_string_map->()), (1 .. 10),
        ],
    };
    my $proto_maps = {
        string_int32_maps => [
            map +{ string_int32_map => $_ }, @{$maps->{string_int32_maps}}
        ],
        string_string_maps => [
            map +{ string_string_map => $_ }, @{$maps->{string_string_maps}}
        ],
    };

    $benchmarks{encode}{maps}{protobuf} = $proto_maps;
    $benchmarks{encode}{maps}{plain} = $maps;
}

sub profile_encode_maps {
    my ($repeat_count, $which_benchmarks) = @_;
    my $data = $benchmarks{encode}{maps};

    cmpthese($repeat_count, filter_benchmarks($which_benchmarks, {
        protobuf_upb    => sub { GPD::Profile::Maps->encode($data->{protobuf}) },
        sereal          => sub { $sereal_encoder->encode($data->{plain}) },
        json            => sub { $json_decoder->encode($data->{plain}) },
    }));
}

sub start_callgrind {
    system "callgrind_control --instr=on $$";
}

sub stop_callgrind {
    system "callgrind_control --instr=off $$";
}

sub filter_benchmarks {
    my ($which, $benchmarks) = @_;
    my %which; @which{@$which} = ();

    return {
        map +(exists $which{$_} ? ($_ => $benchmarks->{$_}) : ()), keys %$benchmarks
    };
}

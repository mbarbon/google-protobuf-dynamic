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
    $d->map({
        package => 'profile',
        prefix  => 'GPD::Profile',
        options => { decode_blessed => 0 },
    });
}
my $sereal_encoder = Sereal::Encoder->new;
my $sereal_decoder = Sereal::Decoder->new;
my $json_decoder = JSON::XS->new;

exit main();

sub main {
    my %callgrind_benchmarks = (
        bbpb    => 'protobuf_bbpb',
        upb     => 'protobuf_upb',
        json    => 'json',
        sereal  => 'sereal',
    );

    my $arg_ok = GetOptions(
        'callgrind=s' => \(my $callgrind_implementation),
        'help'        => \(my $help),
    );
    if ($callgrind_implementation && !exists $callgrind_benchmarks{$callgrind_implementation}) {
        $arg_ok = 0;
    }

    if (!$arg_ok || $help) {
        help(!$arg_ok);
    }

    my $with_callgrind = 0;
    my $repeat_count = $callgrind_implementation ? 2000 : -3;
    my $which_benchmarks = $callgrind_implementation ?
        [$callgrind_benchmarks{$callgrind_implementation}] :
        [qw(protobuf_bbpb protobuf_upb json sereal)];

    setup();
    start_callgrind() if $callgrind_implementation;
    profile_decode_maps($repeat_count, $which_benchmarks);
}

sub help {
    my ($error) = @_;

    print <<EOT;
Usage: benchmark.pl [--help] [--callgrind=<implementation>]
EOT
    exit $error;
}

my %benchmarks;

sub random_chars {
    my ($min, $max) = @_;
    my $num = rand($max - $min) + $min;

    return join '', map chr(rand(26) + 97), 1 .. $num;
}

sub setup {
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
            map +{ string_string_map => $_ }, @{$maps->{string_string2_maps}}
        ],
    };

    $benchmarks{decode}{maps}{protobuf} = GPD::Profile::Maps->encode($proto_maps);
    $benchmarks{decode}{maps}{sereal} = $sereal_encoder->encode($maps);
    $benchmarks{decode}{maps}{json} = $json_decoder->encode($maps);
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

sub start_callgrind {
    system "callgrind_control --instr=on $$";
}

sub filter_benchmarks {
    my ($which, $benchmarks) = @_;
    my %which; @which{@$which} = ();

    return {
        map +(exists $which{$_} ? ($_ => $benchmarks->{$_}) : ()), keys %$benchmarks
    };
}

use strict;
use warnings;

use Benchmark qw(cmpthese);
use Sereal;
use JSON::XS ();
use JSON ();
use Google::ProtocolBuffers;
use Google::ProtocolBuffers::Dynamic;

Google::ProtocolBuffers->parsefile('t/proto/person.proto');

my $d;
{
    $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("person.proto");
    $d->load_file("person3.proto");
    $d->map_message("test.Person", "DynamicPerson");
    $d->map_message("test.PersonArray", "DynamicPersonArray");
    $d->map_message("test.Person3", "DynamicPerson3");
    $d->map_message("test.Person3Array", "DynamicPerson3Array");
    $d->resolve_references();
}

my $sereal_encoder = Sereal::Encoder->new;
my $sereal_decoder = Sereal::Decoder->new;

sub chars {
    my ($min, $max) = @_;
    my $num = rand($max - $min) + $min;

    return join '', map chr(rand(26) + 97), 1 .. $num;
}

sub random_person {
    my ($id) = @_;

    return {
        id      => $id,
        name    => chars(3, 16) . ' ' . chars(6, 12),
        email   => chars(7, 12) . '@test.com',
    };
}

my $person = random_person(1);
my $pb_person = Test::Person->encode($person);
my $pbd_person = DynamicPerson->encode($person);
my $sereal_person = $sereal_encoder->encode($person);
my $json_person = JSON::encode_json($person);

sub encode_protobuf_pp_one { Test::Person->encode($person); }
sub encode_protobuf_one { DynamicPerson->encode($person); }
sub encode_protobuf3_one { DynamicPerson3->encode($person); }
sub encode_sereal_one { $sereal_encoder->encode($person); }
sub encode_json_one { JSON::encode_json($person); }

sub decode_protobuf_pp_one { Test::Person->decode($pbd_person); }
sub decode_protobuf_one { DynamicPerson->decode($pbd_person); }
sub decode_protobuf3_one { DynamicPerson3->decode($pbd_person); }
sub decode_sereal_one { $sereal_decoder->decode($sereal_person); }
sub decode_json_one { JSON::from_json($json_person); }

print "\nEncoder\n";
cmpthese(-1, {
    protobuf_pp => \&encode_protobuf_pp_one,
    protobuf    => \&encode_protobuf_one,
    protobuf3   => \&encode_protobuf3_one,
    sereal      => \&encode_sereal_one,
    json        => \&encode_json_one,
});

print "\nDecoder\n";
cmpthese(-1, {
    protobuf_pp => \&decode_protobuf_pp_one,
    protobuf    => \&decode_protobuf_one,
    protobuf3   => \&decode_protobuf3_one,
    sereal      => \&decode_sereal_one,
    json        => \&decode_json_one,
});

my $persons = {
    persons => [map random_person($_), 1 .. 100 ],
};
my $pb_persons = Test::PersonArray->encode($persons);
my $pbd_persons = DynamicPersonArray->encode($persons);
my $sereal_persons = $sereal_encoder->encode($persons);
my $json_persons = JSON::encode_json($persons);

sub encode_protobuf_pp_arr { Test::PersonArray->encode($persons); }
sub encode_protobuf_arr { DynamicPersonArray->encode($persons); }
sub encode_protobuf3_arr { DynamicPerson3Array->encode($persons); }
sub encode_sereal_arr { $sereal_encoder->encode($persons); }
sub encode_json_arr { JSON::encode_json($persons); }

sub decode_protobuf_pp_arr { Test::PersonArray->decode($pbd_persons); }
sub decode_protobuf_arr { DynamicPersonArray->decode($pbd_persons); }
sub decode_protobuf3_arr { DynamicPerson3Array->decode($pbd_persons); }
sub decode_sereal_arr { $sereal_decoder->decode($sereal_persons); }
sub decode_json_arr { JSON::from_json($json_persons); }

print "\nEncoder (arrays)\n";
cmpthese(-1, {
    protobuf_pp => \&encode_protobuf_pp_arr,
    protobuf    => \&encode_protobuf_arr,
    protobuf3   => \&encode_protobuf3_arr,
    sereal      => \&encode_sereal_arr,
    json        => \&encode_json_arr,
});

print "\nDecoder (arrays)\n";
cmpthese(-1, {
    protobuf_pp => \&decode_protobuf_pp_arr,
    protobuf    => \&decode_protobuf_arr,
    protobuf3   => \&decode_protobuf3_arr,
    sereal      => \&decode_sereal_arr,
    json        => \&decode_json_arr,
});

## Google::ProtocolBuffers::Dynamic
Fast and complete Perl protobuf implementation using uPB and Google .proto parser

## Example usage

```perl
$dynamic = Google::ProtocolBuffers::Dynamic->new;
$dynamic->load_string("person.proto", <<'EOT');
syntax = "proto2";

package humans;

message Person {
  required string name = 1;
  required int32 id = 2;
  optional string email = 3;
}
EOT

$dynamic->map({ package => 'humans', prefix => 'Humans' });

# encoding/decoding
$person = Humans::Person->decode("\x0a\x03foo\x10\x1f");
$person = Humans::Person->decode_json('{"id":31,"name":"John Doe"}');
$bytes = Humans::Person->encode($person);
$bytes = Humans::Person->encode_json($person);

# field accessors
$person = Humans::Person->new;
$person->set_id(77);
$id = $person->get_id;
```

See [the full documentation](https://metacpan.org/pod/Google::ProtocolBuffers::Dynamic) on MetaCPAN.

## Description

This module provides a complete Protocol Buffers implementation for Perl: it supports both proto2 and proto3 syntax,
and it supports gRPC client/server generation using [Grpc::XS](https://metacpan.org/pod/Grpc::XS) as the transport (other
transports can be added).

Since Google C++ library is used for Protocol Buffer parsing and loading, the full protocol buffers syntax is supported.
Serialization/deserialization uses uPB, because it provides a better interface for the specific task of creating a protobuf
library for a dynamic language.

## Speed

There is a simple benchmark script in [scripts/benchmark.pl](https://github.com/mbarbon/google-protobuf-dynamic/blob/master/scripts/benchmark.pl):
it only tests a small subset of features, and the exact speed difference is going to depend heavily on the
exact shape of the data and the protobuf schema. As expected, `Google::ProtocolBuffers::Dynamic` is much faster than the
pure-Perl `Google::ProtocolBuffers` implementation. `JSON::XS` and `Sereal` are included only as speed reference: the feature set
of Protocol Buffers, JSON and Sereal is different enough that one can't be used as a drop-in for the other.

Example results, running on Perl 5.20 on an Intel i7-3537U
```
Encoder
                 Rate protobuf_pp    protobuf      sereal        json
protobuf_pp   95467/s          --        -91%        -95%        -95%
protobuf    1071850/s       1023%          --        -47%        -47%
sereal      2025658/s       2022%         89%          --         -0%
json        2029876/s       2026%         89%          0%          --

Decoder
                 Rate protobuf_pp        json    protobuf      sereal
protobuf_pp   64576/s          --        -85%        -90%        -95%
json         420872/s        552%          --        -37%        -67%
protobuf     670690/s        939%         59%          --        -47%
sereal      1274310/s       1873%        203%         90%          --

Encoder (arrays)
               Rate protobuf_pp    protobuf        json      sereal
protobuf_pp   853/s          --        -97%        -98%        -99%
protobuf    33184/s       3789%          --        -15%        -64%
json        39009/s       4471%         18%          --        -58%
sereal      92839/s      10780%        180%        138%          --

Decoder (arrays)
               Rate protobuf_pp    protobuf        json      sereal
protobuf_pp   519/s          --        -95%        -97%        -97%
protobuf    10987/s       2019%          --        -28%        -41%
json        15175/s       2827%         38%          --        -18%
sereal      18618/s       3491%         69%         23%          --
 ```

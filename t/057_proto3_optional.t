use t::lib::Test 'proto3_optional';

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("optional_proto3.proto");
$d->map_message("test.Optional1", "Optional1");
$d->resolve_references();

my %values = (
    double  => ["\x09\x00\x00\x00\x00\x00\x00\xc0?\x89\x02\x00\x00\x00\x00\x00\x00\xd0?", 0.125, 0.25, "\x09\x00\x00\x00\x00\x00\x00\x00\x00", "\x89\x02\x00\x00\x00\x00\x00\x00\x00\x00", 0.0],
    float   => ["\x15\x00\x00\x00>\x95\x02\x00\x00\x80>", 0.125, 0.25, "\x15\x00\x00\x00\x00", "\x95\x02\x00\x00\x00\x00", 0.0],
    int32   => ["\x18\x07\x98\x02\x08", 7, 8, "\x18\x00", "\x98\x02\x00", 0],
    int64   => ["\x20\x07\xa0\x02\x08", 7, 8, "\x20\x00", "\xa0\x02\x00", 0],
    uint32  => ["\x28\x07\xa8\x02\x08", 7, 8, "\x28\x00", "\xa8\x02\x00", 0],
    uint64  => ["\x30\x07\xb0\x02\x08", 7, 8, "\x30\x00", "\xb0\x02\x00", 0],
    bool    => ["\x38\x01\xb8\x02\x01", 1, 1, "\x38\x00", "\xb8\x02\x00", ''],
    string  => ["\x42\x02ab\xc2\x02\x02bc", 'ab', 'bc', "\x42\x00", "\xc2\x02\x00", ''],
    bytes   => ["\x4a\x02ab\xca\x02\x02bc", 'ab', 'bc', "\x4a\x00", "\xca\x02\x00", ''],
    enum    => ["\x50\x01\xd0\x02\x02", 1, 2, "\x50\x00", "\xd0\x02\x00", 0],
);

for my $type (sort keys %values) {
    my ($encoded_nondefault, $v_f, $v_pf, $encoded_f0, $encoded_pf0, $v_0) = @{$values{$type}};

    {
        my $decoded = Optional1->new({
            "${type}_f" => $v_f,
            "${type}_pf"=> $v_pf,
        });

        decode_eq_or_diff('Optional1', $encoded_nondefault, $decoded, "$type - non-default values both present in decoded value");
        encode_eq_or_diff('Optional1', $decoded, $encoded_nondefault, "$type - non-default value both present in encoded bytes");
    }

    {
        my $original = Optional1->new({
            "${type}_f" => $v_0,
            "${type}_pf"=> $v_0,
        });

        encode_eq_or_diff('Optional1', $original, $encoded_pf0, "$type - default value only encoded for optional fields");
    }
}

done_testing();

use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("bigint.proto");
$d->map_message("test.BigInts", "BigInts", { use_bigints => 1 });
$d->resolve_references();

{
    my $encoded = "\x08\xff\xff\xff\xff\x7f\x10\xff\xff\xff\xff\x7f";
    my $decoded = BigInts->new({
        int64_f  => Math::BigInt->new('0x7ffffffff'),
        uint64_f => Math::BigInt->new('0x7ffffffff'),,
    });

    decode_eq_or_diff('BigInts', $encoded, $decoded);
    encode_eq_or_diff('BigInts', $decoded, $encoded);
}

{
    my $encoded = "\x08\xff\xff\xff\x7f\x10\xff\xff\xff\x7f";
    my $decoded = BigInts->new({
        int64_f  => 0xfffffff,
        uint64_f => 0xfffffff,
    });

    decode_eq_or_diff('BigInts', $encoded, $decoded);
    encode_eq_or_diff('BigInts', $decoded, $encoded);
}

done_testing();

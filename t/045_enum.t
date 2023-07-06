use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("enum.proto");
$d->map_message("test.MessageBefore", "MessageBefore", { explicit_defaults => 1});
$d->map_message("test.MessageAfter", "MessageAfter", { explicit_defaults => 1});
$d->resolve_references();

decode_eq_or_diff(
    'MessageAfter', MessageBefore->encode({ value => 2, array => [1, 2] }),
    MessageAfter->new({ value => 2, array => [1, 2] }),
    "sanity check for the tests below",
);

decode_eq_or_diff(
    'MessageBefore', MessageAfter->encode({ value => 3, array => [3, 2, 1] }),
    MessageBefore->new({ value => 1, array => [1, 2, 1] }),
    "unknown enum value uses default in deserialization",
);

encode_throws_ok(
    'MessageBefore', { value => 3 },
    qr/Invalid enumeration value 3 for field 'test.MessageBefore.value'/,
    "unknown enum value croaks in serialization"
);

encode_throws_ok(
    'MessageBefore', { array => [3, 2] },
    qr/Invalid enumeration value 3 for field 'test.MessageBefore.array'/,
    "unknown enum value croaks in serialization"
);

done_testing;

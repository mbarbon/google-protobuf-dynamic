use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("order.proto");
$d->map_message("test.OrderedFields", "OrderedFields");
$d->map_message("test.DisorderedFields", "DisorderedFields");
$d->map_message("test.MixedOneof", "MixedOneof");
$d->map_message("test.InterleavedOneof", "InterleavedOneof");
$d->resolve_references();

my $sc = {
    field_1 => {
        field => 6,
    },
    field_2 => 7,
    field_3 => 8,
    field_4 => {
        field => 9,
    },
};

encode_eq_or_diff('OrderedFields', $sc, "\x0a\x02\x08\x06\x10\x07\x18\x08\x22\x02\x08\x09");
encode_eq_or_diff('DisorderedFields', $sc, "\x0a\x02\x08\x06\x10\x07\x18\x08\x22\x02\x08\x09");

my $oo1 = {
    field_1 => 6,
    field_2 => 7,
    field_3 => 8,
    field_4 => 9,
};
my $oo2 = {
    field_2 => 7,
    field_3 => 8,
    field_4 => 9,
};

encode_eq_or_diff('MixedOneof', $oo1, "\x08\x06\x10\x07\x20\x09");
encode_eq_or_diff('MixedOneof', $oo2, "\x10\x07\x18\x08\x20\x09");

my $io1 = {
    field_1 => 6,
    field_2 => 7,
    field_3 => 8,
    field_4 => 9,
    field_5 => 10,
    field_6 => 11,
};

encode_eq_or_diff('InterleavedOneof', $io1, "\x08\x06\x10\x07\x18\x08\x30\x0b");

done_testing();

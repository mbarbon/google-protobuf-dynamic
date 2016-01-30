use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("scalar.proto");
$d->load_file("repeated.proto");
$d->load_file("message.proto");
$d->map_message("test.Basic", "Basic", { check_enum_values => 0 });
$d->map_message("test.Repeated", "Repeated");
$d->map_message("test.Inner", "Inner");
$d->map_message("test.OuterWithMessage", "OuterWithMessage");
$d->resolve_references();

eq_or_diff(Basic->encode(tied_hash(
)), "");

eq_or_diff(Basic->encode(tied_hash(
    bool_f => 1,
)), "\x38\x01");

eq_or_diff(Repeated->encode({
    bool_f => tied_array(0, 1, 1),
}), "\x38\x00\x38\x01\x38\x01");

eq_or_diff(OuterWithMessage->encode({
    optional_inner => tied_hash(value => 3),
    repeated_inner => tied_array(
        tied_hash(value => 4),
        tied_hash(value => 5),
    ),
}), "\x0a\x02\x08\x03\x12\x02\x08\x04\x12\x02\x08\x05");

done_testing();


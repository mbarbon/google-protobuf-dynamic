use t::lib::Test;

my $encoded_true = "\x38\x01";
my $encoded_false = "\x38\x00";
my $encoded_default = "";
my $encoded_outer = "\x42\x02" . $encoded_true;

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("bool.proto");
    $d->map_message("test.Bool", "PerlBool", { explicit_defaults => 1, boolean_values => 'perl', decode_blessed => 0 });
    $d->resolve_references();

    decode_eq_or_diff('PerlBool', $encoded_true, { bool_f => 1 });
    decode_eq_or_diff('PerlBool', $encoded_false, { bool_f => '' });
    decode_eq_or_diff('PerlBool', $encoded_default, { bool_f => '' });
}

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("bool.proto");
    $d->map_message("test.Bool", "NumericBool", { explicit_defaults => 1, boolean_values => 'numeric', decode_blessed => 0 });
    $d->resolve_references();

    decode_eq_or_diff('NumericBool', $encoded_true, { bool_f => 1 });
    decode_eq_or_diff('NumericBool', $encoded_false, { bool_f => 0 });
    decode_eq_or_diff('NumericBool', $encoded_default, { bool_f => 0 });
}

SKIP: {
    skip 'JSON module not installed' unless eval { require JSON; 1 };

    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("bool.proto");
    $d->map_message("test.Bool", "JSONBool", { explicit_defaults => 1, boolean_values => 'json', decode_blessed => 0 });
    $d->map_message("test.OuterBool", "JSONOuterBool", { decode_blessed => 0 });
    $d->resolve_references();

    decode_eq_or_diff('JSONBool', $encoded_true, { bool_f => JSON::true() });
    decode_eq_or_diff('JSONBool', $encoded_false, { bool_f => JSON::false() });
    decode_eq_or_diff('JSONBool', $encoded_default, { bool_f => JSON::false() });
    decode_eq_or_diff('JSONOuterBool', $encoded_outer, { bool => { bool_f => JSON::true() } }, 'correct mapper object is used during bool decoding');
}

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("bool.proto");

    throws_ok(
        sub { $d->map_message("test.Bool", "OopsBool", { boolean_values => 'nope' }) },
        qr/Invalid value 'nope' for 'boolean_values' option/
    );
}

done_testing();

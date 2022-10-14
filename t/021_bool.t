use t::lib::Test;

my $encoded_true = "\x38\x01";
my $encoded_false = "\x38\x00";
my $encoded_default = "";

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("bool.proto");
    $d->map_message("test.Bool", "PerlBool", { explicit_defaults => 1, boolean_values => 'perl', decode_blessed => 0 });
    $d->resolve_references();

    eq_or_diff(PerlBool->decode($encoded_true), { bool_f => 1 });
    eq_or_diff(PerlBool->decode($encoded_false), { bool_f => '' });
    eq_or_diff(PerlBool->decode($encoded_default), { bool_f => '' });
}

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("bool.proto");
    $d->map_message("test.Bool", "NumericBool", { explicit_defaults => 1, boolean_values => 'numeric', decode_blessed => 0 });
    $d->resolve_references();

    eq_or_diff(NumericBool->decode($encoded_true), { bool_f => 1 });
    eq_or_diff(NumericBool->decode($encoded_false), { bool_f => 0 });
    eq_or_diff(NumericBool->decode($encoded_default), { bool_f => 0 });
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

use t::lib::Test;

my $encoded_true = "\x38\x01";
my $encoded_false = "\x38\x00";
my $encoded_default = "";

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("scalar.proto");
    $d->map_message("test.Basic", "PerlBasic", { explicit_defaults => 1, boolean_values => 'perl' });
    $d->resolve_references();

    is(PerlBasic->decode($encoded_true)->{bool_f}, 1);
    is(PerlBasic->decode($encoded_false)->{bool_f}, '');
    is(PerlBasic->decode($encoded_default)->{bool_f}, '');
}

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("scalar.proto");
    $d->map_message("test.Basic", "NumericBasic", { explicit_defaults => 1, boolean_values => 'numeric' });
    $d->resolve_references();

    is(NumericBasic->decode($encoded_true)->{bool_f}, 1);
    is(NumericBasic->decode($encoded_false)->{bool_f}, 0);
    is(NumericBasic->decode($encoded_default)->{bool_f}, 0);
}

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("scalar.proto");

    throws_ok(
        sub { $d->map_message("test.Basic", "OopsBasic", { boolean_values => 'nope' }) },
        qr/Invalid value 'nope' for 'boolean_values' option/
    );
}

done_testing();

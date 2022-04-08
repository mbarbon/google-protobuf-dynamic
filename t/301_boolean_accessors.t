use t::lib::Test;

my $encoded_true = "\x38\x01";
my $encoded_false = "\x38\x00";
my $encoded_default = "";

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("scalar.proto");
    $d->map_message("test.Basic", "PerlBasic", { explicit_defaults => 1, boolean_values => 'perl' });
    $d->resolve_references();

    my $obj = PerlBasic->new;

    is($obj->get_bool_f, '');

    $obj->set_bool_f("a");
    is($obj->get_bool_f, 1);

    $obj->set_bool_f(0);
    is($obj->get_bool_f, '');
}

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("scalar.proto");
    $d->map_message("test.Basic", "NumericBasic", { explicit_defaults => 1, boolean_values => 'numeric' });
    $d->resolve_references();

    my $obj = NumericBasic->new;

    is($obj->get_bool_f, 0);

    $obj->set_bool_f("a");
    is($obj->get_bool_f, 1);

    $obj->set_bool_f('');
    is($obj->get_bool_f, 0);
}

done_testing();


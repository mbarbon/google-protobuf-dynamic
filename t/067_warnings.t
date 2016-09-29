use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("person.proto");
$d->map_message("test.Person", "Person");
$d->map_message("test.PersonArray", "PersonArray");
$d->resolve_references();

my $uninit = "Use of uninitialized value in subroutine entry";

warning_is(
    sub { Person->encode({ name => undef, id => 3 }) },
    qq[While encoding field 'name': $uninit],
);

warning_is(
    sub { PersonArray->encode({ persons => [{ id => 1, name => 'a' }, { name => undef, id => 3 }] }) },
    qq[While encoding field 'persons.[1].name': $uninit],
);

done_testing();

use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("person.proto");
$d->map_message("test.Person", "Person");
$d->map_message("test.PersonArray", "PersonArray");
$d->load_file("map_proto2.proto");
$d->map_message("test.Maps", "Maps", { implicit_maps => 1 });
$d->map_message("test.Item", "Item");
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

warning_is(
    sub { PersonArray->encode({ persons => [{ id => 1, name => 'a' }, { name => undef, id => 3 }] }) },
    qq[While encoding field 'persons.[1].name': $uninit],
);

warning_is(
    sub { Maps->encode({ string_int32_map => { foo => undef } }) },
    qq[While encoding field 'string_int32_map.{foo}': $uninit],
);

warning_is(
    sub { Maps->encode({ int32_message_map => { 3 => { one_value => undef } } }) },
    qq[While encoding field 'int32_message_map.{3}.one_value': $uninit],
);

done_testing();

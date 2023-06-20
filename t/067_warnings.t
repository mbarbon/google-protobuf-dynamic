use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("person.proto");
$d->map_message("test.Person", "Person");
$d->map_message("test.PersonArray", "PersonArray");
$d->load_file("map_proto2.proto");
$d->map_message("test.Maps", "Maps", { implicit_maps => 1 });
$d->map_message("test.Item", "Item");
$d->resolve_references();

my $d1 = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d1->load_file("map_proto2.proto");
$d1->map_message("test.Maps", "NoMaps", { implicit_maps => 0 });
$d1->map_message("test.Item", "NoItem");
$d1->resolve_references();

my $uninit = "Use of uninitialized value in subroutine entry";

for my $method (encoder_functions) {
    my $method_desc = "($method)";

    warning_is(
        sub { Person->$method({ name => undef, id => 3 }) },
        qq[While encoding field 'name': $uninit],
        $method_desc,
    );

    warning_is(
        sub { PersonArray->$method({ persons => [{ id => 1, name => 'a' }, { name => undef, id => 3 }] }) },
        qq[While encoding field 'persons.[1].name': $uninit],
        $method_desc,
    );

    warning_is(
        sub { PersonArray->$method({ persons => [{ id => 1, name => 'a' }, { name => undef, id => 3 }] }) },
        qq[While encoding field 'persons.[1].name': $uninit],
        $method_desc,
    );

    warning_is(
        sub { Maps->$method({ string_int32_map => { foo => undef } }) },
        qq[While encoding field 'string_int32_map.{foo}': $uninit],
        $method_desc,
    );

    warning_is(
        sub { Maps->$method({ int32_message_map => { 3 => { one_value => undef } } }) },
        qq[While encoding field 'int32_message_map.{3}.one_value': $uninit],
        $method_desc,
    );
}

for my $method (decoder_functions) {
    my $method_desc = "($method)";

    warning_is(
        sub { Maps->$method(NoMaps->encode({ string_int32_map => [{ value => 1}] })) },
        qq[Incomplete map entry: missing key],
        $method_desc,
    );

    warning_is(
        sub { Maps->$method(NoMaps->encode({ string_int32_map => [{}] })) },
        qq[Incomplete map entry: missing key],
        $method_desc,
    );
}

done_testing();

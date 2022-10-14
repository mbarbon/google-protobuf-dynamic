use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("person.proto");
$d->map_message("test.Person", "Person");
$d->map_message("test.PersonArray", "PersonArray");
$d->resolve_references();

lives_ok(sub { Person->new_and_check({ id => 31, name => 'foo' }) });
lives_ok(sub { Person->new_and_check({ id => 32 }) });
lives_ok(sub { PersonArray->new_and_check({ persons => [] }) });
lives_ok(sub { PersonArray->new_and_check({ persons => [
    { id => 34, name => 'foo' }, { id => 35, name => 'bar' },
] }) });

throws_ok(
    sub { Person->new_and_check({ id => 33, neam => 'foo' }) },
    qr/Check failed: Unknown field 'neam' during check/,
);

=for TODO

Check required fields

throws_ok(
    sub { Person->new_and_check({ name => 'foo' }) },
    qr/Check failed: Unknown field 'neam' during check/,
);

=cut

throws_ok(
    sub { PersonArray->new_and_check({ persons => {} }) },
    qr/Not an array reference when encoding field 'test.PersonArray.persons'/,
);

throws_ok(
    sub { PersonArray->new_and_check({ persons => [undef] }) },
    qr/Not a hash reference when checking a test.Person value/,
);

throws_ok(
    sub { PersonArray->new_and_check({ persons => [{ id => 34, neam => 'foo' }] }) },
    qr/Check failed: Unknown field 'neam' during check/,
);

done_testing();

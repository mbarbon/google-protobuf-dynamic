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
    qr/Not an hash reference when checking a test.Person value/,
);

throws_ok(
    sub { PersonArray->new_and_check({ persons => [{ id => 34, neam => 'foo' }] }) },
    qr/Check failed: Unknown field 'neam' during check/,
);

done_testing();
exit 0;

my $p = Person->decode("\x0a\x03foo\x10\x1f");

eq_or_diff($p, Person->new({ id => 31, name => 'foo' }));

my $pa = PersonArray->decode("\x0a\x07\x0a\x03foo\x10\x1f" .
                                     "\x0a\x06\x0a\x02ba\x10\x20");

eq_or_diff($pa, PersonArray->new({
    persons => [
        Person->new({ id => 31, name => 'foo' }),
        Person->new({ id => 32, name => 'ba' }),
    ],
}));

eq_or_diff(Person->encode($p), "\x0a\x03foo\x10\x1f");
eq_or_diff(PersonArray->encode($pa), "\x0a\x07\x0a\x03foo\x10\x1f" .
                                               "\x0a\x06\x0a\x02ba\x10\x20");

throws_ok(
    sub { Person->decode("\x0a\x02") },
    qr/Deserialization failed: Unexpected EOF inside delimited string/,
);

done_testing();

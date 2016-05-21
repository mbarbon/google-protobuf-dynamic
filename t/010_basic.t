use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("person.proto");
$d->map_message("test.Person", "Person");
$d->map_message("test.PersonArray", "PersonArray");
$d->resolve_references();

my $p = Person->decode("\x0a\x03foo\x10\x1f");
my $pj = Person->decode_json('{"id":31,"name":"foo"}');

eq_or_diff($p, Person->new({ id => 31, name => 'foo' }));
eq_or_diff($pj, Person->new({ id => 31, name => 'foo' }));

my $pa = PersonArray->decode("\x0a\x07\x0a\x03foo\x10\x1f" .
                                     "\x0a\x06\x0a\x02ba\x10\x20");
my $paj = PersonArray->decode_json('{"persons":[{"name":"foo","id":31},{"name":"ba","id":32}]}');

eq_or_diff($pa, PersonArray->new({
    persons => [
        Person->new({ id => 31, name => 'foo' }),
        Person->new({ id => 32, name => 'ba' }),
    ],
}));
eq_or_diff($paj, PersonArray->new({
    persons => [
        Person->new({ id => 31, name => 'foo' }),
        Person->new({ id => 32, name => 'ba' }),
    ],
}));

eq_or_diff(Person->encode($p), "\x0a\x03foo\x10\x1f");
eq_or_diff(PersonArray->encode($pa), "\x0a\x07\x0a\x03foo\x10\x1f" .
                                               "\x0a\x06\x0a\x02ba\x10\x20");

eq_or_diff(Person->encode_json($p), '{"name":"foo","id":31}');
eq_or_diff(PersonArray->encode_json($pa), '{"persons":[{"name":"foo","id":31},{"name":"ba","id":32}]}');

throws_ok(
    sub { Person->decode("\x0a\x02") },
    qr/Deserialization failed: Unexpected EOF inside delimited string/,
);

done_testing();

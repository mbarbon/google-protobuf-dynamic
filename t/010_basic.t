use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("person.proto");
$d->map_message("test.Person", "Person");
$d->map_message("test.PersonArray", "PersonArray");
$d->resolve_references();

my $p = Person->decode_to_perl("\x0a\x03foo\x10\x1f");

eq_or_diff($p, { id => 31, name => 'foo', email => '' });

my $pa = PersonArray->decode_to_perl("\x0a\x07\x0a\x03foo\x10\x1f" .
                                     "\x0a\x06\x0a\x02ba\x10\x20");

eq_or_diff($pa, {
    persons => [
        { id => 31, name => 'foo', email => '' },
        { id => 32, name => 'ba', email => '' },
    ],
});

eq_or_diff(Person->encode_from_perl($p), "\x0a\x03foo\x10\x1f\x1a\x00");
eq_or_diff(PersonArray->encode_from_perl($pa), "\x0a\x09\x0a\x03foo\x10\x1f\x1a\x00" .
                                               "\x0a\x08\x0a\x02ba\x10\x20\x1a\x00");

done_testing();

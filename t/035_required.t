use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("person.proto");
$d->map_message("test.Person", "Person");
$d->resolve_references();

{
    my $encoded = "\x0a\x03foo\x10\x1f\x1a\x0cfoo\@test.com";
    my $decoded = Person->new({
        id => 31,
        name => 'foo',
        email => 'foo@test.com',
    });

    decode_eq_or_diff('Person', $encoded, $decoded);
    eq_or_diff(Person->encode($decoded), $encoded);
}

{
    my $encoded = "\x0a\x03foo";
    my $decoded = Person->new({
        name => 'foo',
    });

    decode_throws_ok(
        'Person', $encoded,
        qr/Deserialization failed: Missing required field test.Person.id/,
        qr/Deserialization failed: Missing required field test.Person.id/,
    );

    throws_ok(
        sub { Person->encode($decoded) },
        qr/Serialization failed: Missing required field test.Person.id/,
    );
}

done_testing();

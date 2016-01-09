use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("person.proto");
$d->map_message("test.Person", "Person");
$d->resolve_references();

{
    my $encoded = "\x0a\x03foo\x10\x1f\x1a\x0cfoo\@test.com";
    my $decoded = {
        id => 31,
        name => 'foo',
        email => 'foo@test.com',
    };

    eq_or_diff(Person->decode_to_perl($encoded), $decoded);
    eq_or_diff(Person->encode_from_perl($decoded), $encoded);
}

{
    my $encoded = "\x0a\x03foo";
    my $decoded = {
        name => 'foo',
    };

    throws_ok(
        sub { Person->decode_to_perl($encoded) },
        qr/Deserialization failed: Missing required field test.Person.id/,
    );

    throws_ok(
        sub { Person->encode_from_perl($decoded) },
        qr/Serialization failed: Missing required field test.Person.id/,
    );
}

done_testing();

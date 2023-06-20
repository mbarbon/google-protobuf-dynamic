use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("person.proto");
$d->map({ package => 'test', prefix => 'Test', options => { decode_blessed => 0 } });

my $p_bytes = "\x0a\x03foo\x10\x1f";
my $p = Test::Person->decode($p_bytes);
my $pj = Test::Person->decode_json('{"id":31,"name":"foo"}');

decode_eq_or_diff('Test::Person', $p_bytes, { id => 31, name => 'foo' });
eq_or_diff($pj, { id => 31, name => 'foo' });

my $pa_bytes = "\x0a\x07\x0a\x03foo\x10\x1f" .
                   "\x0a\x06\x0a\x02ba\x10\x20";
my $pa = Test::PersonArray->decode($pa_bytes);
my $paj = Test::PersonArray->decode_json('{"persons":[{"name":"foo","id":31},{"name":"ba","id":32}]}');

decode_eq_or_diff('Test::PersonArray', $pa_bytes, {
    persons => [
        { id => 31, name => 'foo' },
        { id => 32, name => 'ba' },
    ],
});
eq_or_diff($paj,{
    persons => [
        { id => 31, name => 'foo' },
        { id => 32, name => 'ba' },
    ],
});

encode_eq_or_diff('Test::Person', $p, "\x0a\x03foo\x10\x1f");
encode_eq_or_diff('Test::PersonArray', $pa, "\x0a\x07\x0a\x03foo\x10\x1f" .
                                               "\x0a\x06\x0a\x02ba\x10\x20");

eq_or_diff(Test::Person->encode_json($p), '{"name":"foo","id":31}');
eq_or_diff(Test::PersonArray->encode_json($pa), '{"persons":[{"name":"foo","id":31},{"name":"ba","id":32}]}');

done_testing();

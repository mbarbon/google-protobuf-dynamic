use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("person.proto");
$d->map({ package => 'test', prefix => 'Test', options => { decode_blessed => 0 } });

my $p = Test::Person->decode("\x0a\x03foo\x10\x1f");
my $pj = Test::Person->decode_json('{"id":31,"name":"foo"}');

eq_or_diff($p, { id => 31, name => 'foo' });
eq_or_diff($pj, { id => 31, name => 'foo' });

my $pa = Test::PersonArray->decode("\x0a\x07\x0a\x03foo\x10\x1f" .
                                     "\x0a\x06\x0a\x02ba\x10\x20");
my $paj = Test::PersonArray->decode_json('{"persons":[{"name":"foo","id":31},{"name":"ba","id":32}]}');

eq_or_diff($pa, {
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

eq_or_diff(Test::Person->encode($p), "\x0a\x03foo\x10\x1f");
eq_or_diff(Test::PersonArray->encode($pa), "\x0a\x07\x0a\x03foo\x10\x1f" .
                                               "\x0a\x06\x0a\x02ba\x10\x20");

eq_or_diff(Test::Person->encode_json($p), '{"name":"foo","id":31}');
eq_or_diff(Test::PersonArray->encode_json($pa), '{"persons":[{"name":"foo","id":31},{"name":"ba","id":32}]}');

done_testing();

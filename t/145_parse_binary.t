use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new;
my $blob = do {
    open my $fh, '<', 't/proto/person.pb';
    binmode $fh;
    local $/;
    readline $fh;
};
$d->load_serialized_string($blob);

$d->map({ package => 'test', prefix => 'Test' });

my $p = Test::Person->decode("\x0a\x03foo\x10\x1f");

eq_or_diff($p, Test::Person->new({ id => 31, name => 'foo' }));

done_testing();

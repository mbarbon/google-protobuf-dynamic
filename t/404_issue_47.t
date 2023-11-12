use Config;
use if !$Config{usethreads}, 'Test::More', skip_all => 'Threads not available';

use t::lib::Test;
use threads;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("person.proto");
$d->map({ package => 'test', prefix => 'Test' });

for my $i (1 .. 1) {
    threads->create(sub {});
}

$_->join for threads->list;

ok(1, 'got here');

# encoding/decoding still works

my $p_bytes = "\x0a\x03foo\x10\x1f";
my $p = Test::Person->decode($p_bytes);

eq_or_diff($p, Test::Person->new({ id => 31, name => 'foo' }));
eq_or_diff(Test::Person->encode($p), $p_bytes);
done_testing();

use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("message.proto");
$d->map({ package => 'test', prefix => 'Test' });

my $obj = Test::OuterWithMessage->new;
my $empty = {};
my $inner = Test::Inner->new;

eq_or_diff($obj->get_optional_inner, undef);

$obj->set_optional_inner($empty);
eq_or_diff($obj->get_optional_inner, $empty);

$obj->set_optional_inner($inner);
eq_or_diff($obj->get_optional_inner, $inner);

$obj->set_optional_inner(undef);
eq_or_diff($obj->get_optional_inner, undef);

throws_ok(
    sub { $obj->set_optional_inner(123) },
    qr/Value for message field 'test.OuterWithMessage.optional_inner' is not a hash reference/,
    'not a hash reference',
);

done_testing();

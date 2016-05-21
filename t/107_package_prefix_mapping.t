use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto/hierarchical');

$d->load_file("test1.proto");
$d->load_file("test2.proto");
$d->load_file("test3.proto");
$d->load_file("test4.proto");
$d->map_package_prefix('test.sub2', 'Test1::Sub2');
$d->map_package_prefix('test', 'Test1');
$d->resolve_references();

eq_or_diff(Test1::Foo->decode("\x08\x01"), Test1::Foo->new({
    foo => 1,
}), "exact mapping");
eq_or_diff(Test1::Sub2::Foo->decode("\x08\x01"), Test1::Sub2::Foo->new({
    foo => 1,
}), "exact mapping of subpackage");
eq_or_diff(Test1::sub1::Foo->decode("\x08\x01"), Test1::sub1::Foo->new({
    foo => 1,
}), "prefix mapping");
eq_or_diff(Test1::sub1::sub2::Foo->decode("\x08\x01"), Test1::sub1::sub2::Foo->new({
    foo => 1,
}), "multi-level prefix mapping");

done_testing();

use t::lib::Test;

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto/message_prefix');

$d->load_file("a.proto");
$d->load_file("b.proto");
$d->load_file("r.proto");
$d->map_message_prefix('a.A', 'Test');
$d->map_message('r.Foo', 'Foo');
$d->map_message_prefix('r.Foo', 'Test');
$d->resolve_references();

eq_or_diff(Test::a::A->decode("\x0a\x02\x08\x01"),
		   Test::a::A->new(
			   {
				   foo => Test::b::B->new(
					   {
						   bar => 1,
					   }
				   )
			   }
		   ),
		   "exact matching");

eq_or_diff(Foo->decode("\x0A\x04\x0A\x02\x10\x01\x10\x01"),
		   Foo->new(
			   {
				   bar => Test::r::Bar->new(
					   {
						   foo => Foo->new(
							   {
								   x => 1,
							   }
						   ),
					   }
				   ),
				   x => 1,
			   }
		   ),
		   "message recursion check");

done_testing();

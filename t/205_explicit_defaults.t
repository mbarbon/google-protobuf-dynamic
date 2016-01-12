use t::lib::Test;

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("options.proto");
    $d->map({ package => 'test', prefix => 'Test1', options => { explicit_defaults => 1 } });

    eq_or_diff(
        Test1::Defaults->decode_to_perl("\x08\x02"),
        Test1::Defaults->new({ value => 2 }),
    );
    eq_or_diff(
        Test1::Defaults->decode_to_perl(""),
        Test1::Defaults->new({ value => 7 }),
    );
}

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("options.proto");
    $d->map({ package => 'test', prefix => 'Test2', options => { explicit_default => 0 } });

    eq_or_diff(
        Test2::Defaults->decode_to_perl("\x08\x02"),
        Test2::Defaults->new({ value => 2 }),
    );
    eq_or_diff(
        Test2::Defaults->decode_to_perl(""),
        Test2::Defaults->new({}),
    );
}

done_testing();

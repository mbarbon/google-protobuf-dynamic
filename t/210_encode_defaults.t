use t::lib::Test skip_all => 'Placeholder';

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("options.proto");
    $d->map({ package => 'test', prefix => 'Test1', options => { encode_defaults => 1 } });

    eq_or_diff(
        Test1::Defaults->encode({ value => 2 }),
        "\x08\x02",
    );
    eq_or_diff(
        Test1::Defaults->encode({ value => 7 }),
        "\x08\x07",
    );
}

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("options.proto");
    $d->map({ package => 'test', prefix => 'Test2', options => { encode_default => 0 } });

    eq_or_diff(
        Test2::Defaults->encode({ value => 2 }),
        "\x08\x02",
    );
    eq_or_diff(
        Test2::Defaults->encode({ value => 7 }),
        "",
    );
}

done_testing();

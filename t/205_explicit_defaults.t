use t::lib::Test;

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("options.proto");
    $d->map({ package => 'test', prefix => 'Test1', options => { explicit_defaults => 1 } });

    eq_or_diff(
        Test1::Defaults->decode("\x08\x02"),
        Test1::Defaults->new({
            int32_f     => 2,
            uint32_f    => 8,
            int64_f     => 9,
            uint64_f    => 10,
            float_f     => 1.25,
            double_f    => 2.125,
            string_f    => "abcde",
            bytes_f     => "def",
            enum_f      => 2,
        }),
    );
    eq_or_diff(
        Test1::Defaults->decode(""),
        Test1::Defaults->new({
            int32_f     => 7,
            uint32_f    => 8,
            int64_f     => 9,
            uint64_f    => 10,
            float_f     => 1.25,
            double_f    => 2.125,
            string_f    => "abcde",
            bytes_f     => "def",
            enum_f      => 2,
        }),
    );
}

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("options.proto");
    $d->map({ package => 'test', prefix => 'Test2', options => { explicit_default => 0 } });

    eq_or_diff(
        Test2::Defaults->decode("\x08\x02"),
        Test2::Defaults->new({ int32_f => 2 }),
    );
    eq_or_diff(
        Test2::Defaults->decode(""),
        Test2::Defaults->new({}),
    );
}

done_testing();

use t::lib::Test;
use Alien::uPB;

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("options.proto");
    $d->map({ package => 'test', prefix => 'Test1', options => { encode_defaults => 1 } });

    eq_or_diff(
        Test1::Defaults->encode({
            int32_f     => 2,
            uint32_f    => 3,
            int64_f     => 4,
            uint64_f    => 5,
            float_f     => 2.25,
            double_f    => 1.125,
            string_f    => "xyz",
            bytes_f     => "abc",
            enum_f      => 1,
        }),
        join('',
             "\x08\x02",
             "\x10\x03",
             "\x18\x04",
             "\x20\x05",
             "\x2d\x00\x00\x10\x40",
             "\x31\x00\x00\x00\x00\x00\x00\xf2\x3f",
             "\x3a\x03xyz",
             "\x42\x03abc",
             "\x48\x01",
         ),
    );
    eq_or_diff(
        Test1::Defaults->encode({
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
        join('',
             "\x08\x07",
             "\x10\x08",
             "\x18\x09",
             "\x20\x0a",
             "\x2d\x00\x00\xa0\x3f",
             "\x31\x00\x00\x00\x00\x00\x00\x01\x40",
             "\x3a\x05abcde",
             "\x42\x03def",
             "\x48\x02",
         ),
    );
}

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("scalar.proto");
    $d->map({ package => 'test', prefix => 'Test4', options => { encode_defaults => 1 } });

    my $ref = bless { string_f => '' }, 'Test4::Basic';
    my $decoded = Test4::Basic->decode(Test4::Basic->encode($ref));
    eq_or_diff $decoded, $ref, "got the same structure back";
    is $decoded->get_string_f, '', "empty string";
}

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("options.proto");
    $d->map({ package => 'test', prefix => 'Test2', options => { encode_defaults => 0 } });

    eq_or_diff(
        Test2::Defaults->encode({
            int32_f     => 2,
            uint32_f    => 3,
            int64_f     => 4,
            uint64_f    => 5,
            float_f     => 2.25,
            double_f    => 1.125,
            string_f    => "xyz",
            bytes_f     => "abc",
            enum_f      => 1,
        }),
        join('',
             "\x08\x02",
             "\x10\x03",
             "\x18\x04",
             "\x20\x05",
             "\x2d\x00\x00\x10\x40",
             "\x31\x00\x00\x00\x00\x00\x00\xf2\x3f",
             "\x3a\x03xyz",
             "\x42\x03abc",
             "\x48\x01",
         ),
    );
    eq_or_diff(
        Test2::Defaults->encode({
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
        "",
    );
}

if (Google::ProtocolBuffers::Dynamic::is_proto3()) {
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("defaults_proto3.proto");
    $d->map({ package => 'test', prefix => 'Test3', options => { encode_defaults => 0 } });

    eq_or_diff(
        Test3::Defaults->encode({
            int32_f     => 2,
        }),
        join('',
             "\x08\x02",
         ),
    );
    eq_or_diff(
        Test3::Defaults->encode({
            int32_f     => 0,
        }),
        "",
    );
}

if (Google::ProtocolBuffers::Dynamic::is_proto3() && (!Alien::uPB->VERSION || Alien::uPB->VERSION >= '0.14')) {
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("defaults_proto3.proto");
    $d->map({ package => 'test', prefix => 'Test4', options => { encode_defaults => 1 } });

    eq_or_diff(
        Test4::Defaults->encode({
            int32_f     => 2,
        }),
        join('',
             "\x08\x02",
         ),
    );
    eq_or_diff(
        Test4::Defaults->encode({
            int32_f     => 0,
        }),
        "",
    );
}

done_testing();

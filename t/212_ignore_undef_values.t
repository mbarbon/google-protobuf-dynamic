use t::lib::Test;

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("map.proto");
    $d->map({ package => 'test', prefix => 'Test1', options => { encode_defaults_proto3 => 1 } });

    my $encoded;

    warning_like(sub {
        $encoded = Test1::Item->encode({ one_value => undef });
    }, qr/While encoding field 'one_value': Use of uninitialized value in subroutine entry/);
    eq_or_diff($encoded, "\x08\x00");

    warning_like(sub {
        $encoded = Test1::StringMap->encode({ string_int32_map => { a => undef } });
    }, qr/While encoding field 'string_int32_map.{a}': Use of uninitialized value in subroutine entry/);
    eq_or_diff($encoded, "\x0a\x05\x0a\x01a\x10\x00");
}

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("map.proto");
    $d->map({ package => 'test', prefix => 'Test2', options => { ignore_undef_fields => 1, encode_defaults_proto3 => 1 } });

    my $encoded;
    warning_like(sub {
        $encoded = Test2::Item->encode({ one_value => undef });
    }, undef);
    eq_or_diff($encoded, '');

    warning_like(sub {
        $encoded = Test2::StringMap->encode({ string_int32_map => { a => undef } });
    }, undef);
    eq_or_diff($encoded, '');
}

done_testing();

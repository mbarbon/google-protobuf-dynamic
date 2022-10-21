use t::lib::Test;

{
    my $d = Google::ProtocolBuffers::Dynamic->new;
    # protoc -o t/proto/person.pb t/proto/person.proto
    $d->load_serialized_string(_slurp('t/proto/person.pb'));

    $d->map({ package => 'test', prefix => 'Test1' });

    decode_eq_or_diff('Test1::Person', "\x0a\x03foo\x10\x1f",
                      Test1::Person->new({ id => 31, name => 'foo' }));
}

# check no WKTs are mapped since none is used
ok(!Google::ProtocolBuffers::Dynamic::WKT::Timestamp->can('new'));
ok(!Google::ProtocolBuffers::Dynamic::WKT::Duration->can('new'));

# uses multiple Google::ProtocolBuffers::Dynamic instances on purpose
{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    # protoc --include_imports -o t/proto/wkt/timestamp.pb t/proto/wkt/timestamp.proto
    $d->load_serialized_string(_slurp('t/proto/wkt/timestamp.pb'));

    $d->map({ package => 'test', prefix => 'Test2' });

    decode_eq_or_diff('Test2::Basic', "\x60\x03\x0a\x04\x08\x0f\x10\x11",
                      Test2::Basic->new({
                          timestamp_f => Google::ProtocolBuffers::Dynamic::WKT::Timestamp->new({ seconds => 15, nanos => 17 })
                      }));
}

# check only Timestamp is mapped
ok(Google::ProtocolBuffers::Dynamic::WKT::Timestamp->can('new'));
ok(!Google::ProtocolBuffers::Dynamic::WKT::Duration->can('new'));

# uses multiple Google::ProtocolBuffers::Dynamic instances on purpose
{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    # protoc --include_imports -o t/proto/wkt/scalar.pb t/proto/wkt/scalar.proto
    $d->load_serialized_string(_slurp('t/proto/wkt/scalar.pb'));

    $d->map({ package => 'test', prefix => 'Test3' });

    decode_eq_or_diff('Test3::Basic', "\x60\x03\x0a\x04\x08\x0f\x10\x11",
                      Test3::Basic->new({
                          timestamp_f => Google::ProtocolBuffers::Dynamic::WKT::Timestamp->new({ seconds => 15, nanos => 17 })
                      }));
}

# check all WKTs are mapped now
ok(Google::ProtocolBuffers::Dynamic::WKT::Timestamp->can('new'));
ok(Google::ProtocolBuffers::Dynamic::WKT::Duration->can('new'));

# uses multiple Google::ProtocolBuffers::Dynamic instances on purpose
{
    my $d = Google::ProtocolBuffers::Dynamic->new;
    # protoc --include_imports -I t/proto -o t/proto/wkt/scalar_copies.pb t/proto/wkt/scalar_copies.proto
    $d->load_serialized_string(_slurp('t/proto/wkt/scalar_copies.pb'));

    $d->map({ package => 'test', prefix => 'Test4' });
}

done_testing();

sub _slurp {
    open my $fh, '<', $_[0];
    binmode $fh;
    local $/;
    readline $fh;
}

use t::lib::Test 'proto3';

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("map.proto");
$d->map_message("test.Maps", "Maps");
$d->resolve_references();

my %values = (
    string_int32_map => [
        {
            "a" => 1,
            "b" => 7,
        },
        {
            a => "\x0a\x05\x0a\x01a\x10\x01",
            b => "\x0a\x05\x0a\x01b\x10\x07",
        },
    ],
);

sub encode {
    my ($map, $parts) = @_;

    return join('', map $parts->{$_}, keys %$map);
}

for my $field (sort keys %values) {
    my ($values, $encoded) = @{$values{$field}};
    my $bytes = Maps->encode({ $field => $values });
    my $decoded = Maps->decode($bytes);

    eq_or_diff($bytes, encode($values, $encoded),
               "$field - encoded value");
    eq_or_diff($decoded, Maps->new({ $field => $values }),
               "$field - round trip");
}

done_testing();

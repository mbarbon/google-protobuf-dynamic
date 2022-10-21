use t::lib::Test;

{
    my $mtt = $Google::ProtocolBuffers::Dynamic::Fieldtable::debug_transform;

    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("person3.proto");
    $d->load_file("map.proto");
    $d->map({ package => 'test', 'prefix' => 'Test1', options => { decode_blessed => 0 } });
    Test1::Person3->set_decoder_options({ fieldtable => 1, transform => $mtt });
    Test1::Person3Array->set_decoder_options({ fieldtable => 1, transform => $mtt });
    Test1::StringMap->set_decoder_options({ fieldtable => 1, transform => $mtt });

    my $persons_bytes = Test1::Person3Array->encode({
        persons => [
            { name => 'test1' },
            { id => 2, 'email' => 'abc' },
        ],
    });

    decode_eq_or_diff('Test1::Person3Array', $persons_bytes, [
        [1, [
            [ [1, 'test1'] ],
            [ [2, 2], [3, 'abc'] ]
        ] ],
    ]);

    my $map_bytes = Test1::StringMap->encode({
        string_int32_map => { a => 1, b => 2 },
    });

    decode_eq_or_diff('Test1::StringMap', $map_bytes, [ [1, { a => 1, b => 2 }] ]);
}

done_testing();

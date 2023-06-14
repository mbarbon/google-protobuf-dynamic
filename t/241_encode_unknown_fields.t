use t::lib::Test;

{
    my $mtt = $Google::ProtocolBuffers::Dynamic::Fieldtable::debug_encoder_unknown_fields;

    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("person3.proto");
    $d->map({ package => 'test', 'prefix' => 'Test1', options => { decode_blessed => 0 } });
    Test1::Person3->set_encoder_options({ unknown_field => $mtt });
    Test1::Person3Array->set_encoder_options({ unknown_field => $mtt });

    my $person_bytes = Test1::Person3->encode({
        id => 2, foo => { x => 1 }
    });

    eq_or_diff($person_bytes, "\x10\x02");
    eq_or_diff(Google::ProtocolBuffers::Dynamic::Fieldtable::debug_encoder_unknown_fields_get(), [
        ['foo', { x => 1 }],
    ]);

    my $person_array_bytes = Test1::Person3Array->encode({
        persons => [
            {
                id => 2, foo => 'w'
            },
            {
                id => 3
            },
            {
                id => 4, bar => 'xx'
            },
        ],
    });

    eq_or_diff($person_array_bytes, "\x0a\x02\x10\x02\x0a\x02\x10\x03\x0a\x02\x10\x04");
    eq_or_diff(Google::ProtocolBuffers::Dynamic::Fieldtable::debug_encoder_unknown_fields_get(), [
        ['persons', 0, 'foo', 'w'],
        ['persons', 2, 'bar', 'xx'],
    ]);
}

done_testing();

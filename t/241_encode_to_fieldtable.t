use t::lib::Test;

# transformation set on the message
{
    my $mtt = $Google::ProtocolBuffers::Dynamic::Fieldtable::debug_encoder_transform;

    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("person3.proto");
    $d->map({ package => 'test', 'prefix' => 'Test1', options => { decode_blessed => 0 } });
    Test1::Person3->set_encoder_options({ fieldtable => 1, transform => $mtt });
    Test1::Person3Array->set_encoder_options({ fieldtable => 1, transform => $mtt });

    my $person_bytes = Test1::Person3->encode([
        [ 2 => id => 2 ], [ 3 => email => 'abc' ],
    ]);

    decode_eq_or_diff('Test1::Person3', $person_bytes, {
        id => 2, email => 'abc',
    });

    my $persons_bytes = Test1::Person3Array->encode([
        [ 1 => persons => [
            [ [ 1 => name => 'test1' ] ],
            [ [ 2 => id => 2 ], [ 3 => 'email' => 'abc' ] ],
        ] ],
    ]);

    decode_eq_or_diff('Test1::Person3Array', $persons_bytes, {
        persons => [
            { name => 'test1' },
            { id => 2, email => 'abc' },
        ],
    });
}

# transformation set on a single field
{
    my $mtt = $Google::ProtocolBuffers::Dynamic::Fieldtable::debug_encoder_transform;

    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("transform/encoder.proto");
    $d->map({ package => 'test', 'prefix' => 'Test2', options => { decode_blessed => 0 } });
    Test2::ContainerMessage->set_encoder_options({ fieldtable => 1, transform_fields => { scalar => $mtt } });

    my $container_bytes = Test2::ContainerMessage->encode({
        scalar => [ [ 1 => id => 2 ], [ 2 => name => 'abc' ] ],
    });

    decode_eq_or_diff('Test2::ContainerMessage', $container_bytes, {
        scalar => { id => 2, name => 'abc' },
    });
}

# check required fields
{
    my $mtt = $Google::ProtocolBuffers::Dynamic::Fieldtable::debug_encoder_transform;

    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("person.proto");
    $d->map({ package => 'test', 'prefix' => 'Test3', options => { decode_blessed => 0 } });
    Test3::Person->set_encoder_options({ fieldtable => 1, transform => $mtt });

    my $person_bytes = Test3::Person->encode([
        [ 2 => id => 2 ], [ 1 => name => 'abc' ],
    ]);

    decode_eq_or_diff('Test1::Person3', $person_bytes, {
        id => 2, name => 'abc',
    });

    throws_ok(
        sub {
            Test3::Person->encode([
                [ 2 => id => 2 ], [ 3 => email => 'abc' ],
            ])
        },
        qr/Serialization failed: Missing required field 'test.Person.name'/,
    );
}

done_testing();

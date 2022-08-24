use t::lib::Test;

my @no_blessed = (options => { decode_blessed => 0 });

# transformed
{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("transform/decoder.proto");
    $d->map({ package => 'test', prefix => 'Test1', @no_blessed });

    my $strip_repeated_wrapper = {
        transform_fields => {
            transformed => sub { $_[0]->{values} },
        }
    };
    Test1::FieldMessage->set_decoder_options($strip_repeated_wrapper);

    eq_or_diff(Test1::FieldMessage->decode(Test1::FieldMessage->encode({
        transformed => {
            values => [1, 2, 3],
        },
        original => {
            values => [4, 5, 6],
        },
    })),{
        transformed => [1, 2, 3],
        original => {
            values => [4, 5, 6],
        },
    });
}

# field transform overrides message
{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("transform/decoder.proto");
    $d->map({ package => 'test', prefix => 'Test2', @no_blessed });

    my $strip_repeated_wrapper = {
        transform_fields => {
            transformed => sub { $_[0]->{values} },
        }
    };
    my $rename_repeated_wrapper = {
        transform => sub { { pretty_values => $_[0]->{values} } },
    };
    Test2::Int32Array->set_decoder_options($rename_repeated_wrapper);
    Test2::FieldMessage->set_decoder_options($strip_repeated_wrapper);

    eq_or_diff(Test2::FieldMessage->decode(Test2::FieldMessage->encode({
        transformed => {
            values => [1, 2, 3],
        },
        original => {
            values => [4, 5, 6],
        },
    })),{
        transformed => [1, 2, 3],
        original => {
            pretty_values => [4, 5, 6],
        },
    });
}

done_testing();

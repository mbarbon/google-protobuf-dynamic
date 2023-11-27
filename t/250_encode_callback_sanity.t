use t::lib::Test;

my @no_blessed = (options => { decode_blessed => 0 });

# transformation
{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("transform/encoder.proto");
    $d->map({ package => 'test', prefix => 'Test1', @no_blessed });

    {
        # using this variable is just to force the callback to be a closure
        # as opposed to just an anonymous function, this, together with
        # the extra scope is to test proper reference counting of callbacks
        my $key = 'values';
        my $add_repeated_wrapper = { transform => sub { $_[0] = { $key => $_[1] } } };
        my $add_tied_map_wrapper = { transform => sub { $_[0] = tied_hash($key => $_[1]) } };

        Test1::Int32Array->set_encoder_options($add_repeated_wrapper);
        Test1::Int32ArrayArray->set_encoder_options($add_repeated_wrapper);
        Test1::StringInt32MapTied->set_encoder_options($add_tied_map_wrapper);
    }

    # invalid value for wrapper
    throws_ok(
        sub {
            Test1::ContainerMessage->encode({
                array_wrapper => { a => 2, b => 3 },
            });
        },
        qr/Not an array reference when encoding field 'test.Int32ArrayArray.values'/
    );

    # nested messages
    eq_or_diff(Test1::ContainerMessage->decode(Test1::ContainerMessage->encode({
        array_wrapper => [[1, 2], [3, 4]],
    })), {
        array_wrapper => {
            values => [
                { values => [1, 2] },
                { values => [3, 4] },
            ],
        },
    });

    # top-level message
    eq_or_diff(Test1::Int32Array->decode(Test1::Int32Array->encode(
        [1, 2, 3]
    )), {
        values => [1, 2, 3],
    });

    # tied map value
    eq_or_diff(Test1::ContainerMessage->decode(Test1::ContainerMessage->encode({
        tied_map_wrapper => { a => 1 },
    })), {
        tied_map_wrapper => {
            values => {
                a => 1,
            },
        },
    });
}

done_testing();

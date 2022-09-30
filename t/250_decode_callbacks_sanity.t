use t::lib::Test;

my @no_blessed = (options => { decode_blessed => 0 });

# transformation
{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("transform/decoder.proto");
    $d->map({ package => 'test', prefix => 'Test1', @no_blessed });

    my $strip_repeated_wrapper = { transform => sub { $_[0] = $_[0]->{values} } };
    Test1::Int32Array->set_decoder_options($strip_repeated_wrapper);
    Test1::Int32ArrayArray->set_decoder_options($strip_repeated_wrapper);

    # nested messages
    eq_or_diff(Test1::Message->decode(Test1::Message->encode({
        nested_array => [{
            values => [{
                values => [1, 2, 3],
            }],
        }, {
            values => [{
                values => [4, 5, 6],
            }, {
                values => [7, 8, 9],
            }],
        }],
    })), {
        nested_array => [ [ [1, 2, 3] ], [ [4, 5, 6], [7, 8, 9] ] ],
    });

    # top-level message
    eq_or_diff(Test1::Int32Array->decode(Test1::Int32Array->encode({
        values => [1, 2, 3],
    })),
        [1, 2, 3]
    );
}

# no-op cases
my $noop_index = 0;
for my $noop (sub { $_[0] = 42 }, sub { }, sub { return }, sub { $_[0] }) {
    my $prefix = 'TestNoop' . ++$noop_index;
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("transform/decoder.proto");
    $d->map({ package => 'test', prefix => $prefix, @no_blessed });

    my $package = "${prefix}::Int32Array";
    $package->set_decoder_options({ transform => $noop });

    if ($noop_index == 1) {
        # check the test is sane
        eq_or_diff($package->decode($package->encode({
            values => [1, 2, 3],
        })),
            42,
        );
    } else {
        # actually check what we need to check
        eq_or_diff($package->decode($package->encode({
            values => [1, 2, 3],
        })), {
            values => [1, 2, 3],
        });
    }
}

# concatenated messages (callbacks are only triggered once)
{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("transform/decoder.proto");
    $d->map({ package => 'test', prefix => 'Test3', @no_blessed });

    my $strip_repeated_wrapper = { transform => sub { $_[0] = $_[0]->{values} } };
    Test3::Int32Array->set_decoder_options($strip_repeated_wrapper);
    Test3::Int32ArrayArray->set_decoder_options($strip_repeated_wrapper);

    # nested messages
    eq_or_diff(Test3::Message->decode(Test3::Message->encode({
        concatenated_array => {
            values => [{
                values => [1, 2, 3],
            }, {
                values => [4, 5, 6],
            }],
        },
    }) . Test3::Message->encode({
        concatenated_array => {
            values => [{
                values => [7, 8, 9],
            }],
        },
    })), {
        concatenated_array => [ [1, 2, 3], [4, 5, 6], [7, 8, 9] ],
    });
}

done_testing();

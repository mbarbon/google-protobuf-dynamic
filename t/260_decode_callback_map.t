use t::lib::Test;

my @no_blessed = (options => { decode_blessed => 0 });

# map values
{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("transform/decoder.proto");
    $d->map({ package => 'test', prefix => 'Test1', @no_blessed });

    my $strip_repeated_wrapper = { transform => sub { $_[0] = $_[0]->{values} } };
    Test1::Int32Array->set_decoder_options($strip_repeated_wrapper);

    eq_or_diff(Test1::MapMessage->decode(Test1::MapMessage->encode({
        map_values => {
            key1   => {
                values => [1, 2, 3],
            },
        },
    })), {
        map_values => {
            key1 => [1, 2, 3],
        },
    });
}

done_testing();

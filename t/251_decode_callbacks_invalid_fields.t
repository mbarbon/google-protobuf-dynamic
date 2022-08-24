use t::lib::Test;

my @no_blessed = (options => { decode_blessed => 0 });

# invalid fields
{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("transform/decoder.proto");
    $d->map({ package => 'test', prefix => 'Test1', @no_blessed });

    for my $field_name (qw(
            not_message
            repeated_not_message
            repeated_message
            map_field
            )) {
        throws_ok(
            sub {
                Test1::InvalidFields->set_decoder_options({
                    transform_fields => {
                        $field_name => sub { $_[0] },
                    },
                });
            },
            qr/^Can't apply transformation to field $field_name /,
        );
    }
}

done_testing();

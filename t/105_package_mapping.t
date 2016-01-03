use Test::More;
use Test::Exception;
use Test::Differences;

use Google::ProtocolBuffers::Dynamic;

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto/mapping');

    $d->load_file("test1.proto");
    $d->map_package('test1', 'Test1');
    $d->resolve_references();

    eq_or_diff(Test1::Message1->decode_to_perl("\x08\x01"), {
        test1_message1 => 1,
    }, "simple message 2");
    eq_or_diff(Test1::Message2->decode_to_perl("\x08\x01"), {
        test1_message2 => 1,
    }, "simple message 2");
    eq_or_diff(Test1::Message3->decode_to_perl("\x0a\x02\x08\x01\x12\x02\x08\x01"), {
        test1_message3_message1 => {
            test1_message1 => 1,
        },
        test1_message3_message2 => {
            test1_message2 => 1,
        },
    }, "composite message");
}

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto/mapping');

    $d->load_file("test1.proto");
    $d->load_file("test2.proto");
    $d->map_package('test1', 'Test1');
    $d->map_package('test2', 'Test2');
    $d->resolve_references();

    eq_or_diff(Test2::Message3->decode_to_perl("\x0a\x02\x08\x01\x12\x02\x08\x01"), {
        test1_message3_message1 => {
            test2_message1 => 1,
        },
        test1_message3_message2 => {
            test1_message2 => 1,
        },
    }, "composite message - multiple packages");
}

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto/mapping');

    $d->load_file("test1.proto");
    $d->load_file("test2.proto");
    $d->map_package('test1', 'Test1');

    throws_ok(
        sub { $d->map_package('test2', 'Test1') },
        qr/Package 'Test1::Message1' has already been used in a message mapping/,
        "duplicated message",
    );

    lives_ok(
        sub { $d->map_package('test1', 'Test1') },
        "mapping a package multiple times is a no-op",
    );
}

done_testing();

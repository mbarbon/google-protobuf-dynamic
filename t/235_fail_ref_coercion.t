use t::lib::Test;
use if $] < 5.014, 'Test::More' => 'skip_all' => 'Not available on Perl < 5.14';

{
    package TestObj;

    sub new { bless {}, __PACKAGE__ }

    package TestOvl;

    use overload (
        '0+'     => sub { 123 },
    );

    sub new { bless {}, __PACKAGE__ }
}

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("scalar.proto");
    $d->load_file("repeated.proto");
    $d->map({ package => 'test', prefix => 'Test1', options => { fail_ref_coercion => 1 } });

    my $ref = \1;
    my $obj = TestObj->new;
    my $ovl = TestOvl->new;

    throws_ok(
        sub { Test1::Basic->encode({ string_f => $ref }) },
        qr/Reference used when a scalar value is expected for field 'test.Basic.string_f'/,
        "ref for scalar croaks in serialization"
    );

    throws_ok(
        sub { Test1::Basic->encode({ string_f => $obj }) },
        qr/Reference used when a scalar value is expected for field 'test.Basic.string_f'/,
        "obj for scalar croaks in serialization"
    );

    throws_ok(
        sub { Test1::Repeated->encode({ string_f => ['a', $ref] }) },
        qr/Reference used when a scalar value is expected for field 'test.Repeated.string_f'/,
        "ref for repeated scalar croaks in serialization"
    );

    eq_or_diff(
        Test1::Repeated->decode(Test1::Repeated->encode({ string_f => ['a', $ovl] })),
        Test1::Repeated->new({
            string_f => ['a', "123"],
        }),
    );
}

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("scalar.proto");
    $d->load_file("repeated.proto");
    $d->map({ package => 'test', prefix => 'Test2', options => { fail_ref_coercion => 0 } });

    my $ref = \1;
    my $obj = TestObj->new;
    my $ovl = TestOvl->new;

    eq_or_diff(
        Test2::Basic->decode(Test2::Basic->encode({ string_f => $ref })),
        Test2::Basic->new({
            string_f => "$ref",
        }),
    );

    eq_or_diff(
        Test2::Basic->decode(Test2::Basic->encode({ string_f => $obj })),
        Test2::Basic->new({
            string_f => "$obj",
        }),
    );

    eq_or_diff(
        Test2::Repeated->decode(Test2::Repeated->encode({ string_f => ['a', $ref] })),
        Test2::Repeated->new({
            string_f => ['a', "$ref"],
        }),
    );

    eq_or_diff(
        Test2::Repeated->decode(Test2::Repeated->encode({ string_f => ['a', $ovl] })),
        Test2::Repeated->new({
            string_f => ['a', "123"],
        }),
    );
}

done_testing();

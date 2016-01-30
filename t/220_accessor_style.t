use t::lib::Test;

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("scalar.proto");
    $d->load_file("repeated.proto");
    $d->load_file("extensions.proto");
    $d->map({ package => 'test', prefix => 'Test1', options => { accessor_style => 'get_and_set' } });

    my $scalar = Test1::Basic->new;
    my $repeated = Test1::Repeated->new;
    my $extensions = Test1::Message1->new;

    is($scalar->get_int32_f, 0);
    $scalar->set_int32_f(2);
    is($scalar->get_int32_f, 2);

    $repeated->add_double_f(2);
    is($repeated->get_double_f(0), 2);
    eq_or_diff($repeated->get_double_f_list, [2]);
    $repeated->set_double_f_list([7]);
    eq_or_diff($repeated->get_double_f_list, [7]);
    $repeated->set_double_f(0, 4);
    is($repeated->get_double_f(0), 4);

    is($extensions->get_extension('test.value'), 0);
    $extensions->set_extension('test.value', 7);
    is($extensions->get_extension('test.value'), 7);
}

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("scalar.proto");
    $d->load_file("repeated.proto");
    $d->load_file("extensions.proto");
    $d->map({ package => 'test', prefix => 'Test2', options => { accessor_style => 'plain_and_set' } });

    my $scalar = Test2::Basic->new;
    my $repeated = Test2::Repeated->new;
    my $extensions = Test2::Message1->new;

    is($scalar->int32_f, 0);
    $scalar->set_int32_f(2);
    is($scalar->int32_f, 2);

    $repeated->add_double_f(2);
    is($repeated->double_f(0), 2);
    eq_or_diff($repeated->double_f_list, [2]);
    $repeated->set_double_f_list([7]);
    eq_or_diff($repeated->double_f_list, [7]);
    $repeated->set_double_f(0, 4);
    is($repeated->double_f(0), 4);

    is($extensions->extension('test.value'), 0);
    $extensions->set_extension('test.value', 7);
    is($extensions->extension('test.value'), 7);
}

{
    my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
    $d->load_file("scalar.proto");
    $d->load_file("repeated.proto");
    $d->load_file("extensions.proto");
    $d->map({ package => 'test', prefix => 'Test3', options => { accessor_style => 'single_accessor' } });

    my $scalar = Test3::Basic->new;
    my $repeated = Test3::Repeated->new;
    my $extensions = Test3::Message1->new;

    is($scalar->int32_f, 0);
    $scalar->int32_f(2);
    is($scalar->int32_f, 2);

    $repeated->add_double_f(2);
    is($repeated->double_f(0), 2);
    eq_or_diff($repeated->double_f_list, [2]);
    $repeated->double_f_list([7]);
    eq_or_diff($repeated->double_f_list, [7]);
    $repeated->double_f(0, 4);
    is($repeated->double_f(0), 4);

    is($extensions->extension('test.value'), 0);
    $extensions->extension('test.value', 7);
    is($extensions->extension('test.value'), 7);
}

done_testing();

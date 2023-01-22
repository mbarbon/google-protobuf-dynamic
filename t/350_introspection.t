use t::lib::Test;

use Google::ProtocolBuffers::Dynamic qw(:descriptor :values);

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("scalar.proto");
$d->load_file("repeated.proto");
$d->load_file("message.proto");
$d->map({ package => 'test', prefix => 'Test' });

my $basic = Test::Basic->message_descriptor();
my $fields = $basic->fields;

is($basic->full_name, 'test.Basic');

is(scalar @$fields, 16);
eq_or_diff([sort map $_->name, @$fields], [qw(
    bool_f
    bytes_f
    double_f
    enum_f
    fixed32_f
    fixed64_f
    float_f
    int32_f
    int64_f
    sfixed32_f
    sfixed64_f
    sint32_f
    sint64_f
    string_f
    uint32_f
    uint64_f
)]);

my $bool_f = $basic->find_field_by_name('bool_f');
is($bool_f->full_name, 'bool_f');
is($bool_f->containing_type->full_name, 'test.Basic');
is($bool_f->descriptor_type, DESCRIPTOR_BOOL);
is($bool_f->value_type, VALUE_BOOL);
isnt(VALUE_BOOL, DESCRIPTOR_BOOL);

check_props($basic, {
    bool_f      => { is_primitive => 1 },
    bytes_f     => { is_string => 1 },
    string_f    => { is_string => 1 },
});

check_props(Test::Repeated->message_descriptor(), {
    bool_f => { is_primitive => 1, is_repeated => 1 },
});

check_props(Test::Packed->message_descriptor(), {
    bool_f => { is_primitive => 1, is_repeated => 1, is_packed => 1 },
});

check_props(Test::OuterWithMessage->message_descriptor(), {
    optional_inner => { is_message => 1 },
    repeated_inner => { is_message => 1, is_repeated => 1 },
});

check_default_value(Test::Basic->message_descriptor(), {
    bool_f      => '',
    bytes_f     => '',
    double_f    => 0.0,
    enum_f      => 1,
    fixed32_f   => 0,
    fixed64_f   => 0,
    float_f     => 0.0,
    int32_f     => 0,
    int64_f     => 0,
    sfixed32_f  => 0,
    sfixed64_f  => 0,
    sint32_f    => 0,
    sint64_f    => 0,
    string_f    => '',
    uint32_f    => 0,
    uint64_f    => 0,
});

check_default_value(Test::Default->message_descriptor(), {
    bool_f      => 1,
    bytes_f     => 'some bytes',
    double_f    => 1.0,
    enum_f      => 3,
    fixed32_f   => 9,
    fixed64_f   => 11,
    float_f     => 2.0,
    int32_f     => 3,
    int64_f     => 4,
    sfixed32_f  => -10,
    sfixed64_f  => -12,
    sint32_f    => -7,
    sint64_f    => -8,
    string_f    => 'a string',
    uint32_f    => 5,
    uint64_f    => 6,
});

done_testing();

sub check_props {
    my($message_def, $properties) = @_;

    for my $field_name (sort keys %$properties) {
        my $field = $message_def->find_field_by_name($field_name);
        my $props = $properties->{$field_name};

        for my $prop (qw(is_primitive is_string is_message is_repeated is_packed)) {
            is($field->$prop, $props->{$prop} // '', "$field_name - $prop");
        }
    }
}

sub check_default_value {
    my($message_def, $default_values) = @_;

    for my $field_name (sort keys %$default_values) {
        my $field = $message_def->find_field_by_name($field_name);
        my $default_value = $default_values->{$field_name};

        is($field->default_value, $default_value, "$field_name - default_value");
    }
}

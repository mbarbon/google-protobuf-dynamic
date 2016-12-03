use t::lib::Test;

use Google::ProtocolBuffers::Dynamic qw(:descriptor :values);

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("scalar.proto");
$d->map({ package => 'test', prefix => 'Test' });

my $basic = Test::Basic->message_descriptor();
my $fields = $basic->fields;
my $bool_f = $basic->find_field_by_name('bool_f');
my $enum_f = $basic->find_field_by_name('enum_f');

is($basic->full_name, 'test.Basic');

is(scalar @$fields, 16);
eq_or_diff([sort map $_->full_name, @$fields], [qw(
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

is($bool_f->full_name, 'bool_f');
is($bool_f->containing_type->full_name, 'test.Basic');
is($bool_f->descriptor_type, DESCRIPTOR_BOOL);
is($bool_f->value_type, VALUE_BOOL);
isnt(VALUE_BOOL, DESCRIPTOR_BOOL);

my $enum_t = $enum_f->enum_type;
is($enum_t->full_name, 'test.Enum');
eq_or_diff($enum_t->values, {
    FIRST   => 1,
    SECOND  => 2,
    THIRD   => 3,
});
is($enum_t->find_name_by_number(2), 'SECOND');
is($enum_t->find_name_by_number(7), undef);
is($enum_t->find_number_by_name('SECOND'), 2);
is($enum_t->find_number_by_name('SEVENTH'), undef);

my $enum_t_again = Test::Enum->enum_descriptor;
is($enum_t_again->full_name, 'test.Enum');
eq_or_diff($enum_t_again->values, {
    FIRST   => 1,
    SECOND  => 2,
    THIRD   => 3,
});

done_testing();

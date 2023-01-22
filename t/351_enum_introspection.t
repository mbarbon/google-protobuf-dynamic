use t::lib::Test;

use Google::ProtocolBuffers::Dynamic qw(:descriptor :values);

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("scalar.proto");
$d->map({ package => 'test', prefix => 'Test' });

my $enum_f = Test::Basic->message_descriptor->find_field_by_name('enum_f');

my $enum_t = $enum_f->enum_type;
is($enum_t->full_name, 'test.Enum');
eq_or_diff($enum_t->values, {
    FIRST   => 1,
    SECOND  => 2,
    THIRD   => 3,
});
is($enum_t->default_value, 1);
is($enum_t->value_count, 3);
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

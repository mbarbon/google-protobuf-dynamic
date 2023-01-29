use t::lib::Test;

use Google::ProtocolBuffers::Dynamic qw(:descriptor :values);

my $d = Google::ProtocolBuffers::Dynamic->new('t/proto');
$d->load_file("scalar.proto");
$d->load_file('extensions.proto');
$d->load_file('grpc/greeter.proto');
$d->map(
    { package => 'test', prefix => 'Test' },
    { package => 'helloworld', prefix => 'Test', options => {
        client_services => 'noop',
    }},
);

my $scalar_file = Test::Basic->message_descriptor->file;
is($scalar_file->package, 'test');
is($scalar_file->name, 'scalar.proto');

{
    my $messages = $scalar_file->messages;

    isa_ok($_, 'Google::ProtocolBuffers::Dynamic::MessageDef') for @$messages;
    eq_or_diff([map $_->name, @$messages], [qw(Basic Default)]);
}

{
    my $enums = $scalar_file->enums;

    isa_ok($_, 'Google::ProtocolBuffers::Dynamic::EnumDef') for @$enums;
    eq_or_diff([map $_->name, @$enums], [qw(Enum)]);
}

my $service_file = Test::Greeter->service_descriptor->file;
is($service_file->package, 'helloworld');
is($service_file->name, 'grpc/greeter.proto');

{
    my $services = $service_file->services;

    isa_ok($_, 'Google::ProtocolBuffers::Dynamic::ServiceDef') for @$services;
    eq_or_diff([map $_->name, @$services], [qw(Greeter)]);
}

my $extension_file = Test::Message1->message_descriptor->file;
is($extension_file->package, 'test');
is($extension_file->name, 'extensions.proto');

{
    my $extensions = $extension_file->extensions;

    isa_ok($_, 'Google::ProtocolBuffers::Dynamic::FieldDef') for @$extensions;
    eq_or_diff([map $_->name, @$extensions], [qw(extension1 value)]);
}

done_testing();

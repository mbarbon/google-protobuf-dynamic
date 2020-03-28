package Google::ProtocolBuffers::Dynamic;
# ABSTRACT: fast and complete protocol buffer implementation

use strict;
use warnings;
use XSLoader;
use Exporter ();

# @EXPORT_OK/%EXPORT_TAGS are set up in XS
*import = \&Exporter::import;

# VERSION

XSLoader::load(__PACKAGE__);

my $REQUIRED_MAP_PACKAGE_PREFIX = [qw(pb_prefix prefix)];
my $REQUIRED_MAP_MESSAGE_PREFIX = [qw(message prefix)];
my $REQUIRED_MAP_PACKAGE = [qw(package prefix)];
my $REQUIRED_MAP_MESSAGE = [qw(message to)];
my $REQUIRED_MAP_ENUM = [qw(enum to)];
my $REQUIRED_MAP_SERVICE = [qw(service to)];

sub map {
    my ($self, @mappings) = @_;

    for my $mapping (@mappings) {
        if (exists $mapping->{pb_prefix}) {
            _check_keys($mapping, $REQUIRED_MAP_PACKAGE_PREFIX);
            $self->map_package_prefix($mapping->{pb_prefix}, $mapping->{prefix}, $mapping->{options});
        } elsif (exists $mapping->{message} && exists $mapping->{prefix}) {
            _check_keys($mapping, $REQUIRED_MAP_MESSAGE_PREFIX);
            $self->map_message_prefix($mapping->{message}, $mapping->{prefix}, $mapping->{options});
        } elsif (exists $mapping->{package}) {
            _check_keys($mapping, $REQUIRED_MAP_PACKAGE);
            $self->map_package($mapping->{package}, $mapping->{prefix}, $mapping->{options});
        } elsif (exists $mapping->{message}) {
            _check_keys($mapping, $REQUIRED_MAP_MESSAGE);
            $self->map_message($mapping->{message}, $mapping->{to}, $mapping->{options});
        } elsif (exists $mapping->{enum}) {
            _check_keys($mapping, $REQUIRED_MAP_ENUM);
            $self->map_enum($mapping->{enum}, $mapping->{to}, $mapping->{options});
        } elsif (exists $mapping->{service}) {
            _check_keys($mapping, $REQUIRED_MAP_SERVICE);
            $self->map_service($mapping->{service}, $mapping->{to}, $mapping->{options});
        } else {
            die "Unrecognized mapping ", _dump($mapping);
        }
    }

    $self->resolve_references;
}

sub _check_keys {
    my ($mapping, $required_keys) = @_;
    my %keys; @keys{keys %$mapping} = ();

    delete $keys{options};
    for my $required_key (@$required_keys) {
        if (!exists $keys{$required_key}) {
            die "Missing required key '$required_key' in mapping ", _dump($mapping);
        }
        delete $keys{$required_key};
    }

    die "Unknown keys in mapping: ", join(", ", map "'$_'", keys %keys), " ", _dump($mapping)
        if %keys;
}

sub _dump {
    require Data::Dumper;

    local $Data::Dumper::Terse = 1;
    Data::Dumper::Dumper(@_)
}

1;

__END__

=head1 SYNOPSIS

    $dynamic = Google::ProtocolBuffers::Dynamic->new;
    $dynamic->load_string("person.proto", <<'EOT');
    syntax = "proto2";

    package humans;

    message Person {
      required string name = 1;
      required int32 id = 2;
      optional string email = 3;
    }
    EOT

    $dynamic->map({ package => 'humans', prefix => 'Humans' });

    # encoding/decoding
    $person = Humans::Person->decode("\x0a\x03foo\x10\x1f");
    $person = Humans::Person->decode_json('{"id":31,"name":"John Doe"}');
    $bytes = Humans::Person->encode($person);
    $bytes = Humans::Person->encode_json($person);

    # field accessors
    $person = Humans::Person->new;
    $person->set_id(77);
    $id = $person->get_id;

See also L<protoc-gen-perl-gpd> for ahead-of-time generation.

=head1 DESCRIPTION

This module uses Google C++ Protocol Buffers parser and the uPB
serialization/deserialization library to provide a fast and complete
mapping of Protocol Buffers for Perl.

There is builtin support for creating gRPC clients for RPC services,
using L<Grpc::XS> as transport.

Protocol Buffers objects are normal blessed hashes, with
auto-generated accessors for message fields. Accessing the hash
directly is possible but not recommended (it will skip validation, it
won't respect the default value option of fields, it won't respect
oneof semantics, ...).

=head1 LANGUAGE MAPPING

The following section outlines how Protocol Buffer sematics are mapped
to Perl; it also doubles as an API reference for generated message
classes.

=head2 PACKAGES

In general, there is no direct correspondence between Protocol Buffer
packages and Perl packages (each Protocol Buffer message can be mapped
independently to an arbitrary Perl package).

That being said, it is simpler to map each Protocol Buffer package to a
Perl package prefix, and use the automated package mapping (when
C<pb_test> is mapped to C<Perl::Test>, C<pb_test.Message> becomes
C<Perl::Test::Message>, C<pb_test.Message.SomeEnum> becomes
C<Perl::Test::Message::SomeEnum>, ...).

=head2 MESSAGES

Each Protocol Buffer message is mapped to a generated Perl class.

Message classes don't have a parent class (but expose the methods
documented in L<Google::ProtocolBuffers::Dynamic::Message>, so you can
think of them as interface implementations, if you want), and provide
the field accessors documented below.

Message classes should not be subclassed: if you thread carefully you
might be able to get away with it, but in general they won't work
correctly (for example, overridden constructors/accessors won't be
called during serialization/deserialization).

Unless explicitly mapped, nested messages will be mapped as nested
packages (e.g. if C<package.Foo> is mapped to C<MyPack::Foo>,
C<package.Foo.Bar> will be mapped to C<MyPack::Foo::Bar>.

=head2 FIELDS

Each field is mapped to a set of accessors (documented below).

For each field, a constant with the field number is exposed to
Perl. For example the field C<optional string foo = 7> will result in
a constant equivalent to C<< use constant FOO_FIELD_NUMBER => 7 >>.

On Perls with 32-bit integers, C<int64>/C<uint64> fields with values
outside the representable range will be returned as L<Math::BigInt>
objects and can be set the same way. On Perls with 64-bit integers,
L<Math::BigInt> objects are accepted by setters but not returned by
getters (unless L</use_bigints> is used).

=head3 Scalar fields

For a field named C<foo> the message class will have a getter (C<<
$msg->get_foo >>), a setter (C<< $msg->set_foo($value) >>) a clear
method (C<< $msg->clear_foo >>) and an existence check (C<<
$msg->has_foo >>).

The getter returns the default value for the field (C<0> for numeric
fields, C<''> for string fields, C<undef> for message fields) if the
field is not set/has been cleared.

The setter performs coercion and type checks; for example setting an
integer field to a floating point value will truncate the decimal
part.

Setting a message field will take ownership of the passed-in hash/object.

The setters for enum fields check that the passed-in value is one of
the valid enum values, unless L</check_enum_values> is false.

=head3 Repeated fields

For a field named C<foo> the message class will have an item getter
(C<< $msg->get_foo($index) >>), an item setter (C<<
$msg->set_foo($index, $value) >>) and appender (C<<
$msg->add_foo($value) >>).

C<< $msg->foo_size >> returns the number of elements in the list, C<<
$msg->get_foo_list >> and C<< $msg->set_foo_list >> act as
getter/setter for the whole list, taking/returning an array reference.

=head3 Map fields

For a field named C<foo> the message class will have an item getter
(C<< $msg->get_foo($key) >>) and an item setter (C<<
$msg->set_foo($key, $value) >>).

C<< $msg->get_foo_map >> and C<< $msg->set_foo_map >> act as
getter/setter for the whole map, taking/returning a hash reference.

The C<< map<key, value> >> syntax is only available for C<proto3>, see
see L</implicit_maps> to emulate map support when using C<proto2>.

=head2 ENUMERATIONS

Enumeration values are passed around as integers.

As a convenience, each enumeration value is mapped to a Perl constant;
for example:

    enum Foo {
        BAR = 1;
        BAZ = 2;
    }

will result in constants equivalent to

    use constant {
        BAR => 1,
        BAZ => 2,
    };

If L</check_enum_values> is true (the default), trying to encode a
message containing an invalid enum value will result in an error,
trying to decode it will ignore the invalid value. If
L</check_enum_values> is false, invalid enum values are passed through
unchanged.

=head2 ONEOF

Oneof fields don't generate any specific accessors, however calling
the setter for one of the contained fields will clear the other fields
that are members of the same oneof.

=head2 EXTENSIONS

Extension fields are treated like other scalar/repeated fields, except
that accessor names are generated using the fully-qualified name of
the extension.

For example

    package test;

    // ...

    extend Message1 {
        optional int32 value = 102;
    }

    message Message2 {
        extend Message1 {
            optional Message2 extension2 = 101;
        }
        optional int32 value = 1;
    }

will create (scalar, in this case) accessors for fields named
C<test_value> and C<test_message2_extension2>.

In addition to per-field accessors, there is a set of generic
extension accessors (documented in
L<Google::ProtocolBuffers::Dynamic::Message/EXTENSION METHODS>).

=head2 SERVICES

Each service is mapped to a generated Perl class, with each method
mapped to a Perl method.

C<Google::ProtocolBuffers::Dynamic> does not provide an RPC
implementation, but supports generating a gRPC client using
L<Grpc::XS> (other implementations might be provided in the future).

In addition, there is a C<noop> service mapping mode that exposes
introspection information to simplify the mapping of external RPC
implementation on top of C<Google::ProtocolBuffers::Dynamic>.

The exact interface of mapped methods depends on the underlying RPC
implementation (for example, for L<Grpc::XS>, the return value is a
subclass of L<Grpc::Client::AbstractCall>) and whether the method uses
client/server streaming.

=head2 INTROSPECTION

All mapped entities provide access to an introspection object that
describes the Protocol Buffers object definition.

Each mapped message has a C<message_descriptor> class method returning a L<MessageDef|Google::ProtocolBuffers::Dynamic::Introspection/Google::ProtocolBuffers::Dynamic::MessageDef> object.

Each mapped enum has an C<enum_descriptor> class method returning an L<EnumDef|Google::ProtocolBuffers::Dynamic::Introspection/Google::ProtocolBuffers::Dynamic::EnumDef> object.

Each mapped service has a C<service_descriptor> class method returning a L<ServiceDef|Google::ProtocolBuffers::Dynamic::Introspection/Google::ProtocolBuffers::Dynamic::ServiceDef> object.

=head1 METHODS

This section describes methods used to load/map Protocol Buffer
messages, see above for the API of mapped messages.

The normal usage pattern is:

=over 4

=item create a new C<Google::ProtocolBuffers::Dynamic> instance

=item call L</load_string>/L</load_file> one or more times

=item call L</map> to map Protocol Buffer types to Perl packages

=back

L</map> is a wrapper around the various C<map_*> methods and
L</resolve_references>, so there should be no reason for using those
directly.

=head2 new

    $dynamic = Google::ProtocolBuffers::Dynamic->new;
    $dynamic = Google::ProtocolBuffers::Dynamic->new($root_directory);

When specified, C<$root_directory> is used as base for relative paths in
C<load_file>.

=head2 load_file

    $dynamic->load_file($file_path);

Loads the specified file, searching for it in the path passed to the
constructor.

=head2 load_string

    $dynamic->load_string($file_name, $string);

Parses message definitions from a string. The C<$file_name> parameter
is only used to report errors, and can be the empty string.

=head2 load_serialized_string

    $dynamic->load_serialized_string($string);

Parses message definitions from serialized descriptor data in the
format produced by C<protoc> C<--descriptor_set_out> option.

=head2 map

    # the 'options' key is optional
    $dynamic->map(
        # uses map_package_prefix
        { pb_prefix => $pb_prefix,prefix => $perl_prefix, options => $options },
        # uses map_message_prefix
        { message => $pb_message, prefix => $perl_prefix, options => $options },
        # uses map_package
        { package => $pb_package, prefix => $perl_prefix, options => $options },
        # uses map_message
        { message => $pb_message, to    => $perl_package, options => $options },
        # uses map_service
        { service => $pb_enum,    to    => $perl_package,
          options => {
              client_services => $mapping_mode,
              # other options
          },
    );

Applies the mappings in order and then calls L</resolve_references>.

Order is important because package mappings skip types already mapped
by previous mappings, but message/enum mappings throw an error if the
type has already been mapped.

For example, given:

    package test;

    message Foo { }
    message Bar { }

the following mapping:

    $dynamic->map(
        { message => 'test.Foo', to     => 'SomePackage::Foo' },
        { package => 'test'      prefix => 'OtherPackage' },
    );

maps C<test.Foo> to C<SomePackage::Foo> and C<test.Bar> to
C<OtherPackage::Bar> whereas

    $dynamic->map(
        { package => 'test'      prefix => 'OtherPackage' },
        { message => 'test.Foo', to     => 'SomePackage::Foo' },
    );

throws an error because C<test.Foo> has already been mapped (to
C<OtherPackage::Foo>) by the first mapping.

=head2 map_package_prefix

    $dynamic->map_package_prefix($pb_prefix, $perl_prefix);
    $dynamic->map_package_prefix($pb_prefix, $perl_prefix, $options);

Finds all types contained in ProtocolBuffers packages having the given
prefix and (recursively) maps them under the specified Perl package
prefix. It silently skips any already mapped types.

=head2 map_message_prefix

    $dynamic->map_message_prefix($pb_message, $perl_prefix);
    $dynamic->map_message_prefix($pb_message, $perl_prefix, $options);

Maps the specified message (recursively) under the Perl package prefix,
using the full package name of the message.  It silently skips any
already mapped types (although it will still recurse in to them).

In particular, this is quite useful to automatically map a message
that uses messages types defined in other packages.

You might also want to map a required message to a specified package
and then make sure other messages used by that message are mapped
elsewhere.  This can be achieved by mapping with a prefix after mapping
the message (since L</map_message_prefix> skips already-mapped stuff).

=head2 map_package

    $dynamic->map_package($pb_package, $perl_prefix);
    $dynamic->map_package($pb_package, $perl_prefix, $options);

Finds all types contained in the specified package and (recursively)
maps them under the specified Perl package prefix. It silently skips
any already mapped types.

=head2 map_message

    $dynamic->map_message($pb_message, $perl_package);
    $dynamic->map_message($pb_message, $perl_package, $options);

Maps the specified message to the specified Perl package and
(recursively) all contained types to inner packages. It throws an
error if the specified message has already been mapped but silently
skips any already mapped inner types.

=head2 map_enum

    $dynamic->map_enum($pb_enum, $perl_package);
    $dynamic->map_enum($pb_enum, $perl_package, $options);

Maps the specified enumeration to the specified Perl package. It
throws an error if the enumeration has already been mapped.

=head2 map_service

    $dynamic->map_service($pb_service, $perl_package, options => {
        client_services => $mapping_mode,
        # other options
    });

Maps the specified service to the specified Perl package. It
throws an error if the service has already been mapped.

=head2 resolve_references

    $dynamic->resolve_references;

Called implicitly by L</map>, must be called when using the lower-level
mapping functions. Resolves any message cross-references (e.g. fields
with message or enumeration types).

=head1 OPTIONS

Can be passed to the various mapping methods.

=head2 implicit_maps

C<proto3> provides a special C<< map<key, value> >> syntax to define
map fields:

    map<key-type, value-type> field = 1;

which is equivalent to the following C<proto2> definition

    message FieldEntry {
        optional key-type key = 1;
        optional value-type value = 2;
    }
    repeated FieldEntry field = 1;

C<Google::ProtocolBuffers::Dynamic> can detect when a map-like
structure has been defined using the C<proto2> syntax above and
provide the same semantics as a C<proto3> map (i.e. it will accept a
Perl hash when encoding and produce a Perl hash when decoding).

When C<implicit_maps> is enabled, messages with the following properties:

=over 4

=item message name ends in C<Entry>

=item it has an C<optional> field named C<key> with tag 1

=item it has an C<optional> field named C<value> with tag 2

=item it has exactly two fields, and no C<oneof> field

=item C<key> is not a message or enum field

=back

will be marked as map entries. Those rules do not check all the
restrictions that are in place for a C<proto3> map, and they might be
made stricter in the future.

=head2 use_bigints

Enabled by default on Perls with 32-bit integers, disabled by default
on Perls with 64-bit integers.

When enabled, values that don't fit in a 32-bit integer are returned as
L<Math::BigInt> objects.

=head2 check_required_fields

Enabled by default.

When enabled, checks that all required fields are present (for both
encoding and decoding).

=head2 encode_defaults

Disabled by default, it can't be enabled for C<proto3>.

When disabled, if the value of a field is equal to its default, the
field is not emitted during serialization, whether the value was
explicitly set or not.

When enabled, and a value was explicitly set for the field, the field
is always emitted.

=head2 explicit_defaults

Disabled by default.

When enabled, explicitly sets all missing scalar fields (but not oneof
members) to their default value when decoding.

=head2 check_enum_values

Enabled by default.

When enabled, invalid enum values throw an error in
getters/setters/encoding and are replaced with the default value for
that enum when decoding.

=head2 generic_extension_methods

Enabled by default.

Controls the generation of generic extension methods. Only disable
when there is a field named C<extension>.

=head2 accessor_style

Defaults to C<get_and_set>.

It controls the naming and interface of getters/setters created for
message fields. Accepted values are:

=over 4

=item get_and_set

Getter is prefixed with C<get_>, setter with C<set_>.

=item plain_and_set

Getter does not have any prefix, setter with C<set_>.

=item single_accessor

Generates a single method without a prefix that acts as a setter when
a value is passed, as a getter otherwise.

=back

=head2 client_services

Defaults to C<disable>.

=over 4

=item disable

Silently ignore all service definitions.

=item noop

Only provide introspection information for services (see L</INTROSPECTION>).

=item grpc_xs

Generate a client implementation using L<Grpc::XS>.

=back

=head2 decode_blessed

Enabled by default.

Return blessed objects when decoding. When disabled, plain hashes are
returned.

=head2 fail_ref_coercion

Throws an error when a non-overloaded reference is passsed as the value
of a non-message field.

Not available for Perl 5.12 or older.

=head1 KNOWN BUGS

When a field has the incorrect value, sometimes serialization performs
a coercion, sometimes it throws an error, but it should always be an
error.

Unknown fields are discarded on deserialization.

Proto3 support is only partial (support for well-known types, such as Any and Duration is missing).

See also the L<TODO|https://github.com/mbarbon/google-protobuf-dynamic/blob/master/TODO>

=head1 SEE ALSO

L<protoc-gen-perl-gpd>

L<Google Protocol Buffers page|https://developers.google.com/protocol-buffers>

L<uPB serialization/deserialization library|https://github.com/google/upb>

=cut

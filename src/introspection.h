#ifndef _GPD_XS_DYNAMIC_INTROSPECTION
#define _GPD_XS_DYNAMIC_INTROSPECTION

#include "perl_unpollute.h"

#include <string>

namespace google {
namespace protobuf {
class Message;
class Descriptor;
class MessageOptions;
class FieldDescriptor;
class FieldOptions;
class OneofDescriptor;
class OneofOptions;
class EnumDescriptor;
class EnumOptions;
class ServiceDescriptor;
class ServiceOptions;
class MethodDescriptor;
class MethodOptions;
class FileDescriptor;
class FileOptions;
class DescriptorPool;
class DynamicMessageFactory;
}
}

#include "thx_member.h"
#include "perl_unpollute.h"

namespace gpd {
namespace intr {

class DescriptorOptionsWrapper;

// same values as in upb/def.h, for backwards compatibility/stability
enum ValueType {
    VALUE_BOOL     = 1,
    VALUE_FLOAT    = 2,
    VALUE_INT32    = 3,
    VALUE_UINT32   = 4,
    VALUE_ENUM     = 5,  /* Enum values are int32. */
    VALUE_STRING   = 6,
    VALUE_BYTES    = 7,
    VALUE_MESSAGE  = 8,
    VALUE_DOUBLE   = 9,
    VALUE_INT64    = 10,
    VALUE_UINT64   = 11,
    VALUE_INVALID  = -1
};

// helpers for uPB methods that don't have a direct equivalent in the descriptor API
ValueType field_value_type(const google::protobuf::FieldDescriptor *field_def);
bool field_is_primitive(const google::protobuf::FieldDescriptor *field_def);
SV *field_default_value(pTHX_ const google::protobuf::FieldDescriptor *field_def);

int enum_default_value(const google::protobuf::EnumDescriptor *enum_def);

const google::protobuf::FieldDescriptor *oneof_find_field_by_number(const google::protobuf::OneofDescriptor *oneof_def, int number);
const google::protobuf::FieldDescriptor *oneof_find_field_by_name(const google::protobuf::OneofDescriptor *oneof_def, const std::string &name);

bool options_make_wrapper(const google::protobuf::DescriptorPool *descriptor_pool, const google::protobuf::Message &options_def, google::protobuf::DynamicMessageFactory **factory, google::protobuf::Message **options_dyn);

template<class W, class D>
W *options_make_wrapper(pTHX_ const D *descriptor) {
    google::protobuf::DynamicMessageFactory *factory;
    google::protobuf::Message *options_dyn;

    if (options_make_wrapper(descriptor->file()->pool(), descriptor->options(), &factory, &options_dyn)) {
        return new W(aTHX_ factory, options_dyn, &descriptor->options());
    }

    return NULL;
}

// helper function for introspection.xsp
void define_constant(pTHX_ const char *name, const char *tag, int value);

class DescriptorOptionsWrapper {
public:
    DescriptorOptionsWrapper(pTHX_ google::protobuf::DynamicMessageFactory *_factory, const google::protobuf::Message *_options);
    virtual ~DescriptorOptionsWrapper();

    SV *custom_option_by_name(const std::string &name);
    SV *custom_option_by_number(int number);

    bool get_attribute(CV *autoload_cv, SV **retval);

private:
    SV *get_field(const google::protobuf::FieldDescriptor *field);

    DECL_THX_MEMBER;
    google::protobuf::DynamicMessageFactory *factory;
    const google::protobuf::Message *options;
};

#define OPTIONS_WRAPPER_SCAFFOLDING(OPT_TYPE)                               \
    private:                                                                \
        const google::protobuf::OPT_TYPE ## Options *options;               \
    public:                                                                 \
        OPT_TYPE ## OptionsWrapper(pTHX_                                    \
                google::protobuf::DynamicMessageFactory *_factory,          \
                google::protobuf::Message *_options_dyn,                    \
                const google::protobuf::OPT_TYPE ## Options *_options) :    \
            DescriptorOptionsWrapper(aTHX_ _factory, _options_dyn),         \
            options(_options) {                                             \
        }

class MessageOptionsWrapper : public DescriptorOptionsWrapper {
    OPTIONS_WRAPPER_SCAFFOLDING(Message);

    bool deprecated();
};

class FieldOptionsWrapper : public DescriptorOptionsWrapper {
    OPTIONS_WRAPPER_SCAFFOLDING(Field);

    bool deprecated();
};

class OneofOptionsWrapper : public DescriptorOptionsWrapper {
    OPTIONS_WRAPPER_SCAFFOLDING(Oneof);
};

class EnumOptionsWrapper : public DescriptorOptionsWrapper {
    OPTIONS_WRAPPER_SCAFFOLDING(Enum);

    bool deprecated();
};

class ServiceOptionsWrapper : public DescriptorOptionsWrapper {
    OPTIONS_WRAPPER_SCAFFOLDING(Service);

    bool deprecated();
};

class MethodOptionsWrapper : public DescriptorOptionsWrapper {
    OPTIONS_WRAPPER_SCAFFOLDING(Method);

    bool deprecated();
};

class FileOptionsWrapper : public DescriptorOptionsWrapper {
    OPTIONS_WRAPPER_SCAFFOLDING(File);

    bool deprecated();
};

#undef OPTIONS_WRAPPER_SCAFFOLDING

}
}

// type aliases to make XS++-generated code work
namespace Google {
    namespace ProtocolBuffers {
        namespace Dynamic {
            typedef google::protobuf::Descriptor MessageDef;
            typedef gpd::intr::MessageOptionsWrapper MessageOptionsDef;
            typedef google::protobuf::FieldDescriptor FieldDef;
            typedef gpd::intr::FieldOptionsWrapper FieldOptionsDef;
            typedef google::protobuf::OneofDescriptor OneofDef;
            typedef gpd::intr::OneofOptionsWrapper OneofOptionsDef;
            typedef google::protobuf::EnumDescriptor EnumDef;
            typedef gpd::intr::EnumOptionsWrapper EnumOptionsDef;
            typedef google::protobuf::ServiceDescriptor ServiceDef;
            typedef gpd::intr::ServiceOptionsWrapper ServiceOptionsDef;
            typedef google::protobuf::MethodDescriptor MethodDef;
            typedef gpd::intr::MethodOptionsWrapper MethodOptionsDef;
            typedef google::protobuf::FileDescriptor FileDef;
            typedef gpd::intr::FileOptionsWrapper FileOptionsDef;
        }
    }
}

#endif

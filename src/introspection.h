#ifndef _GPD_XS_DYNAMIC_INTROSPECTION
#define _GPD_XS_DYNAMIC_INTROSPECTION

#include "perl_unpollute.h"

#include <string>

namespace google {
namespace protobuf {
class Descriptor;
class FieldDescriptor;
class OneofDescriptor;
class EnumDescriptor;
class ServiceDescriptor;
class MethodDescriptor;
}
}

#include "EXTERN.h"
#include "perl.h"

namespace gpd {
namespace intr {

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

// helper function for introspection.xsp
void define_constant(pTHX_ const char *name, const char *tag, int value);

}
}

// type aliases to make XS++-generated code work
namespace Google {
    namespace ProtocolBuffers {
        namespace Dynamic {
            typedef google::protobuf::Descriptor MessageDef;
            typedef google::protobuf::FieldDescriptor FieldDef;
            typedef google::protobuf::OneofDescriptor OneofDef;
            typedef google::protobuf::EnumDescriptor EnumDef;
            typedef google::protobuf::ServiceDescriptor ServiceDef;
            typedef google::protobuf::MethodDescriptor MethodDef;
        }
    }
}

#endif

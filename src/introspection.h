#ifndef _GPD_XS_DYNAMIC_INTROSPECTION
#define _GPD_XS_DYNAMIC_INTROSPECTION

#include "perl_unpollute.h"

#include <upb/def.h>

#include "EXTERN.h"
#include "perl.h"

namespace gpd {
namespace intr {

// equivalent to FieldDescriptor::has_presence() in src/google/protobuf/descriptor.h
inline bool has_presence(const upb::FieldDef* field_def) {
    if (field_def->IsSequence())
        return false;
    if (field_def->IsSubMessage() || field_def->containing_oneof())
        return true;
    if (field_def->containing_type() && field_def->containing_type()->syntax() == UPB_SYNTAX_PROTO2)
        return true;
    return false;
}

SV *field_default_value(pTHX_ const upb::FieldDef *field_def);

// helper function for introspection.xsp
void define_constant(pTHX_ const char *name, const char *tag, int value);

}
}

#ifdef _GPD_XS_DYNAMIC_IN_XS

// type aliases to make XS++-generated code work
namespace Google {
    namespace ProtocolBuffers {
        namespace Dynamic {
            typedef upb::MessageDef MessageDef;
            typedef upb::FieldDef FieldDef;
            typedef upb::OneofDef OneofDef;
            typedef upb::EnumDef EnumDef;
            typedef gpd::ServiceDef ServiceDef;
            typedef gpd::MethodDef MethodDef;
        }
    }
}

#endif

#endif

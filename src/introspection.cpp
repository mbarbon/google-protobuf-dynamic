#include "introspection.h"

#include <google/protobuf/descriptor.h>

using namespace gpd;
using namespace gpd::intr;
using namespace google::protobuf;

ValueType gpd::intr::field_value_type(const FieldDescriptor *field_def) {
    using CppType = FieldDescriptor::CppType;
    using Type = FieldDescriptor::Type;

    switch (field_def->cpp_type()) {
    case CppType::CPPTYPE_INT32:
        return VALUE_INT32;
    case CppType::CPPTYPE_INT64:
        return VALUE_INT64;
    case CppType::CPPTYPE_UINT32:
        return VALUE_UINT32;
    case CppType::CPPTYPE_UINT64:
        return VALUE_UINT64;
    case CppType::CPPTYPE_DOUBLE:
        return VALUE_DOUBLE;
    case CppType::CPPTYPE_FLOAT:
        return VALUE_FLOAT;
    case CppType::CPPTYPE_BOOL:
        return VALUE_BOOL;
    case CppType::CPPTYPE_ENUM:
        return VALUE_ENUM;
    case CppType::CPPTYPE_STRING:
        return field_def->type() == Type::TYPE_STRING ?
            VALUE_STRING : VALUE_BYTES;;
    case CppType::CPPTYPE_MESSAGE:
        return VALUE_MESSAGE;
    }
    return VALUE_INVALID; // should not happen
}

bool gpd::intr::field_is_primitive(const FieldDescriptor *field_def) {
    using CppType = FieldDescriptor::CppType;
    using Type = FieldDescriptor::Type;

    switch (field_def->cpp_type()) {
    case CppType::CPPTYPE_STRING:
    case CppType::CPPTYPE_MESSAGE:
        return false;
    default:
        return true;
    }
}

SV *gpd::intr::field_default_value(pTHX_ const FieldDescriptor *field_def) {
    using CppType = FieldDescriptor::CppType;
    using Type = FieldDescriptor::Type;

    switch (field_def->cpp_type()) {
    case CppType::CPPTYPE_FLOAT:
        return newSVnv(field_def->default_value_float());
    case CppType::CPPTYPE_DOUBLE:
        return newSVnv(field_def->default_value_double());
    case CppType::CPPTYPE_BOOL:
        return field_def->default_value_bool() ? &PL_sv_yes : &PL_sv_no;
    case CppType::CPPTYPE_STRING: {
        const std::string &value = field_def->default_value_string();
        SV *result = newSVpv(value.data(), value.length());

        if (field_def->type() == Type::TYPE_STRING)
            SvUTF8_on(result);

        return result;
    }
    case CppType::CPPTYPE_MESSAGE:
        return &PL_sv_undef;
    case CppType::CPPTYPE_ENUM: {
        auto enumvalue_def = field_def->default_value_enum();

        return enumvalue_def != NULL ? newSViv(enumvalue_def->number()) : 0;
    }
    case CppType::CPPTYPE_INT32:
        return newSViv(field_def->default_value_int32());
    case CppType::CPPTYPE_INT64:
        return newSViv(field_def->default_value_int64());
    case CppType::CPPTYPE_UINT32:
        return newSVuv(field_def->default_value_uint32());
    case CppType::CPPTYPE_UINT64:
        return newSVuv(field_def->default_value_uint64());
    }
    return NULL; // should not happen
}

int gpd::intr::enum_default_value(const EnumDescriptor *enum_def) {
    if (enum_def->value_count() > 0) {
        return enum_def->value(0)->number();
    }

    return 0;
}

const FieldDescriptor *gpd::intr::oneof_find_field_by_number(const OneofDescriptor *oneof_def, int number) {
    const FieldDescriptor *field_def = oneof_def->containing_type()->FindFieldByNumber(number);

    return field_def->containing_oneof() == oneof_def ? field_def : NULL;
}

const FieldDescriptor *gpd::intr::oneof_find_field_by_name(const OneofDescriptor *oneof_def, const std::string &name) {
    const FieldDescriptor *field_def = oneof_def->containing_type()->FindFieldByName(name);

    return field_def->containing_oneof() == oneof_def ? field_def : NULL;
}

#if PERL_VERSION < 10
    #undef  newCONSTSUB
    #define newCONSTSUB(a, b,c) Perl_newCONSTSUB(aTHX_ a, const_cast<char *>(b), c)
#endif

void gpd::intr::define_constant(pTHX_ const char *name, const char *tag, int value) {
    HV *stash = gv_stashpv("Google::ProtocolBuffers::Dynamic", GV_ADD);
    AV *export_ok = get_av("Google::ProtocolBuffers::Dynamic::EXPORT_OK", GV_ADD);
    HV *export_tags = get_hv("Google::ProtocolBuffers::Dynamic::EXPORT_TAGS", GV_ADD);
    SV **tag_arrayref = hv_fetch(export_tags, tag, strlen(tag), 1);

    newCONSTSUB(stash, name, newSViv(value));

    if (!SvOK(*tag_arrayref)) {
        sv_upgrade(*tag_arrayref, SVt_RV);
        SvROK_on(*tag_arrayref);
        SvRV_set(*tag_arrayref, (SV *) newAV());
    }
    AV *tag_array = (AV *) SvRV(*tag_arrayref);

    av_push(export_ok, newSVpv(name, 0));
    av_push(tag_array, newSVpv(name, 0));
}

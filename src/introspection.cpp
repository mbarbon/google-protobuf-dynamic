#include "introspection.h"

SV *gpd::intr::field_default_value(pTHX_ const upb::FieldDef *field_def) {
     switch (field_def->type()) {
     case UPB_TYPE_FLOAT:
         return newSVnv(field_def->default_float());
     case UPB_TYPE_DOUBLE:
         return newSVnv(field_def->default_double());
     case UPB_TYPE_BOOL:
         return field_def->default_bool() ? &PL_sv_yes : &PL_sv_no;
     case UPB_TYPE_STRING:
     case UPB_TYPE_BYTES: {
          size_t length;
          const char *bytes = field_def->default_string(&length);
          SV *result = newSVpv(bytes, length);

          if (field_def->type() == UPB_TYPE_STRING)
             SvUTF8_on(result);

          return result;
     }
     case UPB_TYPE_MESSAGE:
         return &PL_sv_undef;
     case UPB_TYPE_ENUM:
     case UPB_TYPE_INT32:
         return newSViv(field_def->default_int32());
     case UPB_TYPE_INT64:
         return newSViv(field_def->default_int64());
     case UPB_TYPE_UINT32:
         return newSVuv(field_def->default_uint32());
     case UPB_TYPE_UINT64:
         return newSVuv(field_def->default_uint64());
     }
     return NULL; // should not happen
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

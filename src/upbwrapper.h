#ifndef _GPD_XS_DYNAMIC_UPBWRAPPER
#define _GPD_XS_DYNAMIC_UPBWRAPPER

#undef New

#if PERL_VERSION < 10
    #undef  newCONSTSUB
    #define newCONSTSUB(a, b,c) Perl_newCONSTSUB(aTHX_ a, const_cast<char *>(b), c)
#endif

#include <upb/def.h>

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

namespace gpd {
    inline void define_constant(pTHX_ const char *name, const char *tag, int value) {
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
}

#endif

#ifndef _GPD_XS_FIELDITERATOR_INCLUDED
#define _GPD_XS_FIELDITERATOR_INCLUDED

#include "perl_unpollute.h"

#include "thx_member.h"

#include "fieldmap.h"

#include "EXTERN.h"
#include "perl.h"

#include <vector>

namespace gpd {

HE *hv_fetch_ent_tied(pTHX_ HV *hv, SV *name, I32 lval, U32 hash);

template<class F>
struct FieldIterator {
    DECL_THX_MEMBER;
    HV *hv;
    bool tied;
    typename std::vector<F>::const_iterator it, en;
    HE *he;

    static const bool direct_required_check = true;

    FieldIterator(pTHX_ HV *_hv, const std::vector<F> &fields, const FieldMap<F> &field_map) {
        SET_THX_MEMBER;
        hv = _hv;
        tied = SvTIED_mg((SV *) hv, PERL_MAGIC_tied);
        it = fields.begin();
        en = fields.end();
    }

    void setup() {
    }

    bool has_next() {
        return it != en;
    }

    void next() {
        ++it;
    }

    bool setup_next() {
        he = tied ? hv_fetch_ent_tied(aTHX_ hv, it->name, 0, it->name_hash) :
                    hv_fetch_ent(hv, it->name, 0, it->name_hash);

        return he != NULL;
    }

    SV *field_context() {
        return it->name;
    }

    const F *field() {
        return &*it;
    }

    SV *value() {
        return HeVAL(he);
    }
};

template<class F>
struct HashAPIIterator {
    DECL_THX_MEMBER;
    HV *hv;
    HE *he;
    const FieldMap<F> &field_map;
    const F *current_field;

    static const bool direct_required_check = false;

    HashAPIIterator(pTHX_ HV *_hv, const std::vector<F> &fields, const FieldMap<F> &_field_map) : field_map(_field_map) {
        SET_THX_MEMBER;
        hv = _hv;
        hv_iterinit(hv);
    }

    void setup() {
    }

    bool has_next() {
        he = hv_iternext(hv);

        return he != NULL;
    }

    void next() { }

    bool setup_next() {
        current_field = field_map.find_by_name(aTHX_ he);

        return current_field != NULL;
    }

    HE *field_context() {
        return he;
    }

    const F *field() {
        return current_field;
    }

    SV *value() {
        return HeVAL(he);
    }
};

}

#endif

#include "fielditerator.h"

HE *gpd::hv_fetch_ent_tied(pTHX_ HV *hv, SV *name, I32 lval, U32 hash) {
    if (!hv_exists_ent(hv, name, hash))
        return NULL;

    return hv_fetch_ent(hv, name, lval, hash);
}

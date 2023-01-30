#include "transform.h"

#include "unordered_map.h"

using namespace gpd::transform;

DecoderTransform::DecoderTransform(CDecoderTransform _c_transform) :
        c_transform(_c_transform),
        c_transform_fieldtable(NULL),
        perl_transform(NULL)
{}

DecoderTransform::DecoderTransform(CDecoderTransformFieldtable _c_transform_fieldtable) :
        c_transform(NULL),
        c_transform_fieldtable(_c_transform_fieldtable),
        perl_transform(NULL)
{}

DecoderTransform::DecoderTransform(SV *_perl_transform) :
        c_transform(NULL),
        c_transform_fieldtable(NULL),
        perl_transform(_perl_transform)
{}

void DecoderTransform::destroy(pTHX) {
    SvREFCNT_dec(perl_transform);
    delete this;
}

void DecoderTransform::transform(pTHX_ SV *target) const {
    if (c_transform) {
        c_transform(aTHX_ target);
    } else if (perl_transform) {
        dSP;

        PUSHMARK(SP);
        XPUSHs(target);
        PUTBACK;

        // return value is always 1 because of G_SCALAR
        call_sv(perl_transform, G_VOID|G_DISCARD);
    } else {
        croak("Internal error: transform function not provided");
    }
}

void DecoderTransform::transform_fieldtable(pTHX_ SV *target, Fieldtable *fieldtable) const {
    if (c_transform_fieldtable) {
        c_transform_fieldtable(aTHX_ target, fieldtable);
    } else {
        croak("Internal error: fieldtable transform function not provided");
    }
}

DecoderTransformQueue::DecoderTransformQueue(pTHX) {
    SET_THX_MEMBER;
}

DecoderTransformQueue::~DecoderTransformQueue() {
    clear();
}

void DecoderTransformQueue::static_clear(DecoderTransformQueue *queue) {
    queue->clear();
}

void DecoderTransformQueue::clear() {
    for (std::vector<PendingTransform>::iterator it = pending_transforms.begin(), en = pending_transforms.end(); it != en; ++it) {
        SvREFCNT_dec(it->target);
    }

    pending_transforms.clear();

    for (std::vector<Fieldtable::Entry>::iterator it = fieldtable.begin(), en = fieldtable.end(); it != en; ++it) {
        SvREFCNT_dec(it->value);
    }

    fieldtable.clear();
}

size_t DecoderTransformQueue::add_transform(SV *target, const DecoderTransform *message_transform, const DecoderTransform *field_transform) {
    if (field_transform != NULL) {
        pending_transforms.push_back(PendingTransform(SvREFCNT_inc(target), field_transform));
    } else if (message_transform != NULL) {
        pending_transforms.push_back(PendingTransform(SvREFCNT_inc(target), message_transform));
    }

    return pending_transforms.size() - 1;
}

void DecoderTransformQueue::finish_add_transform(size_t index, int size, Fieldtable::Entry *entries) {
    int fieldtable_offset = fieldtable.size();

    fieldtable.insert(fieldtable.end(), entries, entries + size);

    pending_transforms[index].fieldtable_offset = fieldtable_offset;
    pending_transforms[index].fieldtable_size = size;
}

void DecoderTransformQueue::apply_transforms() {
    STD_TR1::unordered_set<SV *> already_mapped;
    Fieldtable table;

    for (std::vector<PendingTransform>::reverse_iterator it = pending_transforms.rbegin(), en = pending_transforms.rend(); it != en; ++it) {
        SV *target = it->target;
        // this block should only be entered when decoding the concatenation
        // of two protocol buffer messahes
        //
        // if an SV has refcount 2 (1 in the decoded struct, 1 in the
        // transform list) it can't be in the transform list multiple times
        if (SvREFCNT(target) > 2) {
            if (already_mapped.find(target) != already_mapped.end())
                continue;
            already_mapped.insert(target);
        }

        if (it->fieldtable_offset == -1) {
            it->transform->transform(aTHX_ target);
        } else {
            table.size = it->fieldtable_size;
            table.entries = &fieldtable[it->fieldtable_offset];

            it->transform->transform_fieldtable(aTHX_ target, &table);
        }
    }
}

// the only use for this transform is to be able to test fieldtable trnasformations
void gpd::transform::fieldtable_debug_transform(pTHX_ SV *target, Fieldtable *fieldtable) {
    AV *res = newAV();

    SvUPGRADE(target, SVt_RV);
    SvROK_on(target);
    SvRV_set(target, (SV *) res);

    for (int i = 0; i < fieldtable->size; ++i) {
        AV *item = newAV();

        av_push(item, newSViv(fieldtable->entries[i].field));
        av_push(item, fieldtable->entries[i].value);

        fieldtable->entries[i].value = NULL;

        av_push(res, newRV_noinc((SV *) item));
    }
}

// the only use for this transform is to be able to benchmark fieldtable trnasformations overhead
void gpd::transform::fieldtable_profile_transform(pTHX_ SV *target, Fieldtable *fieldtable) {
    Fieldtable::Entry *entry = fieldtable->entries;

    sv_setsv(target, entry->value);
    SvREFCNT_dec(entry->value);
    entry->value = NULL;
}


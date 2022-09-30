#include "transform.h"

#include "unordered_map.h"

DecoderTransform::DecoderTransform(CDecoderTransform function) :
        c_function(function),
        perl_function(NULL)
{}

DecoderTransform::DecoderTransform(SV *function) :
        c_function(NULL),
        perl_function(function)
{}

void DecoderTransform::transform(pTHX_ SV *target) const {
    if (c_function) {
        c_function(aTHX_ target);
    } else {
        dSP;

        PUSHMARK(SP);
        XPUSHs(target);
        PUTBACK;

        // return value is always 1 because of G_SCALAR
        call_sv(perl_function, G_VOID|G_DISCARD);
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
}

void DecoderTransformQueue::add_transform(SV *target, const DecoderTransform *message_transform, const DecoderTransform *field_transform) {
    if (field_transform != NULL) {
        pending_transforms.push_back(PendingTransform(SvREFCNT_inc(target), field_transform));
    } else if (message_transform != NULL) {
        pending_transforms.push_back(PendingTransform(SvREFCNT_inc(target), message_transform));
    }
}

void DecoderTransformQueue::apply_transforms() {
    STD_TR1::unordered_set<SV *> already_mapped;

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

        it->transform->transform(aTHX_ target);
    }
}

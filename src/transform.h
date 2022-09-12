#ifndef _GPD_XS_TRANSFORM_INCLUDED
#define _GPD_XS_TRANSFORM_INCLUDED

#include "perl_unpollute.h"

#include "EXTERN.h"
#include "perl.h"
#include "ppport.h"

#include "thx_member.h"

#include <vector>

typedef SV *(*CDecoderTransform)(pTHX_ SV *target);

class DecoderTransform {
public:
    DecoderTransform(CDecoderTransform function);
    DecoderTransform(SV *function);

    SV *transform(pTHX_ SV *target) const;

private:
    CDecoderTransform c_function;
    SV *perl_function;
};

class DecoderTransformQueue {
    struct PendingTransform {
        const DecoderTransform *transform;
        SV *target;

        PendingTransform(SV *trg, const DecoderTransform *trf) {
            target = trg;
            transform = trf;
        }
    };

public:
    DecoderTransformQueue(pTHX);
    ~DecoderTransformQueue();

    void clear();
    void add_transform(SV *target, const DecoderTransform *message_transform, const DecoderTransform *field_transform);
    void apply_transforms();

    static void static_clear(DecoderTransformQueue *queue);

private:
    DECL_THX_MEMBER;
    std::vector<PendingTransform> pending_transforms;
};

#endif

#ifndef _GPD_XS_TRANSFORM_INCLUDED
#define _GPD_XS_TRANSFORM_INCLUDED

#include "perl_unpollute.h"

#include "EXTERN.h"
#include "perl.h"
#include "ppport.h"

#include "thx_member.h"

#include <vector>
#include <list>

namespace gpd {
namespace transform {

struct Fieldtable {
    struct Entry {
        unsigned int field;
        SV *value;

        Entry(unsigned int _field, SV *_value) {
            field = _field;
            value = _value;
        }
    };

    int size;
    Entry *entries;
};

typedef void (*CDecoderTransform)(pTHX_ SV *target);
typedef void (*CDecoderTransformFieldtable)(pTHX_ SV *target, Fieldtable *fieldtable);

class DecoderTransform {
public:
    DecoderTransform(CDecoderTransform c_transfrom);
    DecoderTransform(CDecoderTransformFieldtable c_transform_fieldtable);
    DecoderTransform(SV *perl_transform);

    void transform(pTHX_ SV *target) const;
    void transform_fieldtable(pTHX_ SV *target, Fieldtable *fieldtable) const;

private:
    CDecoderTransform c_transform;
    CDecoderTransformFieldtable c_transform_fieldtable;
    SV *perl_transform;
};

class DecoderTransformQueue {
    struct PendingTransform {
        const DecoderTransform *transform;
        SV *target;
        int fieldtable_offset;
        int fieldtable_size;

        PendingTransform(SV *_target, const DecoderTransform *_transform) {
            target = _target;
            transform = _transform;
            fieldtable_offset = -1;
        }
    };

public:
    DecoderTransformQueue(pTHX);
    ~DecoderTransformQueue();

    void clear();
    size_t add_transform(SV *target, const DecoderTransform *message_transform, const DecoderTransform *field_transform);
    void finish_add_transform(size_t index, int size, Fieldtable::Entry *entries);
    void apply_transforms();

    static void static_clear(DecoderTransformQueue *queue);

private:
    DECL_THX_MEMBER;
    std::vector<PendingTransform> pending_transforms;
    std::vector<Fieldtable::Entry> fieldtable;
};

void fieldtable_debug_transform(pTHX_ SV *target, Fieldtable *fieldtable);

}
}

#endif

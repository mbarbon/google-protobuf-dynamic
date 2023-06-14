#ifndef _GPD_XS_TRANSFORM_INCLUDED
#define _GPD_XS_TRANSFORM_INCLUDED

#include "perl_unpollute.h"

#include "EXTERN.h"
#include "perl.h"
#include "ppport.h"

#include "thx_member.h"
#include "mapper_context.h"

#include <vector>
#include <list>

namespace gpd {
namespace transform {

struct DecoderFieldtable {
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

struct EncoderFieldtable {
    struct Entry {
        const char *name;
        unsigned int field;
        SV *value;
    };

    int size;
    Entry *entries;
};

struct UnknownFieldContext {
    int size;
    const gpd::MapperContext::ExternalItem *const *mapper_context;
};

typedef void (*CDecoderTransform)(pTHX_ SV *target);
typedef void (*CDecoderTransformFieldtable)(pTHX_ SV *target, DecoderFieldtable *fieldtable);
typedef void (*CEncoderTransform)(pTHX_ SV *target, SV *value);
typedef void (*CEncoderTransformFieldtable)(pTHX_ EncoderFieldtable **fieldtable, SV *value);
typedef void (*CUnknownFieldTransform)(pTHX_ UnknownFieldContext *field_context, SV *value);

class DecoderTransform {
public:
    DecoderTransform(CDecoderTransform c_transfrom);
    DecoderTransform(CDecoderTransformFieldtable c_transform_fieldtable);
    DecoderTransform(SV *perl_transform);

    // clear and delete this object, it is needed just to avoid
    // having a THX member in this object
    void destroy(pTHX);

    void transform(pTHX_ SV *target) const;
    void transform_fieldtable(pTHX_ SV *target, DecoderFieldtable *fieldtable) const;

private:
    // private to make sure deletion goes through destroy()
    ~DecoderTransform() {}

    CDecoderTransform c_transform;
    CDecoderTransformFieldtable c_transform_fieldtable;
    SV *perl_transform;
};

class EncoderTransform {
public:
    EncoderTransform(CEncoderTransform c_transform);
    EncoderTransform(CEncoderTransformFieldtable c_transform_fieldtable);
    EncoderTransform(SV * perl_transform);

    // clear and delete this object, it is needed just to avoid
    // having a THX member in this object
    void destroy(pTHX);

    void transform(pTHX_ SV *target, SV *value) const;
    void transform_fieldtable(pTHX_ EncoderFieldtable **target, SV *value) const;

private:
    // private to make sure deletion goes through destroy()
    ~EncoderTransform() {}

    CEncoderTransform c_transform;
    CEncoderTransformFieldtable c_transform_fieldtable;
    SV *perl_transform;
};

class UnknownFieldTransform {
public:
    UnknownFieldTransform(CUnknownFieldTransform c_transform);

    // clear and delete this object, it is needed just to avoid
    // having a THX member in this object
    void destroy(pTHX);

    void transform(pTHX_ UnknownFieldContext *field_context, SV *value) const {
        c_transform(aTHX_ field_context, value);
    }

private:
    // private to make sure deletion goes through destroy()
    ~UnknownFieldTransform() {}

    CUnknownFieldTransform c_transform;
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
    void finish_add_transform(size_t index, int size, DecoderFieldtable::Entry *entries);
    void apply_transforms();

    static void static_clear(DecoderTransformQueue *queue);

private:
    DECL_THX_MEMBER;
    std::vector<PendingTransform> pending_transforms;
    std::vector<DecoderFieldtable::Entry> fieldtable;
};

void fieldtable_debug_decoder_transform(pTHX_ SV *target, DecoderFieldtable *fieldtable);
void fieldtable_profile_decoder_transform(pTHX_ SV *target, DecoderFieldtable *fieldtable);
void fieldtable_debug_encoder_transform(pTHX_ EncoderFieldtable **fieldtable, SV *value);
void fieldtable_debug_encoder_unknown_fields(pTHX_ UnknownFieldContext *field_context, SV *value);
AV *fieldtable_debug_encoder_unknown_fields_get();

}
}

#endif

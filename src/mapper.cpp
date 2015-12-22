#include "mapper.h"

#undef New

#include <google/protobuf/repeated_field.h>

using namespace gpd;
using namespace std;
using namespace google::protobuf;

const Mapper *Mapper::mapper_for_descriptor(const google::protobuf::Descriptor *descriptor) const {
    InnerMessagesMap::const_iterator it = descriptor_map.find(descriptor);
    if (it == descriptor_map.end())
        croak("Unable to find mapper for type %s", descriptor->full_name().c_str());

    return it->second;
}

void Mapper::decode_to_perl(const Message *message, SV *target) const {
    const Descriptor *descriptor = message->GetDescriptor();
    HV *hv = newHV();

    sv_upgrade(target, SVt_RV);
    SvROK_on(target);
    SvRV_set(target, (SV *) hv);

    for (int i = 0, max = descriptor->field_count(); i < max; ++i) {
        const FieldDescriptor *fd = descriptor->field(i);
        const string &name = fd->name();
        SV **svp = hv_fetch(hv, name.data(), name.size(), 1);

        if (fd->is_repeated())
            decode_to_perl_array(message, fd, *svp);
        else
            decode_to_perl(message, fd, *svp);
    }
}

void Mapper::decode_to_perl(const Message *message, const FieldDescriptor *fd, SV *target) const {
    switch (fd->type()) {
    case FieldDescriptor::TYPE_DOUBLE:
        sv_setnv(target, reflection->GetDouble(*message, fd));
        break;
    case FieldDescriptor::TYPE_FLOAT:
        sv_setnv(target, reflection->GetFloat(*message, fd));
        break;
    case FieldDescriptor::TYPE_SINT64:
    case FieldDescriptor::TYPE_SFIXED64:
    case FieldDescriptor::TYPE_INT64:
        sv_setiv(target, reflection->GetInt64(*message, fd));
        break;
    case FieldDescriptor::TYPE_FIXED64:
    case FieldDescriptor::TYPE_UINT64:
        sv_setuv(target, reflection->GetUInt64(*message, fd));
        break;
    case FieldDescriptor::TYPE_SINT32:
    case FieldDescriptor::TYPE_SFIXED32:
    case FieldDescriptor::TYPE_INT32:
        sv_setiv(target, reflection->GetInt32(*message, fd));
        break;
    case FieldDescriptor::TYPE_BOOL: {
        if (reflection->GetBool(*message, fd)) {
            sv_setiv(target, 1);
        } else {
            sv_upgrade(target, SVt_PV);
            SvCUR_set(target, 0);
            SvPOK_on(target);
        }
    }
        break;
    case FieldDescriptor::TYPE_STRING: {
        string s;
        const string &str = reflection->GetStringReference(*message, fd, &s);
        sv_setpvn(target, str.data(), str.size());
        SvUTF8_on(target);
    }
        break;
    case FieldDescriptor::TYPE_GROUP:
    case FieldDescriptor::TYPE_MESSAGE: {
        const Message &value = reflection->GetMessage(*message, fd);

        mapper_for_descriptor(value.GetDescriptor())->decode_to_perl(&value, target);
    }
        break;
    case FieldDescriptor::TYPE_BYTES: {
        string s;
        const string &str = reflection->GetStringReference(*message, fd, &s);
        sv_setpvn(target, str.data(), str.size());
    }
        break;
    case FieldDescriptor::TYPE_FIXED32:
    case FieldDescriptor::TYPE_UINT32:
        sv_setuv(target, reflection->GetUInt32(*message, fd));
        break;
    case FieldDescriptor::TYPE_ENUM:
        sv_setiv(target, reflection->GetEnum(*message, fd)->number());
        break;
    default:
        croak("Unhandled FieldDescriptor::Type value %d", fd->type());
    }
}

namespace {
    class NVSetter {
    public:
        void operator()(SV *target, NV value) const { sv_setnv(target, value); }
    };

    class IVSetter {
    public:
        void operator()(SV *target, IV value) const { sv_setiv(target, value); }
    };

    class UVSetter {
    public:
        void operator()(SV *target, UV value) const { sv_setuv(target, value); }
    };

    class StringSetter {
    public:
        void operator()(SV *target, const string &value) const {
            sv_setpvn(target, value.data(), value.size());
            SvUTF8_on(target);
        }
    };

    class BytesSetter {
    public:
        void operator()(SV *target, const string &value) const {
            sv_setpvn(target, value.data(), value.size());
        }
    };

    class BoolSetter {
    public:
        void operator()(SV *target, bool value) const {
            if (value) {
                sv_setiv(target, 1);
            } else {
                sv_upgrade(target, SVt_PV);
                SvCUR_set(target, 0);
                SvPOK_on(target);
            }
        }
    };

    template<class T, class S>
    void decode_to_array(const Message *message, const Reflection *reflection, const FieldDescriptor *fd, AV *target) {
        S setter;
        const RepeatedField<T> &value = reflection->GetRepeatedField<T>(*message, fd);

        av_fill(target, value.size());

        int index = 0;
        for (typename RepeatedField<T>::const_iterator it = value.begin(), en = value.end(); it != en; ++it, ++index) {
            SV **item = av_fetch(target, index, 1);

            setter(*item, *it);
        }
    }

    template<class S>
    void decode_to_string_array(const Message *message, const Reflection *reflection, const FieldDescriptor *fd, AV *target) {
        S setter;
        const RepeatedPtrField<string> &value = reflection->GetRepeatedPtrField<string>(*message, fd);

        av_fill(target, value.size());

        int index = 0;
        for (typename RepeatedPtrField<string>::const_iterator it = value.begin(), en = value.end(); it != en; ++it, ++index) {
            SV **item = av_fetch(target, index, 1);

            setter(*item, *it);
        }
    }

    void decode_to_message_array(const Mapper *mapper, const Message *message, const FieldDescriptor *fd, AV *target) {
        const RepeatedPtrField<Message> &value = mapper->reflection->GetRepeatedPtrField<Message>(*message, fd);
        const Mapper *item_mapper = mapper->mapper_for_descriptor(fd->message_type());

        av_fill(target, value.size() - 1);

        int index = 0;
        for (RepeatedPtrField<Message>::const_iterator it = value.begin(), en = value.end(); it != en; ++it, ++index) {
            SV **item = av_fetch(target, index, 1);

            item_mapper->decode_to_perl(&*it, *item);
        }
    }
}

void Mapper::decode_to_perl_array(const Message *message, const FieldDescriptor *fd, SV *target) const {
    AV *array = newAV();

    sv_upgrade(target, SVt_RV);
    SvROK_on(target);
    SvRV_set(target, (SV *) array);

    switch (fd->type()) {
    case FieldDescriptor::TYPE_DOUBLE:
        decode_to_array<double, NVSetter>(message, reflection, fd, array);
        break;
    case FieldDescriptor::TYPE_FLOAT:
        decode_to_array<float, NVSetter>(message, reflection, fd, array);
        break;
    case FieldDescriptor::TYPE_SINT64:
    case FieldDescriptor::TYPE_SFIXED64:
    case FieldDescriptor::TYPE_INT64:
        decode_to_array<int64, IVSetter>(message, reflection, fd, array);
        break;
    case FieldDescriptor::TYPE_FIXED64:
    case FieldDescriptor::TYPE_UINT64:
        decode_to_array<uint64, UVSetter>(message, reflection, fd, array);
        break;
    case FieldDescriptor::TYPE_SINT32:
    case FieldDescriptor::TYPE_SFIXED32:
    case FieldDescriptor::TYPE_INT32:
        decode_to_array<int32, IVSetter>(message, reflection, fd, array);
        break;
    case FieldDescriptor::TYPE_BOOL:
        decode_to_array<bool, BoolSetter>(message, reflection, fd, array);
        break;
    case FieldDescriptor::TYPE_STRING:
        decode_to_string_array<StringSetter>(message, reflection, fd, array);
        break;
    case FieldDescriptor::TYPE_GROUP:
    case FieldDescriptor::TYPE_MESSAGE:
        decode_to_message_array(this, message, fd, array);
        break;
    case FieldDescriptor::TYPE_BYTES:
        decode_to_string_array<BytesSetter>(message, reflection, fd, array);
        break;
    case FieldDescriptor::TYPE_FIXED32:
    case FieldDescriptor::TYPE_UINT32:
        decode_to_array<uint32, UVSetter>(message, reflection, fd, array);
        break;
    case FieldDescriptor::TYPE_ENUM:
        decode_to_array<int, IVSetter>(message, reflection, fd, array);
        break;
    default:
        croak("Unhandled FieldDescriptor::Type value %d", fd->type());
    }
}

void Mapper::encode_from_perl(Message *message, SV *ref) const {
    const Descriptor *descriptor = message->GetDescriptor();
    if (!SvROK(ref) || SvTYPE(SvRV(ref)) != SVt_PVHV)
        croak("Not an hash reference when encoding a %s value", message->GetDescriptor()->full_name().c_str());
    HV *hv = (HV *) SvRV(ref);

    for (int i = 0, max = descriptor->field_count(); i < max; ++i) {
        const FieldDescriptor *fd = descriptor->field(i);
        const string &name = fd->name();
        SV **svp = hv_fetch(hv, name.data(), name.size(), 0);

        // XXX what about required?
        if (!svp)
            continue;

        if (fd->is_repeated())
            encode_from_perl_array(message, fd, *svp);
        else
            encode_from_perl(message, fd, *svp);
    }
}

void Mapper::encode_from_perl(Message *message, const FieldDescriptor *fd, SV *sv) const {
    switch (fd->type()) {
    case FieldDescriptor::TYPE_DOUBLE:
        reflection->SetDouble(message, fd, SvNV(sv));
        break;
    case FieldDescriptor::TYPE_FLOAT:
        reflection->SetFloat(message, fd, SvNV(sv));
        break;
    case FieldDescriptor::TYPE_SINT64:
    case FieldDescriptor::TYPE_SFIXED64:
    case FieldDescriptor::TYPE_INT64:
        reflection->SetInt64(message, fd, SvIV(sv));
        break;
    case FieldDescriptor::TYPE_FIXED64:
    case FieldDescriptor::TYPE_UINT64:
        reflection->SetUInt64(message, fd, SvUV(sv));
        break;
    case FieldDescriptor::TYPE_SINT32:
    case FieldDescriptor::TYPE_SFIXED32:
    case FieldDescriptor::TYPE_INT32:
        reflection->SetInt32(message, fd, SvIV(sv));
        break;
    case FieldDescriptor::TYPE_BOOL:
        reflection->SetBool(message, fd, SvTRUE(sv));
        break;
    case FieldDescriptor::TYPE_STRING: {
        STRLEN len;
        const char *buf = SvPVutf8(sv, len);

        reflection->SetString(message, fd, string(buf, len));
    }
        break;
    case FieldDescriptor::TYPE_GROUP:
    case FieldDescriptor::TYPE_MESSAGE: {
        mapper_for_descriptor(fd->message_type())->encode_from_perl(reflection->MutableMessage(message, fd), sv);
    }
        break;
    case FieldDescriptor::TYPE_BYTES: {
        STRLEN len;
        const char *buf = SvPV(sv, len);

        reflection->SetString(message, fd, string(buf, len));
    }
        break;
    case FieldDescriptor::TYPE_FIXED32:
    case FieldDescriptor::TYPE_UINT32:
        reflection->SetUInt32(message, fd, SvUV(sv));
        break;
    case FieldDescriptor::TYPE_ENUM:
        croak("XXX Implement me");
        break;
    default:
        croak("Unhandled FieldDescriptor::Type value %d", fd->type());
    }
}

namespace {
    class NVGetter {
    public:
        NV operator()(SV *src) const { return SvNV(src); }
    };

    class IVGetter {
    public:
        IV operator()(SV *src) const { return SvIV(src); }
    };

    class UVGetter {
    public:
        UV operator()(SV *src) const { return SvUV(src); }
    };

    class StringGetter {
    public:
        void operator()(SV *src, string *dest) const {
            STRLEN len;
            const char *buf = SvPVutf8(src, len);

            dest->assign(buf, len);
        }
    };

    class BytesGetter {
    public:
        void operator()(SV *src, string *dest) const {
            STRLEN len;
            const char *buf = SvPV(src, len);

            dest->assign(buf, len);
        }
    };

    class BoolGetter {
    public:
        bool operator()(SV *src) const { return SvTRUE(src); }
    };

    template<class T, class G>
    void encode_from_array(Message *message, const Reflection *reflection, const FieldDescriptor *fd, AV *source) {
        G getter;
        RepeatedField<T> *value = reflection->MutableRepeatedField<T>(message, fd);
        int size = av_top_index(source) + 1;

        value->Reserve(size);

        for (int i = 0; i < size; ++i) {
            SV **item = av_fetch(source, i, 0);
            if (!item)
                croak("Empty value at index %d when serializing field %s", i, fd->full_name().c_str());

            value->AddAlreadyReserved(getter(*item));
        }
    }

    template<class G>
    void encode_from_string_array(Message *message, const Reflection *reflection, const FieldDescriptor *fd, AV *source) {
        G getter;
        RepeatedPtrField<string> *value = reflection->MutableRepeatedPtrField<string>(message, fd);
        int size = av_top_index(source) + 1;

        value->Reserve(size);

        for (int i = 0; i < size; ++i) {
            SV **item = av_fetch(source, i, 0);
            if (!item)
                croak("Empty value at index %d when serializing field %s", i, fd->full_name().c_str());

            getter(*item, value->Add());
        }
    }

    void encode_from_message_array(const Mapper *mapper, Message *message, const FieldDescriptor *fd, AV *source) {
        RepeatedPtrField<Message> *value = mapper->reflection->MutableRepeatedPtrField<Message>(message, fd);
        const Mapper *item_mapper = mapper->mapper_for_descriptor(fd->message_type());
        int size = av_top_index(source) + 1;

        value->Reserve(size);

        for (int i = 0; i < size; ++i) {
            SV **item = av_fetch(source, i, 0);
            if (!item)
                croak("Empty value at index %d when serializing field %s", i, fd->full_name().c_str());
            Message *target = item_mapper->prototype->New();

            value->AddAllocated(target);
            item_mapper->encode_from_perl(target, *item);
        }
    }
}

void Mapper::encode_from_perl_array(Message *message, const FieldDescriptor *fd, SV *ref) const {
    if (!SvROK(ref) || SvTYPE(SvRV(ref)) != SVt_PVAV)
        croak("Not an array reference when encoding field %s", fd->full_name().c_str());
    AV *array = (AV *) SvRV(ref);

    switch (fd->type()) {
    case FieldDescriptor::TYPE_DOUBLE:
        encode_from_array<double, NVGetter>(message, reflection, fd, array);
        break;
    case FieldDescriptor::TYPE_FLOAT:
        encode_from_array<float, NVGetter>(message, reflection, fd, array);
        break;
    case FieldDescriptor::TYPE_SINT64:
    case FieldDescriptor::TYPE_SFIXED64:
    case FieldDescriptor::TYPE_INT64:
        encode_from_array<int64, IVGetter>(message, reflection, fd, array);
        break;
    case FieldDescriptor::TYPE_FIXED64:
    case FieldDescriptor::TYPE_UINT64:
        encode_from_array<uint64, UVGetter>(message, reflection, fd, array);
        break;
    case FieldDescriptor::TYPE_SINT32:
    case FieldDescriptor::TYPE_SFIXED32:
    case FieldDescriptor::TYPE_INT32:
        encode_from_array<int32, IVGetter>(message, reflection, fd, array);
        break;
    case FieldDescriptor::TYPE_BOOL:
        encode_from_array<bool, BoolGetter>(message, reflection, fd, array);
        break;
    case FieldDescriptor::TYPE_STRING:
        encode_from_string_array<StringGetter>(message, reflection, fd, array);
        break;
    case FieldDescriptor::TYPE_GROUP:
    case FieldDescriptor::TYPE_MESSAGE:
        encode_from_message_array(this, message, fd, array);
        break;
    case FieldDescriptor::TYPE_BYTES:
        encode_from_string_array<BytesGetter>(message, reflection, fd, array);
        break;
    case FieldDescriptor::TYPE_FIXED32:
    case FieldDescriptor::TYPE_UINT32:
        encode_from_array<uint32, UVGetter>(message, reflection, fd, array);
        break;
    case FieldDescriptor::TYPE_ENUM:
        encode_from_array<int, IVGetter>(message, reflection, fd, array);
        break;
    default:
        croak("Unhandled FieldDescriptor::Type value %d", fd->type());
    }
}

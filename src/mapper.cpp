#include "mapper.h"
#include "dynamic.h"

#undef New

#include <upb/pb/encoder.h>
#include <upb/pb/decoder.h>

using namespace gpd;
using namespace std;
using namespace upb;
using namespace upb::pb;

#ifdef MULTIPLICITY
#    define THX_DECLARE_AND_GET tTHX my_perl = cxt->my_perl
#else
#    define THX_DECLARE_AND_GET
#endif

namespace {
    void unref_on_scope_leave(void *ref) {
        ((Refcounted *) ref)->unref();
    }

    void refcounted_mortalize(pTHX_ const Refcounted *ref) {
        SAVEDESTRUCTOR(unref_on_scope_leave, ref);
    }
}

Mapper::DecoderHandlers::DecoderHandlers(pTHX_ const Mapper *mapper) {
    SET_THX_MEMBER;
    mappers.push_back(mapper);
}

void Mapper::DecoderHandlers::prepare(HV *target) {
    mappers.resize(1);
    seen_fields.resize(1);
    seen_fields.back().clear();
    seen_fields.back().resize(mappers.back()->fields.size());
    items.resize(1);
    items[0] = (SV *) target;
    string = NULL;
}

SV *Mapper::DecoderHandlers::get_target() {
    return items[0];
}

void Mapper::DecoderHandlers::clear() {
    SvREFCNT_dec(items[0]);
}

void Mapper::DecoderHandlers::apply_defaults() {
    const vector<bool> &seen = seen_fields.back();
    const vector<Mapper::Field> &fields = mappers.back()->fields;

    for (int i = 0, n = fields.size(); i < n; ++i) {
        const Mapper::Field &field = fields[i];

        if (!seen[i] && field.has_default) {
            SV *target = get_target(&i);

            switch (field.field_def->type()) {
            case UPB_TYPE_FLOAT:
                sv_setnv(target, field.field_def->default_float());
                break;
            case UPB_TYPE_DOUBLE:
                sv_setnv(target, field.field_def->default_double());
                break;
            case UPB_TYPE_BOOL:
                if (field.field_def->default_bool())
                    sv_setiv(target, 1);
                else
                    sv_setpvn(target, "", 0);
                break;
            case UPB_TYPE_BYTES:
            case UPB_TYPE_STRING: {
                size_t len;
                const char *val = field.field_def->default_string(&len);
                if (!val) {
                    val = "";
                    len = 0;
                }

                sv_setpvn(target, val, len);
                if (field.field_def->type() == UPB_TYPE_STRING)
                    SvUTF8_on(target);
            }
                break;
            case UPB_TYPE_ENUM:
            case UPB_TYPE_INT32:
                sv_setiv(target, field.field_def->default_int32());
                break;
            case UPB_TYPE_UINT32:
                sv_setuv(target, field.field_def->default_uint32());
                break;
            case UPB_TYPE_INT64:
                sv_setiv(target, field.field_def->default_int64());
                break;
            case UPB_TYPE_UINT64:
                sv_setuv(target, field.field_def->default_uint64());
                break;
            }
        }
    }
}

bool Mapper::DecoderHandlers::on_end_message(DecoderHandlers *cxt, upb::Status *status) {
    if (!status || status->ok())
        cxt->apply_defaults();

    return true;
}

Mapper::DecoderHandlers *Mapper::DecoderHandlers::on_start_string(DecoderHandlers *cxt, const int *field_index, size_t size_hint) {
    cxt->mark_seen(field_index);
    cxt->string = cxt->get_target(field_index);

    return cxt;
}

size_t Mapper::DecoderHandlers::on_string(DecoderHandlers *cxt, const int *field_index, const char *buf, size_t len) {
    THX_DECLARE_AND_GET;

    if (!SvOK(cxt->string))
        sv_setpvn(cxt->string, buf, len);
    else
        sv_catpvn(cxt->string, buf, len);

    return len;
}

bool Mapper::DecoderHandlers::on_end_string(DecoderHandlers *cxt, const int *field_index) {
    const Mapper *mapper = cxt->mappers.back();
    if (mapper->fields[*field_index].field_def->type() == UPB_TYPE_STRING)
        SvUTF8_on(cxt->string);
    cxt->string = NULL;

    return true;
}

Mapper::DecoderHandlers *Mapper::DecoderHandlers::on_start_sequence(DecoderHandlers *cxt, const int *field_index) {
    THX_DECLARE_AND_GET;

    cxt->mark_seen(field_index);
    SV *target = cxt->get_target(field_index);
    AV *av = NULL;

    if (!SvROK(target)) {
        av = newAV();

        SvUPGRADE(target, SVt_RV);
        SvROK_on(target);
        SvRV_set(target, (SV *) av);
    } else
        av = (AV *) SvRV(target);

    cxt->items.push_back((SV *) av);

    return cxt;
}

bool Mapper::DecoderHandlers::on_end_sequence(DecoderHandlers *cxt, const int *field_index) {
    cxt->items.pop_back();

    return true;
}

Mapper::DecoderHandlers *Mapper::DecoderHandlers::on_start_sub_message(DecoderHandlers *cxt, const int *field_index) {
    THX_DECLARE_AND_GET;

    cxt->mark_seen(field_index);
    const Mapper *mapper = cxt->mappers.back();
    SV *target = cxt->get_target(field_index);
    HV *hv = newHV();

    SvUPGRADE(target, SVt_RV);
    SvROK_on(target);
    SvRV_set(target, (SV *) hv);

    cxt->items.push_back((SV *) hv);
    cxt->mappers.push_back(mapper->fields[*field_index].mapper);
    cxt->seen_fields.resize(cxt->seen_fields.size() + 1);
    cxt->seen_fields.back().resize(cxt->mappers.back()->fields.size());

    return cxt;
}

bool Mapper::DecoderHandlers::on_end_sub_message(DecoderHandlers *cxt, const int *field_index) {
    cxt->items.pop_back();
    cxt->mappers.pop_back();
    cxt->seen_fields.pop_back();

    return true;
}

template<class T>
bool Mapper::DecoderHandlers::on_nv(DecoderHandlers *cxt, const int *field_index, T val) {
    THX_DECLARE_AND_GET;

    cxt->mark_seen(field_index);
    sv_setnv(cxt->get_target(field_index), val);

    return true;
}

template<class T>
bool Mapper::DecoderHandlers::on_iv(DecoderHandlers *cxt, const int *field_index, T val) {
    THX_DECLARE_AND_GET;

    cxt->mark_seen(field_index);
    sv_setiv(cxt->get_target(field_index), val);

    return true;
}

template<class T>
bool Mapper::DecoderHandlers::on_uv(DecoderHandlers *cxt, const int *field_index, T val) {
    THX_DECLARE_AND_GET;

    cxt->mark_seen(field_index);
    sv_setuv(cxt->get_target(field_index), val);

    return true;
}

bool Mapper::DecoderHandlers::on_bool(DecoderHandlers *cxt, const int *field_index, bool val) {
    THX_DECLARE_AND_GET;

    cxt->mark_seen(field_index);
    SV * target = cxt->get_target(field_index);

    if (val)
        sv_setiv(target, 1);
    else
        sv_setpvn(target, "", 0);

    return true;
}

SV *Mapper::DecoderHandlers::get_target(const int *field_index) {
    const Mapper *mapper = mappers.back();
    const Field &field = mapper->fields[*field_index];
    SV *curr = items.back();

    if (SvTYPE(curr) == SVt_PVAV) {
        AV *av = (AV *) curr;

        return *av_fetch(av, av_top_index(av) + 1, 1);
    } else {
        HV *hv = (HV *) curr;

        return HeVAL(hv_fetch_ent(hv, field.name, 1, field.name_hash));
    }
}

void Mapper::DecoderHandlers::mark_seen(const int *field_index) {
    seen_fields.back()[*field_index] = true;
}

Mapper::Mapper(pTHX_ Dynamic *_registry, const MessageDef *_message_def) :
        registry(_registry),
        message_def(_message_def),
        decoder_callbacks(aTHX_ this),
        string_sink(&output_buffer) {
    SET_THX_MEMBER;

    registry->ref();
    encoder_handlers = Encoder::NewHandlers(message_def);
    decoder_handlers = Handlers::New(message_def);

    if (!decoder_handlers->SetEndMessageHandler(UpbMakeHandler(DecoderHandlers::on_end_message)))
        croak("Unable to set upb end message handler for %s", message_def->full_name());

    // XXX one_of, extensions, ...
    for (MessageDef::const_field_iterator it = message_def->field_begin(), en = message_def->field_end(); it != en; ++it) {
        int index = fields.size();
        fields.push_back(Field());

        Field &field = fields.back();;
        const FieldDef *field_def = *it;

        field.field_def = field_def;
        field.name = newSVpv_share(field_def->name(), 0);
        field.name_hash = SvSHARED_HASH(field.name);
        field.mapper = NULL;

#define GET_SELECTOR(KIND, TO) \
    ok = ok && encoder_handlers->GetSelector(field_def, UPB_HANDLER_##KIND, &field.selector.TO)

#define SET_VALUE_HANDLER(TYPE, FUNCTION) \
    ok = ok && decoder_handlers->SetValueHandler<TYPE>(field_def, UpbBind(DecoderHandlers::FUNCTION, new int(index)))

#define SET_HANDLER(KIND, FUNCTION) \
    ok = ok && decoder_handlers->Set##KIND##Handler(field_def, UpbBind(DecoderHandlers::FUNCTION, new int(index)))

        bool ok = true;
        bool has_default = true;
        switch (field_def->type()) {
        case UPB_TYPE_FLOAT:
            GET_SELECTOR(FLOAT, primitive);
            SET_VALUE_HANDLER(float, on_nv<float>);
            break;
        case UPB_TYPE_DOUBLE:
            GET_SELECTOR(DOUBLE, primitive);
            SET_VALUE_HANDLER(double, on_nv<double>);
            break;
        case UPB_TYPE_BOOL:
            GET_SELECTOR(BOOL, primitive);
            SET_VALUE_HANDLER(bool, on_bool);
            break;
        case UPB_TYPE_STRING:
        case UPB_TYPE_BYTES:
            GET_SELECTOR(STARTSTR, str_start);
            GET_SELECTOR(STRING, str_cont);
            GET_SELECTOR(ENDSTR, str_end);
            SET_HANDLER(StartString, on_start_string);
            SET_HANDLER(String, on_string);
            SET_HANDLER(EndString, on_end_string);
            break;
        case UPB_TYPE_MESSAGE:
            GET_SELECTOR(STARTSUBMSG, msg_start);
            GET_SELECTOR(ENDSUBMSG, msg_end);
            SET_HANDLER(StartSubMessage, on_start_sub_message);
            SET_HANDLER(EndSubMessage, on_end_sub_message);
            has_default = false;
            break;
        case UPB_TYPE_ENUM:
        case UPB_TYPE_INT32:
            GET_SELECTOR(INT32, primitive);
            SET_VALUE_HANDLER(int32_t, on_iv<int32_t>);
            break;
        case UPB_TYPE_UINT32:
            GET_SELECTOR(UINT32, primitive);
            SET_VALUE_HANDLER(uint32_t, on_uv<uint32_t>);
            break;
        case UPB_TYPE_INT64:
            GET_SELECTOR(INT64, primitive);
            SET_VALUE_HANDLER(int64_t, on_iv<int64_t>);
            break;
        case UPB_TYPE_UINT64:
            GET_SELECTOR(UINT64, primitive);
            SET_VALUE_HANDLER(uint64_t, on_uv<uint64_t>);
            break;
        default:
            croak("Unhandled field type %d", field_def->type());
        }

        if (has_default && field_def->label() == UPB_LABEL_OPTIONAL) {
            field.has_default = true;
        }

        if (field_def->label() == UPB_LABEL_REPEATED) {
            GET_SELECTOR(STARTSEQ, seq_start);
            GET_SELECTOR(ENDSEQ, seq_end);
            SET_HANDLER(StartSequence, on_start_sequence);
            SET_HANDLER(EndSequence, on_end_sequence);
        }

#undef GET_SELECTOR
#undef SET_VALUE_HANDLER
#undef SET_HANDLER

        if (!ok)
            croak("Unable to get upb selector for field %s", field_def->full_name());
    }
}

Mapper::~Mapper() {
    for (vector<Field>::iterator it = fields.begin(), en = fields.end(); it != en; ++it)
        if (it->mapper)
            it->mapper->unref();
    // make sure this only goes away after inner destructors have completed
    refcounted_mortalize(registry);
}

void Mapper::resolve_mappers() {
    for (vector<Field>::iterator it = fields.begin(), en = fields.end(); it != en; ++it) {
        const FieldDef *field = it->field_def;

        if (field->type() != UPB_TYPE_MESSAGE)
            continue;
        it->mapper = registry->find_mapper(field->message_subdef());
        it->mapper->ref();
        decoder_handlers->SetSubHandlers(it->field_def, it->mapper->decoder_handlers.get());
    }

    decoder_method = DecoderMethod::New(DecoderMethodOptions(decoder_handlers.get()));
    decoder_sink.Reset(decoder_handlers.get(), &decoder_callbacks);
    decoder = upb::pb::Decoder::Create(&env, decoder_method.get(), &decoder_sink);
    encoder = upb::pb::Encoder::Create(&env, encoder_handlers.get(), string_sink.input());
}

SV *Mapper::encode_from_perl(SV *ref) {
    output_buffer.clear();
    SV *result = NULL;
    if (encode_from_perl(encoder, encoder->input(), ref))
        result = newSVpvn(output_buffer.data(), output_buffer.size());
    output_buffer.clear();

    return result;
}

SV *Mapper::decode_to_perl(const char *buffer, STRLEN bufsize) {
    decoder->Reset();
    decoder_callbacks.prepare(newHV());

    SV *result = NULL;
    if (BufferSource::PutBuffer(buffer, bufsize, decoder->input()))
        result = newRV_inc(decoder_callbacks.get_target());
    decoder_callbacks.clear();

    return result;
}

namespace {
    class SVGetter {
    public:
        SV *operator()(pTHX_ SV *src) const { return src; }
    };

    class NVGetter {
    public:
        NV operator()(pTHX_ SV *src) const { return SvNV(src); }
    };

    class IVGetter {
    public:
        IV operator()(pTHX_ SV *src) const { return SvIV(src); }
    };

    class UVGetter {
    public:
        UV operator()(pTHX_ SV *src) const { return SvUV(src); }
    };

    class StringGetter {
    public:
        void operator()(pTHX_ SV *src, string *dest) const {
            STRLEN len;
            const char *buf = SvPVutf8(src, len);

            dest->assign(buf, len);
        }
    };

    class BytesGetter {
    public:
        void operator()(pTHX_ SV *src, string *dest) const {
            STRLEN len;
            const char *buf = SvPV(src, len);

            dest->assign(buf, len);
        }
    };

    class BoolGetter {
    public:
        bool operator()(pTHX_ SV *src) const { return SvTRUE(src); }
    };

#define DEF_SIMPLE_SETTER(NAME, METHOD, TYPE)   \
    struct NAME { \
        bool operator()(pTHX_ Sink *sink, const Mapper::Field &fd, TYPE value) { \
            sink->METHOD(fd.selector.primitive, value);    \
        } \
    }

    DEF_SIMPLE_SETTER(Int32Setter, PutInt32, IV);
    DEF_SIMPLE_SETTER(Int64Setter, PutInt64, IV);
    DEF_SIMPLE_SETTER(UInt32Setter, PutUInt32, UV);
    DEF_SIMPLE_SETTER(UInt64Setter, PutUInt64, UV);
    DEF_SIMPLE_SETTER(FloatSetter, PutFloat, NV);
    DEF_SIMPLE_SETTER(DoubleSetter, PutDouble, NV);
    DEF_SIMPLE_SETTER(BoolSetter, PutBool, bool);

#undef DEF_SIMPLE_SETTER

    struct StringSetter {
        bool operator()(pTHX_ Sink *sink, const Mapper::Field &fd, SV *value) {
            STRLEN len;
            const char *str = fd.field_def->type() == UPB_TYPE_STRING ? SvPVutf8(value, len) : SvPV(value, len);
            Sink sub;
            if (!sink->StartString(fd.selector.str_start, len, &sub))
                return false;
            sub.PutStringBuffer(fd.selector.str_cont, str, len, NULL);
            return sink->EndString(fd.selector.str_end);
        }
    };
}

template<class G, class S>
bool Mapper::encode_from_array(Encoder *encoder, Sink *sink, const Mapper::Field &fd, AV *source) const {
    G getter;
    S setter;
    Sink sub, *actual = sink;
    bool packed = upb_fielddef_isprimitive(fd.field_def) && fd.field_def->packed();

    if (packed) {
        if (!sink->StartSequence(fd.selector.seq_start, &sub))
            return false;
        actual = &sub;
    }
    int size = av_top_index(source) + 1;

    for (int i = 0; i < size; ++i) {
        SV **item = av_fetch(source, i, 0);
        if (!item)
            return false;

        setter(aTHX_ actual, fd, getter(aTHX_ *item));
    }

    return !packed || sink->EndSequence(fd.selector.seq_end);
}

bool Mapper::encode_from_message_array(Encoder *encoder, Sink *sink, const Mapper::Field &fd, AV *source) const {
    int size = av_top_index(source) + 1;

    for (int i = 0; i < size; ++i) {
        SV **item = av_fetch(source, i, 0);
        if (!item)
            return false;
        Sink submsg;

        if (!sink->StartSubMessage(fd.selector.msg_start, &submsg))
            return false;
        if (!encode_from_perl(encoder, &submsg, *item))
            return false;
        if (!sink->EndSubMessage(fd.selector.msg_end))
            return false;
    }

    return true;
}

bool Mapper::encode_from_perl(Encoder* encoder, Sink *sink, SV *ref) const {
    if (!SvROK(ref) || SvTYPE(SvRV(ref)) != SVt_PVHV)
        croak("Not an hash reference when encoding a %s value", message_def->full_name());
    HV *hv = (HV *) SvRV(ref);
    Status status;

    if (!sink->StartMessage())
        return false;

    for (vector<Field>::const_iterator it = fields.begin(), en = fields.end(); it != en; ++it) {
        HE *he = hv_fetch_ent(hv, it->name, 0, it->name_hash);

        // XXX what about required?
        if (!he)
            continue;

        if (it->field_def->label() == UPB_LABEL_REPEATED)
            encode_from_perl_array(encoder, sink, *it, HeVAL(he));
        else
            encode_from_perl(encoder, sink, *it, HeVAL(he));
    }

    if (!sink->EndMessage(&status))
        return false;
}

bool Mapper::encode_from_perl(Encoder* encoder, Sink *sink, const Field &fd, SV *ref) const {
    switch (fd.field_def->type()) {
    case UPB_TYPE_FLOAT:
        return sink->PutFloat(fd.selector.primitive, SvNV(ref));
    case UPB_TYPE_DOUBLE:
        return sink->PutDouble(fd.selector.primitive, SvNV(ref));
    case UPB_TYPE_BOOL:
        return sink->PutBool(fd.selector.primitive, SvTRUE(ref));
    case UPB_TYPE_STRING:
    case UPB_TYPE_BYTES: {
        STRLEN len;
        const char *str = fd.field_def->type() == UPB_TYPE_STRING ? SvPVutf8(ref, len) : SvPV(ref, len);
        Sink sub;
        if (!sink->StartString(fd.selector.str_start, len, &sub))
            return false;
        sub.PutStringBuffer(fd.selector.str_cont, str, len, NULL);
        return sink->EndString(fd.selector.str_end);
    }
    case UPB_TYPE_MESSAGE: {
        Sink sub;
        if (!sink->StartSubMessage(fd.selector.msg_start, &sub))
            return false;
        if (!fd.mapper->encode_from_perl(encoder, &sub, ref))
            return false;
        return sink->EndSubMessage(fd.selector.msg_end);
    }
    case UPB_TYPE_ENUM:
        return sink->PutInt32(fd.selector.primitive, SvIV(ref)); // XXX validation
    case UPB_TYPE_INT32:
        return sink->PutInt32(fd.selector.primitive, SvIV(ref));
    case UPB_TYPE_UINT32:
        return sink->PutUInt32(fd.selector.primitive, SvUV(ref));
    case UPB_TYPE_INT64:
        // XXX 32-bit Perl
        return sink->PutInt64(fd.selector.primitive, SvIV(ref));
    case UPB_TYPE_UINT64:
        // XXX 32-bit Perl
        return sink->PutUInt64(fd.selector.primitive, SvUV(ref));
    default:
        return false; // just in case
    }
}

bool Mapper::encode_from_perl_array(Encoder* encoder, Sink *sink, const Field &fd, SV *ref) const {
    if (!SvROK(ref) || SvTYPE(SvRV(ref)) != SVt_PVAV)
        croak("Not an array reference when encoding field %s", fd.field_def->full_name());
    AV *array = (AV *) SvRV(ref);

    switch (fd.field_def->type()) {
    case UPB_TYPE_FLOAT:
        return encode_from_array<NVGetter, FloatSetter>(encoder, sink, fd, array);
    case UPB_TYPE_DOUBLE:
        return encode_from_array<NVGetter, DoubleSetter>(encoder, sink, fd, array);
    case UPB_TYPE_BOOL:
        return encode_from_array<BoolGetter, BoolSetter>(encoder, sink, fd, array);
    case UPB_TYPE_STRING:
        return encode_from_array<SVGetter, StringSetter>(encoder, sink, fd, array);
    case UPB_TYPE_BYTES:
        return encode_from_array<SVGetter, StringSetter>(encoder, sink, fd, array);
    case UPB_TYPE_MESSAGE:
        return fd.mapper->encode_from_message_array(encoder, sink, fd, array);
    case UPB_TYPE_ENUM:
        // XXX validation
        return encode_from_array<IVGetter, Int32Setter>(encoder, sink, fd, array);
    case UPB_TYPE_INT32:
        return encode_from_array<IVGetter, Int32Setter>(encoder, sink, fd, array);
    case UPB_TYPE_UINT32:
        return encode_from_array<UVGetter, UInt32Setter>(encoder, sink, fd, array);
    case UPB_TYPE_INT64:
        return encode_from_array<IVGetter, Int64Setter>(encoder, sink, fd, array);
    case UPB_TYPE_UINT64:
        return encode_from_array<UVGetter, UInt64Setter>(encoder, sink, fd, array);
    default:
        return false; // just in case
    }
}

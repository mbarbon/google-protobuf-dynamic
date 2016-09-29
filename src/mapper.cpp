#include "mapper.h"
#include "dynamic.h"

#undef New

#include <upb/pb/encoder.h>
#include <upb/pb/decoder.h>

using namespace gpd;
using namespace std;
using namespace upb;
using namespace upb::pb;
using namespace upb::json;

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
    if (int oneof_count = mappers.back()->message_def->oneof_count()) {
        seen_oneof.resize(1);
        seen_oneof.back().clear();
        seen_oneof.back().resize(oneof_count, -1);
    }
    items.resize(1);
    error.clear();
    items[0] = (SV *) target;
    string = NULL;
}

SV *Mapper::DecoderHandlers::get_target() {
    return items[0];
}

void Mapper::DecoderHandlers::clear() {
    SvREFCNT_dec(items[0]);
}

namespace {
#if PERL_VERSION < 18
    inline SSize_t GPD_av_top_index(pTHX_ AV *av) {
        return AvFILL(av);
    }
    #define av_top_index(av) GPD_av_top_index(aTHX_ (av))

    #define READ_XDIGIT(s)  ((0xf & (isDIGIT(*(s))     \
                                    ? (*(s)++)         \
                                    : (*(s)++ + 9))))
#endif

    inline void set_bool(pTHX_ SV *target, bool value) {
        if (value)
            sv_setiv(target, 1);
        else
            sv_setpvn(target, "", 0);
    }
}

string Mapper::Field::full_name() const {
    if (field_def->is_extension())
        return field_def->full_name();
    else
        return string(field_def->containing_type()->full_name()) +
               '.' +
               field_def->name();
}

FieldDef::Type Mapper::Field::map_value_type() const {
    const vector<Field> &map_fields = mapper->fields;

    return map_fields[1].is_value ?
        map_fields[1].field_def->type() :
        map_fields[0].field_def->type();
}

const STD_TR1::unordered_set<int32_t> &Mapper::Field::map_enum_values() const {
    const vector<Field> &map_fields = mapper->fields;

    return map_fields[1].is_value ?
        map_fields[1].enum_values :
        map_fields[0].enum_values;
}

bool Mapper::DecoderHandlers::apply_defaults_and_check() {
    const vector<bool> &seen = seen_fields.back();
    const Mapper *mapper = mappers.back();
    const vector<Mapper::Field> &fields = mapper->fields;
    bool decode_explict_defaults = mapper->decode_explicit_defaults;
    bool check_required_fields = mapper->check_required_fields;

    for (int i = 0, n = fields.size(); i < n; ++i) {
        const Mapper::Field &field = fields[i];
        bool field_seen = seen[i];

        if (!field_seen && decode_explict_defaults && field.has_default) {
            SV *target = get_target(&i);

            // this is the case where we merged multiple message instances,
            // so defaults have already been applied
            if (SvOK(target))
                continue;

            switch (field.field_def->type()) {
            case UPB_TYPE_FLOAT:
                sv_setnv(target, field.field_def->default_float());
                break;
            case UPB_TYPE_DOUBLE:
                sv_setnv(target, field.field_def->default_double());
                break;
            case UPB_TYPE_BOOL:
                set_bool(aTHX_ target, field.field_def->default_bool());
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
        } else if (!field_seen && check_required_fields && field.field_def->label() == UPB_LABEL_REQUIRED) {
            error = "Missing required field " + field.full_name();

            return false;
        }
    }

    return true;
}

bool Mapper::DecoderHandlers::on_end_message(DecoderHandlers *cxt, upb::Status *status) {
    if (!status || status->ok()) {
        return cxt->apply_defaults_and_check();
    } else
        return false;
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

Mapper::DecoderHandlers *Mapper::DecoderHandlers::on_start_map(DecoderHandlers *cxt, const int *field_index) {
    THX_DECLARE_AND_GET;

    cxt->mark_seen(field_index);
    const Mapper *mapper = cxt->mappers.back();
    SV *target = cxt->get_target(field_index);
    HV *hv = NULL;

    if (!SvROK(target)) {
        hv = newHV();

        SvUPGRADE(target, SVt_RV);
        SvROK_on(target);
        SvRV_set(target, (SV *) hv);
    } else
        hv = (HV *) SvRV(target);

    cxt->mappers.push_back(mapper->fields[*field_index].mapper);
    cxt->items.push_back((SV *) hv);
    cxt->items.push_back(sv_newmortal());
    cxt->items.push_back(NULL);

    return cxt;
}

bool Mapper::DecoderHandlers::on_end_map(DecoderHandlers *cxt, const int *field_index) {
    cxt->mappers.pop_back();
    cxt->items.pop_back();
    cxt->items.pop_back();
    cxt->items.pop_back();

    return true;
}

Mapper::DecoderHandlers *Mapper::DecoderHandlers::on_start_sub_message(DecoderHandlers *cxt, const int *field_index) {
    THX_DECLARE_AND_GET;

    cxt->mark_seen(field_index);
    const Mapper *mapper = cxt->mappers.back();
    SV *target = cxt->get_target(field_index);
    HV *hv = NULL;

    if (!SvROK(target)) {
        hv = newHV();

        SvUPGRADE(target, SVt_RV);
        SvROK_on(target);
        SvRV_set(target, (SV *) hv);
    } else
        hv = (HV *) SvRV(target);

    cxt->items.push_back((SV *) hv);
    cxt->mappers.push_back(mapper->fields[*field_index].mapper);
    cxt->seen_fields.resize(cxt->seen_fields.size() + 1);
    cxt->seen_fields.back().resize(cxt->mappers.back()->fields.size());
    if (int oneof_count = cxt->mappers.back()->message_def->oneof_count()) {
        cxt->seen_oneof.resize(cxt->seen_oneof.size() + 1);
        cxt->seen_oneof.back().resize(oneof_count, -1);
    }
    sv_bless(target, cxt->mappers.back()->stash);

    return cxt;
}

bool Mapper::DecoderHandlers::on_end_sub_message(DecoderHandlers *cxt, const int *field_index) {
    cxt->items.pop_back();
    cxt->mappers.pop_back();
    cxt->seen_fields.pop_back();
    if (cxt->mappers.back()->message_def->oneof_count())
        cxt->seen_oneof.pop_back();

    return true;
}

bool Mapper::DecoderHandlers::on_end_map_entry(DecoderHandlers *cxt, const int *field_index) {
    THX_DECLARE_AND_GET;

    size_t size = cxt->items.size();
    HV *hash = (HV *) cxt->items[size - 3];
    SV *key = (SV *) cxt->items[size - 2];
    SV *value = (SV *) cxt->items[size - 1];

    SvREFCNT_inc(value);
    hv_store_ent(hash, key, value, 0);

    if (SvPOK(key))
        SvLEN_set(key, 0);
    SvOK_off(key);

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

bool Mapper::DecoderHandlers::on_enum(DecoderHandlers *cxt, const int *field_index, int32_t val) {
    THX_DECLARE_AND_GET;

    const Field &field = cxt->mappers.back()->fields[*field_index];
    if (field.enum_values.find(val) == field.enum_values.end()) {
        // this will use the default value later, it's intentional
        // mark_seen is not called
        if (SvTYPE(cxt->items.back()) == SVt_PVAV)
            sv_setiv(cxt->get_target(field_index), field.field_def->default_int32());
        return true;
    }

    cxt->mark_seen(field_index);
    sv_setiv(cxt->get_target(field_index), val);

    return true;
}

namespace {
    bool set_bigint(pTHX_ SV *target, uint64_t value, bool negative) {
        dSP;

        // this code is horribly slow; it could be made slightly faster,
        // but I doubt there is any point
        char buffer[19] = "-0x"; // -0x8000000000000000
        for (int i = 15; i >= 0; --i, value >>= 4) {
            int digit = value & 0xf;

            buffer[3 + i] = digit < 10 ? '0' + digit : 'a' + digit - 10;
        }

        PUSHMARK(SP);
        XPUSHs(sv_2mortal(newSVpvs("Math::BigInt")));
        XPUSHs(sv_2mortal(newSVpvn(negative ? buffer : buffer + 1, negative ? 19 : 18)));
        PUTBACK;

        int count = call_method("new", G_SCALAR);

        SPAGAIN;
        SV *res = POPs;
        PUTBACK;

        sv_setsv(target, res);

        return true;
    }
}

bool Mapper::DecoderHandlers::on_bigiv(DecoderHandlers *cxt, const int *field_index, int64_t val) {
    THX_DECLARE_AND_GET;
    cxt->mark_seen(field_index);

    if (val >= I32_MIN && val <= I32_MAX) {
        sv_setiv(cxt->get_target(field_index), (IV) val);

        return true;
    } else {
        THX_DECLARE_AND_GET;

        return set_bigint(aTHX_ cxt->get_target(field_index), (uint64_t) val, val < 0);
    }
}

bool Mapper::DecoderHandlers::on_biguv(DecoderHandlers *cxt, const int *field_index, uint64_t val) {
    THX_DECLARE_AND_GET;
    cxt->mark_seen(field_index);

    if (val <= U32_MAX) {
        sv_setiv(cxt->get_target(field_index), (IV) val);

        return true;
    } else {
        THX_DECLARE_AND_GET;

        return set_bigint(aTHX_ cxt->get_target(field_index), val, false);
    }
}

bool Mapper::DecoderHandlers::on_bool(DecoderHandlers *cxt, const int *field_index, bool val) {
    THX_DECLARE_AND_GET;

    cxt->mark_seen(field_index);
    set_bool(aTHX_ cxt->get_target(field_index), val);

    return true;
}

SV *Mapper::DecoderHandlers::get_target(const int *field_index) {
    const Mapper *mapper = mappers.back();
    const Field &field = mapper->fields[*field_index];
    SV *curr = items.back();

    if (field.is_key) {
        return items[items.size() - 2];
    } else if (field.is_value) {
        SV *sv = sv_newmortal();

        items[items.size() - 1] = sv;

        return sv;
    } else if (SvTYPE(curr) == SVt_PVAV) {
        AV *av = (AV *) curr;

        return *av_fetch(av, av_top_index(av) + 1, 1);
    } else {
        HV *hv = (HV *) curr;

        if (field.oneof_index != -1) {
            int32_t seen = seen_oneof.back()[field.oneof_index];

            if (seen != -1 && seen != *field_index) {
                const Field &field = mapper->fields[seen];

                hv_delete_ent(hv, field.name, G_DISCARD, field.name_hash);
            }
            seen_oneof.back()[field.oneof_index] = *field_index;
        }

        return HeVAL(hv_fetch_ent(hv, field.name, 1, field.name_hash));
    }
}

void Mapper::DecoderHandlers::mark_seen(const int *field_index) {
    seen_fields.back()[*field_index] = true;
}

Mapper::Mapper(pTHX_ Dynamic *_registry, const MessageDef *_message_def, HV *_stash, const MappingOptions &options) :
        registry(_registry),
        message_def(_message_def),
        stash(_stash),
        decoder_callbacks(aTHX_ this),
        string_sink(&output_buffer) {
    SET_THX_MEMBER;

    SvREFCNT_inc(stash);

    env.ReportErrorsTo(&status);
    registry->ref();
    pb_encoder = NULL;
    pb_decoder = NULL;
    json_encoder = NULL;
    json_decoder = NULL;
    pb_encoder_handlers = Encoder::NewHandlers(message_def);
    json_encoder_handlers = Printer::NewHandlers(message_def, false /* XXX option */);
    decoder_handlers = Handlers::New(message_def);
    decode_explicit_defaults = options.explicit_defaults;
    encode_defaults = message_def->syntax() == UPB_SYNTAX_PROTO2 &&
        options.encode_defaults;
    check_enum_values = options.check_enum_values;
    warn_context = WarnContext::get(aTHX);

    if (!decoder_handlers->SetEndMessageHandler(UpbMakeHandler(DecoderHandlers::on_end_message)))
        croak("Unable to set upb end message handler for %s", message_def->full_name());

    bool map_entry = message_def->mapentry();
    bool has_required = false;
    for (MessageDef::const_field_iterator it = message_def->field_begin(), en = message_def->field_end(); it != en; ++it) {
        int index = fields.size();
        fields.push_back(Field());

        Field &field = fields.back();;
        const FieldDef *field_def = *it;
        const OneofDef *oneof_def = field_def->containing_oneof();

        has_required = has_required || field_def->label() == UPB_LABEL_REQUIRED;
        field.field_def = field_def;
        if (field_def->is_extension()) {
            string temp = string() + "[" + field_def->full_name() + "]";

            field.name = newSVpvn_share(temp.data(), temp.size(), 0);
        } else {
            string temp = string(field_def->name());

            field.name = newSVpvn_share(temp.data(), temp.size(), 0);
        }
        field.name_hash = SvSHARED_HASH(field.name);
        field.has_default = field.is_map = false;
        field.mapper = NULL;
        field.oneof_index = -1;

        if (map_entry) {
            field.is_key = field_def->number() == 1;
            field.is_value = field_def->number() == 2;
        }

        if (field_def->label() == UPB_LABEL_REPEATED &&
                field_def->type() == UPB_TYPE_MESSAGE &&
                field_def->message_subdef()->mapentry()) {
            field.is_map = true;
        }

#define GET_SELECTOR(KIND, TO) \
    ok = ok && pb_encoder_handlers->GetSelector(field_def, UPB_HANDLER_##KIND, &field.selector.TO)

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
            field.default_nv = field_def->default_float();
            break;
        case UPB_TYPE_DOUBLE:
            GET_SELECTOR(DOUBLE, primitive);
            SET_VALUE_HANDLER(double, on_nv<double>);
            field.default_nv = field_def->default_double();
            break;
        case UPB_TYPE_BOOL:
            GET_SELECTOR(BOOL, primitive);
            SET_VALUE_HANDLER(bool, on_bool);
            field.default_bool = field_def->default_bool();
            break;
        case UPB_TYPE_STRING:
        case UPB_TYPE_BYTES:
            GET_SELECTOR(STARTSTR, str_start);
            GET_SELECTOR(STRING, str_cont);
            GET_SELECTOR(ENDSTR, str_end);
            SET_HANDLER(StartString, on_start_string);
            SET_HANDLER(String, on_string);
            SET_HANDLER(EndString, on_end_string);
            field.default_str = field_def->default_string(&field.default_str_len);
            break;
        case UPB_TYPE_MESSAGE:
            GET_SELECTOR(STARTSUBMSG, msg_start);
            GET_SELECTOR(ENDSUBMSG, msg_end);
            if (field.is_map) {
                SET_HANDLER(EndSubMessage, on_end_map_entry);
            } else {
                SET_HANDLER(StartSubMessage, on_start_sub_message);
                SET_HANDLER(EndSubMessage, on_end_sub_message);
            }
            has_default = false;
            break;
        case UPB_TYPE_ENUM: {
            GET_SELECTOR(INT32, primitive);
            if (check_enum_values)
                SET_VALUE_HANDLER(int32_t, on_enum);
            else
                SET_VALUE_HANDLER(int32_t, on_iv<int32_t>);
            field.default_iv = field_def->default_int32();

            if (check_enum_values) {
                const EnumDef *enumdef = field_def->enum_subdef();
                upb_enum_iter i;
                for (upb_enum_begin(&i, enumdef); !upb_enum_done(&i); upb_enum_next(&i))
                    field.enum_values.insert(upb_enum_iter_number(&i));
            }
        }
            break;
        case UPB_TYPE_INT32:
            GET_SELECTOR(INT32, primitive);
            SET_VALUE_HANDLER(int32_t, on_iv<int32_t>);
            field.default_iv = field_def->default_int32();
            break;
        case UPB_TYPE_UINT32:
            GET_SELECTOR(UINT32, primitive);
            SET_VALUE_HANDLER(uint32_t, on_uv<uint32_t>);
            field.default_uv = field_def->default_uint32();
            break;
        case UPB_TYPE_INT64:
            GET_SELECTOR(INT64, primitive);
            if (options.use_bigints)
                SET_VALUE_HANDLER(int64_t, on_bigiv);
            else
                SET_VALUE_HANDLER(int64_t, on_iv<int64_t>);
            field.default_i64 = field_def->default_int64();
            break;
        case UPB_TYPE_UINT64:
            GET_SELECTOR(UINT64, primitive);
            if (options.use_bigints)
                SET_VALUE_HANDLER(uint64_t, on_biguv);
            else
                SET_VALUE_HANDLER(uint64_t, on_uv<uint64_t>);
            field.default_u64 = field_def->default_uint64();
            break;
        default:
            croak("Unhandled field type %d for field '%s'", field_def->type(), field.full_name().c_str());
        }

        if (has_default &&
                field_def->label() == UPB_LABEL_OPTIONAL &&
                !oneof_def) {
            field.has_default = true;
        }

        if (field_def->label() == UPB_LABEL_REPEATED) {
            GET_SELECTOR(STARTSEQ, seq_start);
            GET_SELECTOR(ENDSEQ, seq_end);
            if (field.is_map) {
                SET_HANDLER(StartSequence, on_start_map);
                SET_HANDLER(EndSequence, on_end_map);
            } else {
                SET_HANDLER(StartSequence, on_start_sequence);
                SET_HANDLER(EndSequence, on_end_sequence);
            }
        }

#undef GET_SELECTOR
#undef SET_VALUE_HANDLER
#undef SET_HANDLER

        if (!ok)
            croak("Unable to get upb selector for field %s", field.full_name().c_str());

    }

    for (vector<Field>::iterator it = fields.begin(), en = fields.end(); it != en; ++it) {
        if (it->field_def->is_extension()) {
            extension_mapper_fields.push_back(new MapperField(aTHX_ this, &*it));
            unref(); // to avoid ref loop
        }
        field_map[SvPV_nolen(it->name)] = &*it;
    }

    int oneof_index = 0;
    for (MessageDef::const_oneof_iterator it = message_def->oneof_begin(), en = message_def->oneof_end(); it != en; ++it, ++oneof_index) {
        const OneofDef *oneof_def = *it;

        for (OneofDef::const_iterator it = oneof_def->begin(), en = oneof_def->end(); it != en; ++it) {
            const FieldDef *field_def = *it;
            Field &field = fields[field_def->index()];

            field.oneof_index = oneof_index;
        }
    }

    check_required_fields = has_required && options.check_required_fields;
}

Mapper::~Mapper() {
    if (json_decoder)
        upb_env_free(&env, json_decoder);
    upb_env_free(&env, json_encoder);
    upb_env_free(&env, pb_decoder);
    upb_env_free(&env, pb_encoder);

    for (vector<Field>::iterator it = fields.begin(), en = fields.end(); it != en; ++it)
        if (it->mapper)
            it->mapper->unref();
    for (vector<MapperField *>::iterator it = extension_mapper_fields.begin(), en = extension_mapper_fields.end(); it != en; ++it)
        // this will make the mapper ref count to go negative, but it's OK
        (*it)->unref();

    // make sure this only goes away after inner destructors have completed
    refcounted_mortalize(aTHX_ registry);
    SvREFCNT_dec(stash);
}

const char *Mapper::full_name() const {
    return message_def->full_name();
}

MapperField *Mapper::find_extension(const std::string &name) const {
    for (vector<MapperField *>::const_iterator it = extension_mapper_fields.begin(), en = extension_mapper_fields.end(); it != en; ++it) {
        if (name == (*it)->name())
            return *it;
    }

    return NULL;
}

int Mapper::field_count() const {
    return fields.size();
}

const Mapper::Field *Mapper::get_field(int index) const {
    return &fields[index];
}

SV *Mapper::message_descriptor() const {
    SV *ref = newSV(0);

    sv_setref_iv(ref, "Google::ProtocolBuffers::Dynamic::MessageDef", (IV) message_def);

    return ref;
}

SV *Mapper::make_object(SV *data) const {
    SV *obj = NULL;

    if (data) {
        if (!SvROK(data) || SvTYPE(SvRV(data)) != SVt_PVHV)
            croak("Not an hash reference");

        if (SvTEMP(data) && SvREFCNT(data) == 1) {
            // steal
            SvREFCNT_inc(data);
            obj = data;
        } else {
            HV * hv = newHV();
            sv_2mortal((SV *) hv);
            HV *orig = (HV *) SvRV(data);
            I32 keylen;
            char *key;
            hv_iterinit(orig);
            while (SV *sv = hv_iternextsv(orig, &key, &keylen)) {
                hv_store(hv, key, keylen, newSVsv(sv), 0);
            }
            obj = newRV_inc((SV *) hv);
        }
    } else {
        obj = newRV_noinc((SV *) newHV());
    }
    sv_bless(obj, stash);

    return obj;
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
}

void Mapper::create_encoder_decoder() {
    pb_decoder_method = DecoderMethod::New(DecoderMethodOptions(decoder_handlers.get()));
    json_decoder_method = ParserMethod::New(message_def);
    decoder_sink.Reset(decoder_handlers.get(), &decoder_callbacks);
    pb_decoder = upb::pb::Decoder::Create(&env, pb_decoder_method.get(), &decoder_sink);
    json_decoder = NULL;
    pb_encoder = upb::pb::Encoder::Create(&env, pb_encoder_handlers.get(), string_sink.input());
    json_encoder = upb::json::Printer::Create(&env, json_encoder_handlers.get(), string_sink.input());
}

SV *Mapper::encode(SV *ref) {
    if (pb_encoder == NULL)
        croak("It looks like resolve_references() was not called (and please use map() anyway)");
    status.Clear();
    output_buffer.clear();
    warn_context->clear();
    warn_context->localize_warning_handler(aTHX);
    SV *result = NULL;
    if (encode(pb_encoder->input(), &status, ref))
        result = newSVpvn(output_buffer.data(), output_buffer.size());
    output_buffer.clear();

    return result;
}

SV *Mapper::encode_json(SV *ref) {
    if (json_encoder == NULL)
        croak("It looks like resolve_references() was not called (and please use map() anyway)");
    status.Clear();
    output_buffer.clear();
    warn_context->clear();
    warn_context->localize_warning_handler(aTHX);
    SV *result = NULL;
    if (encode(json_encoder->input(), &status, ref))
        result = newSVpvn(output_buffer.data(), output_buffer.size());
    output_buffer.clear();

    return result;
}

SV *Mapper::decode(const char *buffer, STRLEN bufsize) {
    if (pb_decoder == NULL)
        croak("It looks like resolve_references() was not called (and please use map() anyway)");
    status.Clear();
    pb_decoder->Reset();
    decoder_callbacks.prepare(newHV());

    SV *result = NULL;
    if (BufferSource::PutBuffer(buffer, bufsize, pb_decoder->input()))
        result = sv_bless(newRV_inc(decoder_callbacks.get_target()), stash);
    decoder_callbacks.clear();

    return result;
}

SV *Mapper::decode_json(const char *buffer, STRLEN bufsize) {
    if (pb_decoder == NULL)
        croak("It looks like resolve_references() was not called (and please use map() anyway)");
    status.Clear();
    decoder_callbacks.prepare(newHV());
    // if an exception was thrown
    if (json_decoder != NULL)
        upb_env_free(&env, json_decoder);
    // at the moment, the JSON parser can't be reused, and it's unclear whether
    // this is a bug or a feature, so just create a new one every time
    json_decoder = upb::json::Parser::Create(&env, json_decoder_method.get(), &decoder_sink);

    SV *result = NULL;
    if (BufferSource::PutBuffer(buffer, bufsize, json_decoder->input()))
        result = sv_bless(newRV_inc(decoder_callbacks.get_target()), stash);
    decoder_callbacks.clear();
    upb_env_free(&env, json_decoder);
    json_decoder = NULL;

    return result;
}

bool Mapper::check(SV *ref) {
    if (pb_encoder == NULL)
        croak("It looks like resolve_references() was not called (and please use map() anyway)");
    status.Clear();
    return check(&status, ref);
}

const char *Mapper::last_error_message() const {
    return !decoder_callbacks.error.empty() ? decoder_callbacks.error.c_str() :
           !status.ok()                     ? status.error_message() :
                                              "Unknown error";
}

namespace {
    // this code is horribly slow; it could be made slightly faster,
    // but I doubt there is any point
    uint64_t extract_bits(pTHX_ SV *src, bool *negative) {
        dSP;

        PUSHMARK(SP);
        XPUSHs(src);
        PUTBACK;

        int count = call_method("as_hex", G_SCALAR);

        SPAGAIN;
        SV *res = POPs;
        PUTBACK;

        uint64_t integer = 0;
        const char *buffer = SvPV_nolen(res);
        *negative = buffer[0] == '-';

        if (*negative)
            ++buffer;
        buffer += 2; // skip 0x
        while (*buffer) {
            integer <<= 4;
            integer |= READ_XDIGIT(buffer);
        }

        return integer;
    }

    uint64_t get_uint64(pTHX_ SV *src) {
        if (SvROK(src) && sv_derived_from(src, "Math::BigInt")) {
            bool negative = false;
            uint64_t value = extract_bits(aTHX_ src, &negative);

            return negative ? ~value + 1 : value;
        } else
            return SvUV(src);
    }

    int64_t get_int64(pTHX_ SV *src) {
        if (SvROK(src) && sv_derived_from(src, "Math::BigInt")) {
            bool negative = false;
            uint64_t value = extract_bits(aTHX_ src, &negative);

            return negative ? -value : value;
        } else
            return SvIV(src);
    }

    IV key_iv(pTHX_ const char *key, I32 keylen) {
        UV value;
        int numtype = grok_number(key, keylen, &value);

        if (numtype & IS_NUMBER_IN_UV) {
            if (numtype & IS_NUMBER_NEG) {
                if (value < (UV) IV_MIN)
                    return - (IV) value;
            } else {
                if (value < (UV) IV_MAX)
                    return (IV) value;
            }
        }
        // XXX warn
        return 0;
    }

    UV key_uv(pTHX_ const char *key, I32 keylen) {
        UV value;
        int numtype = grok_number(key, keylen, &value);

        if (numtype & IS_NUMBER_IN_UV) {
            return value;
        }
        // XXX warn
        return 0;
    }

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

    class I64Getter {
    public:
        int64_t operator()(pTHX_ SV *src) const { return get_int64(aTHX_ src); }
    };

    class U64Getter {
    public:
        uint64_t operator()(pTHX_ SV *src) const { return get_uint64(aTHX_ src); }
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
        NAME(Status *status) { } \
        bool operator()(pTHX_ Sink *sink, const Mapper::Field &fd, TYPE value) { \
            return sink->METHOD(fd.selector.primitive, value);    \
        } \
    }

    DEF_SIMPLE_SETTER(Int32Emitter, PutInt32, int32_t);
    DEF_SIMPLE_SETTER(Int64Emitter, PutInt64, int64_t);
    DEF_SIMPLE_SETTER(UInt32Emitter, PutUInt32, uint32_t);
    DEF_SIMPLE_SETTER(UInt64Emitter, PutUInt64, uint64_t);
    DEF_SIMPLE_SETTER(FloatEmitter, PutFloat, float);
    DEF_SIMPLE_SETTER(DoubleEmitter, PutDouble, double);
    DEF_SIMPLE_SETTER(BoolEmitter, PutBool, bool);

#undef DEF_SIMPLE_SETTER

    struct EnumEmitter {
        Status *status;

        EnumEmitter(Status *_status) { status = _status; }

        bool operator()(pTHX_ Sink *sink, const Mapper::Field &fd, int32_t value) {
            if (fd.enum_values.find(value) == fd.enum_values.end()) {
                status->SetFormattedErrorMessage(
                    "Invalid enumeration value %d for field '%s'",
                    value,
                    fd.full_name().c_str()
                );
                return false;
            }

            return sink->PutInt32(fd.selector.primitive, value);
        }
    };

    struct StringEmitter {
        StringEmitter(Status *status) { }

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
bool Mapper::encode_from_array(Sink *sink, Status *status, const Mapper::Field &fd, AV *source) const {
    G getter;
    S setter(status);
    Sink sub;

    if (!sink->StartSequence(fd.selector.seq_start, &sub))
        return false;
    int size = av_top_index(source) + 1;

    WarnContext::Item &warn_cxt = warn_context->push_level(WarnContext::Array);
    for (int i = 0; i < size; ++i) {
        warn_cxt.index = i;
        SV **item = av_fetch(source, i, 0);
        if (!item)
            return false;

        if (!setter(aTHX_ &sub, fd, getter(aTHX_ *item)))
            return false;
    }
    warn_context->pop_level();

    return sink->EndSequence(fd.selector.seq_end);
}

bool Mapper::encode_from_message_array(Sink *sink, Status *status, const Mapper::Field &fd, AV *source) const {
    int size = av_top_index(source) + 1;
    Sink sub;

    if (!sink->StartSequence(fd.selector.seq_start, &sub))
        return false;

    WarnContext::Item &warn_cxt = warn_context->push_level(WarnContext::Array);
    for (int i = 0; i < size; ++i) {
        warn_cxt.index = i;
        SV **item = av_fetch(source, i, 0);
        if (!item)
            return false;
        Sink submsg;

        SvGETMAGIC(*item);
        if (!sub.StartSubMessage(fd.selector.msg_start, &submsg))
            return false;
        if (!encode(&submsg, status, *item))
            return false;
        if (!sub.EndSubMessage(fd.selector.msg_end))
            return false;
    }
    warn_context->pop_level();

    return sink->EndSequence(fd.selector.seq_end);
}

namespace {
    HE *hv_fetch_ent_tied(pTHX_ HV *hv, SV *name, I32 lval, U32 hash) {
        if (!hv_exists_ent(hv, name, hash))
            return NULL;

        return hv_fetch_ent(hv, name, lval, hash);
    }
}

bool Mapper::encode(Sink *sink, Status *status, SV *ref) const {
    SvGETMAGIC(ref);
    if (!SvROK(ref) || SvTYPE(SvRV(ref)) != SVt_PVHV)
        croak("Not an hash reference when encoding a %s value", message_def->full_name());
    HV *hv = (HV *) SvRV(ref);

    if (!sink->StartMessage())
        return false;

    bool tied = SvTIED_mg((SV *) hv, PERL_MAGIC_tied);
    bool ok = true;
    int last_oneof = -1;
    WarnContext::Item &warn_cxt = warn_context->push_level(WarnContext::Message);
    for (vector<Field>::const_iterator it = fields.begin(), en = fields.end(); it != en; ++it) {
        warn_cxt.field = &*it;
        HE *he = tied ? hv_fetch_ent_tied(aTHX_ hv, it->name, 0, it->name_hash) :
                        hv_fetch_ent(hv, it->name, 0, it->name_hash);

        if (!he) {
            if (it->field_def->label() == UPB_LABEL_REQUIRED) {
                status->SetFormattedErrorMessage(
                    "Missing required field '%s'",
                    it->full_name().c_str());
                return false;
            } else
                continue;
        } else if (it->oneof_index != -1) {
            if (it->oneof_index == last_oneof)
                continue;
            else
                last_oneof = it->oneof_index;
        }

        if (it->is_map)
            ok = ok && encode_from_perl_hash(sink, status, *it, HeVAL(he));
        else if (it->field_def->label() == UPB_LABEL_REPEATED)
            ok = ok && encode_from_perl_array(sink, status, *it, HeVAL(he));
        else if (encode_defaults || !it->has_default)
            ok = ok && encode(sink, status, *it, HeVAL(he));
        else
            ok = ok && encode_nodefaults(sink, status, *it, HeVAL(he));
    }
    warn_context->pop_level();

    if (!sink->EndMessage(status))
        return false;

    return ok;
}

bool Mapper::encode(Sink *sink, Status *status, const Field &fd, SV *ref) const {
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
        if (!fd.mapper->encode(&sub, status, ref))
            return false;
        return sink->EndSubMessage(fd.selector.msg_end);
    }
    case UPB_TYPE_ENUM: {
        IV value = SvIV(ref);
        if (check_enum_values &&
                fd.enum_values.find(value) == fd.enum_values.end()) {
            status->SetFormattedErrorMessage(
                "Invalid enumeration value %d for field '%s'",
                value,
                fd.full_name().c_str()
            );
            return false;
        }

        return sink->PutInt32(fd.selector.primitive, value);
    }
    case UPB_TYPE_INT32:
        return sink->PutInt32(fd.selector.primitive, SvIV(ref));
    case UPB_TYPE_UINT32:
        return sink->PutUInt32(fd.selector.primitive, SvUV(ref));
    case UPB_TYPE_INT64:
        if (sizeof(IV) >= sizeof(int64_t))
            return sink->PutInt64(fd.selector.primitive, SvIV(ref));
        else
            return sink->PutInt64(fd.selector.primitive, get_int64(aTHX_ ref));
    case UPB_TYPE_UINT64:
        if (sizeof(UV) >= sizeof(int64_t))
            return sink->PutInt64(fd.selector.primitive, SvUV(ref));
        else
            return sink->PutUInt64(fd.selector.primitive, get_uint64(aTHX_ ref));
    default:
        return false; // just in case
    }
}

bool Mapper::encode_nodefaults(Sink *sink, Status *status, const Field &fd, SV *ref) const {
    switch (fd.field_def->type()) {
    case UPB_TYPE_FLOAT: {
        NV value = SvNV(ref);
        if (value == fd.default_nv)
            return true;
        return sink->PutFloat(fd.selector.primitive, value);
    }
    case UPB_TYPE_DOUBLE: {
        NV value = SvNV(ref);
        if (value == fd.default_nv)
            return true;
        return sink->PutDouble(fd.selector.primitive, value);
    }
    case UPB_TYPE_BOOL: {
        bool value = SvNV(ref);
        if (value == fd.default_bool)
            return true;
        return sink->PutBool(fd.selector.primitive, value);
    }
    case UPB_TYPE_STRING:
    case UPB_TYPE_BYTES: {
        STRLEN len;
        const char *str = fd.field_def->type() == UPB_TYPE_STRING ? SvPVutf8(ref, len) : SvPV(ref, len);
        if (len == fd.default_str_len &&
                (len == 0 || memcmp(str, fd.default_str, len) == 0))
            return true;
        Sink sub;
        if (!sink->StartString(fd.selector.str_start, len, &sub))
            return false;
        sub.PutStringBuffer(fd.selector.str_cont, str, len, NULL);
        return sink->EndString(fd.selector.str_end);
    }
    case UPB_TYPE_ENUM: {
        IV value = SvIV(ref);
        if (value == fd.default_iv)
            return true;
        if (check_enum_values &&
                fd.enum_values.find(value) == fd.enum_values.end()) {
            status->SetFormattedErrorMessage(
                "Invalid enumeration value %d for field '%s'",
                value,
                fd.full_name().c_str()
            );
            return false;
        }

        return sink->PutInt32(fd.selector.primitive, value);
    }
    case UPB_TYPE_INT32: {
        IV value = SvIV(ref);
        if (value == fd.default_iv)
            return true;
        return sink->PutInt32(fd.selector.primitive, value);
    }
    case UPB_TYPE_UINT32: {
        UV value = SvUV(ref);
        if (value == fd.default_uv)
            return true;
        return sink->PutUInt32(fd.selector.primitive, value);
    }
    case UPB_TYPE_INT64: {
        int64_t value = sizeof(IV) >= sizeof(int64_t) ? SvIV(ref) : get_int64(aTHX_ ref);
        if (value == fd.default_i64)
            return true;
        return sink->PutInt64(fd.selector.primitive, value);
    }
    case UPB_TYPE_UINT64: {
        uint64_t value = sizeof(UV) >= sizeof(int64_t) ? SvUV(ref) : get_uint64(aTHX_ ref);
        if (value == fd.default_u64)
            return true;
        return sink->PutUInt64(fd.selector.primitive, value);
    }
    default:
        return false; // just in case
    }
}

bool Mapper::encode_key(Sink *sink, Status *status, const Field &fd, const char *key, I32 keylen) const {
    switch (fd.field_def->type()) {
    case UPB_TYPE_BOOL: {
        // follows what SvTRUE() does for strings
        bool bval = keylen > 1 || keylen == 1 && key[0] != '0';
        return sink->PutBool(fd.selector.primitive, bval);
    }
    case UPB_TYPE_STRING: {
        Sink sub;
        if (!sink->StartString(fd.selector.str_start, keylen, &sub))
            return false;
        sub.PutStringBuffer(fd.selector.str_cont, key, keylen, NULL);
        return sink->EndString(fd.selector.str_end);
    }
    case UPB_TYPE_INT32:
        return sink->PutInt32(fd.selector.primitive, key_iv(aTHX_ key, keylen));
    case UPB_TYPE_UINT32:
        return sink->PutUInt32(fd.selector.primitive, key_uv(aTHX_ key, keylen));
    case UPB_TYPE_INT64:
        return sink->PutInt64(fd.selector.primitive, key_iv(aTHX_ key, keylen));
    case UPB_TYPE_UINT64:
        return sink->PutInt64(fd.selector.primitive, key_uv(aTHX_ key, keylen));
    default:
        return false; // just in case
    }
}

bool Mapper::encode_hash_kv(Sink *sink, Status *status, const char *key, STRLEN keylen, SV *value) const {
    if (!sink->StartMessage())
        return false;
    if (fields[0].is_key) {
        if (!encode_key(sink, status, fields[0], key, keylen))
            return false;
        if (!encode(sink, status, fields[1], value))
            return false;
    } else {
        if (!encode_key(sink, status, fields[1], key, keylen))
            return false;
        if (!encode(sink, status, fields[0], value))
            return false;
    }
    if (!sink->EndMessage(status))
        return false;
    return true;
}

bool Mapper::encode_from_perl_hash(Sink *sink, Status *status, const Field &fd, SV *ref) const {
    SvGETMAGIC(ref);
    if (!SvROK(ref) || SvTYPE(SvRV(ref)) != SVt_PVHV)
        croak("Not an hash reference when encoding field '%s'", fd.full_name().c_str());
    HV *hash = (HV *) SvRV(ref);
    Sink repeated;

    if (!sink->StartSequence(fd.selector.seq_start, &repeated))
        return false;

    hv_iterinit(hash);
    WarnContext::Item &warn_cxt = warn_context->push_level(WarnContext::Hash);
    while (HE *entry = hv_iternext(hash)) {
        Sink key_value;
        SV *value = HeVAL(entry);
        const char *key;
        STRLEN keylen;
        bool needs_free = false;

        if (HeKLEN(entry) == HEf_SVKEY) {
            key = SvPVutf8(HeKEY_sv(entry), keylen);
        } else {
            if (HeKUTF8(entry)) {
                key = HeKEY(entry);
                keylen = HeKLEN(entry);
            } else {
                keylen = HeKLEN(entry);
                key = (const char *) bytes_to_utf8((U8*) HeKEY(entry), &keylen);
                SAVEFREEPV(key);
            }
        }

        warn_cxt.key = key;
        warn_cxt.keylen = keylen;
        if (!repeated.StartSubMessage(fd.selector.msg_start, &key_value))
            return false;
        if (!fd.mapper->encode_hash_kv(&key_value, status, key, keylen, value))
            return false;
        if (!repeated.EndSubMessage(fd.selector.msg_end))
            return false;
    }
    warn_context->pop_level();

    return sink->EndSequence(fd.selector.seq_end);
}

bool Mapper::encode_from_perl_array(Sink *sink, Status *status, const Field &fd, SV *ref) const {
    SvGETMAGIC(ref);
    if (!SvROK(ref) || SvTYPE(SvRV(ref)) != SVt_PVAV)
        croak("Not an array reference when encoding field '%s'", fd.full_name().c_str());
    AV *array = (AV *) SvRV(ref);

    switch (fd.field_def->type()) {
    case UPB_TYPE_FLOAT:
        return encode_from_array<NVGetter, FloatEmitter>(sink, fd, array);
    case UPB_TYPE_DOUBLE:
        return encode_from_array<NVGetter, DoubleEmitter>(sink, fd, array);
    case UPB_TYPE_BOOL:
        return encode_from_array<BoolGetter, BoolEmitter>(sink, fd, array);
    case UPB_TYPE_STRING:
        return encode_from_array<SVGetter, StringEmitter>(sink, fd, array);
    case UPB_TYPE_BYTES:
        return encode_from_array<SVGetter, StringEmitter>(sink, fd, array);
    case UPB_TYPE_MESSAGE:
        return fd.mapper->encode_from_message_array(sink, status, fd, array);
    case UPB_TYPE_ENUM:
        if (check_enum_values)
            return encode_from_array<IVGetter, EnumEmitter>(sink, status, fd, array);
        else
            return encode_from_array<IVGetter, Int32Emitter>(sink, fd, array);
    case UPB_TYPE_INT32:
        return encode_from_array<IVGetter, Int32Emitter>(sink, fd, array);
    case UPB_TYPE_UINT32:
        return encode_from_array<UVGetter, UInt32Emitter>(sink, fd, array);
    case UPB_TYPE_INT64:
        if (sizeof(IV) >= sizeof(int64_t))
            return encode_from_array<IVGetter, Int64Emitter>(sink, fd, array);
        else
            return encode_from_array<I64Getter, Int64Emitter>(sink, fd, array);
    case UPB_TYPE_UINT64:
        if (sizeof(IV) >= sizeof(int64_t))
            return encode_from_array<UVGetter, UInt64Emitter>(sink, fd, array);
        else
            return encode_from_array<U64Getter, UInt64Emitter>(sink, fd, array);
    default:
        return false; // just in case
    }
}

bool Mapper::check_from_message_array(Status *status, const Mapper::Field &fd, AV *source) const {
    int size = av_top_index(source) + 1;

    for (int i = 0; i < size; ++i) {
        SV **item = av_fetch(source, i, 0);
        if (!item)
            return false;

        SvGETMAGIC(*item);
        if (!check(status, *item))
            return false;
    }

    return true;
}

bool Mapper::check_from_enum_array(Status *status, const Mapper::Field &fd, AV *source) const {
    int size = av_top_index(source) + 1;

    for (int i = 0; i < size; ++i) {
        SV **item = av_fetch(source, i, 0);
        if (!item)
            return false;

        IV value = SvIV(*item);
        if (fd.enum_values.find(value) == fd.enum_values.end()) {
            status->SetFormattedErrorMessage(
                "Invalid enumeration value %d for field '%s'",
                value,
                fd.full_name().c_str()
            );
            return false;
        }
    }

    return true;
}

bool Mapper::check(Status *status, SV *ref) const {
    SvGETMAGIC(ref);
    if (!SvROK(ref) || SvTYPE(SvRV(ref)) != SVt_PVHV)
        croak("Not an hash reference when checking a %s value", message_def->full_name());
    HV *hv = (HV *) SvRV(ref);

    I32 count = hv_iterinit(hv);
    bool ok = true;
    for (int i = 0; i < count; ++i) {
        char *key;
        I32 keylen;
        SV *value = hv_iternextsv(hv, &key, &keylen);
        // if the key is marked as UTF-8 and contains non-ASCII characters,
        // it will not be there anyway in the lookup
        string name(key, keylen < 0 ? -keylen : keylen);
        STD_TR1::unordered_map<string, Field *>::const_iterator it = field_map.find(name);

        if (it == field_map.end()) {
            status->SetFormattedErrorMessage(
                "Unknown field '%s' during check",
                name.c_str());
            return false;
        }

        Field *field = it->second;
        if (field->field_def->label() == UPB_LABEL_REPEATED)
            ok = ok && check_from_perl_array(status, *field, value);
        else
            ok = ok && check(status, *field, value);
    }

    return ok;
}

bool Mapper::check(Status *status, const Field &fd, SV *ref) const {
    switch (fd.field_def->type()) {
    case UPB_TYPE_MESSAGE:
        return fd.mapper->check(status, ref);
    case UPB_TYPE_ENUM: {
        if (!check_enum_values)
            return true;

        IV value = SvIV(ref);
        if (fd.enum_values.find(value) == fd.enum_values.end()) {
            status->SetFormattedErrorMessage(
                "Invalid enumeration value %d for field '%s'",
                value,
                fd.full_name().c_str()
            );
            return false;
        }

        return true;
    }
    default:
        // I doubt there is any point in performing "strict" type checks
        // on scalar values, due to coercion
        return true;
    }
}

bool Mapper::check_from_perl_array(Status *status, const Field &fd, SV *ref) const {
    SvGETMAGIC(ref);
    if (!SvROK(ref) || SvTYPE(SvRV(ref)) != SVt_PVAV)
        croak("Not an array reference when encoding field '%s'", fd.full_name().c_str());
    AV *array = (AV *) SvRV(ref);

    switch (fd.field_def->type()) {
    case UPB_TYPE_MESSAGE:
        return fd.mapper->check_from_message_array(status, fd, array);
    case UPB_TYPE_ENUM:
        if (check_enum_values)
            return check_from_enum_array(status, fd, array);
        else
            return true;
    default:
        // I doubt there is any point in performing "strict" type checks
        // on scalar values, due to coercion
        return true;
    }
}

MapperField::MapperField(pTHX_ const Mapper *_mapper, const Mapper::Field *_field) :
        field(_field),
        mapper(_mapper) {
    SET_THX_MEMBER;
    mapper->ref();
}

MapperField::~MapperField() {
    mapper->unref();
}

MapperField *MapperField::find_extension(pTHX_ CV *cv, SV *extension) {
    const Mapper *mapper = (const Mapper *) CvXSUBANY(cv).any_ptr;
    STRLEN len;
    const char *buffer = SvPV(extension, len);

    // ignore square brackets at beginning and end
    if (len > 2 && buffer[0] == '[' && buffer[len - 1] == ']') {
        len -= 2;
        buffer += 1;
    }

    string extension_name(buffer, len);
    MapperField *mapper_field = mapper->find_extension(extension_name);

    if (!mapper_field)
        croak("Unknown extension field '%s' for message '%s'", extension_name.c_str(), mapper->full_name());

    return mapper_field;
}

MapperField *MapperField::find_scalar_extension(pTHX_ CV *cv, SV *extension) {
    MapperField *mf = find_extension(aTHX_ cv, extension);
    if (mf && mf->is_repeated())
        croak("Extension field '%s' is a repeated field", mf->field->full_name().c_str());

    return mf;
}

MapperField *MapperField::find_repeated_extension(pTHX_ CV *cv, SV *extension) {
    MapperField *mf = find_extension(aTHX_ cv, extension);
    if (mf && !mf->is_repeated())
        croak("Extension field '%s' is a non-repeated field", mf->field->full_name().c_str());

    return mf;
}

SV *MapperField::get_read_field(HV *self) {
    HE *ent = hv_fetch_ent(self, field->name, 0, field->name_hash);

    return ent ? HeVAL(ent) : NULL;
}

SV *MapperField::get_write_field(HV *self) {
    HE *ent = hv_fetch_ent(self, field->name, 1, field->name_hash);

    return HeVAL(ent);
}

SV *MapperField::get_read_array_ref(HV *self) {
    HE *ent = hv_fetch_ent(self, field->name, 0, field->name_hash);

    if (!ent)
        return NULL;

    SV *ref = HeVAL(ent);
    if (!SvROK(ref) || SvTYPE(SvRV(ref)) != SVt_PVAV)
        croak("Value of field '%s' is not an array reference", field->full_name().c_str());

    return ref;
}

AV *MapperField::get_read_array(HV *self) {
    SV *ref = get_read_array_ref(self);

    return ref ? (AV *) SvRV(ref) : NULL;
}

AV *MapperField::get_write_array(HV *self) {
    HE *ent = hv_fetch_ent(self, field->name, 1, field->name_hash);
    SV *ref = HeVAL(ent);

    if (!SvOK(ref)) {
        AV *av = newAV();

        SvUPGRADE(ref, SVt_RV);
        SvROK_on(ref);
        SvRV_set(ref, (SV *) av);

        return av;
    } else {
        SV *ref = HeVAL(ent);
        if (!SvROK(ref) || SvTYPE(SvRV(ref)) != SVt_PVAV)
            croak("Value of field '%s' is not an array reference", field->full_name().c_str());

        return (AV *) SvRV(ref);
    }
}

SV *MapperField::get_read_hash_ref(HV *self) {
    HE *ent = hv_fetch_ent(self, field->name, 0, field->name_hash);

    if (!ent)
        return NULL;

    SV *ref = HeVAL(ent);
    if (!SvROK(ref) || SvTYPE(SvRV(ref)) != SVt_PVHV)
        croak("Value of field '%s' is not an hash reference", field->full_name().c_str());

    return ref;
}

HV *MapperField::get_read_hash(HV *self) {
    SV *ref = get_read_hash_ref(self);

    return ref ? (HV *) SvRV(ref) : NULL;
}

HV *MapperField::get_write_hash(HV *self) {
    HE *ent = hv_fetch_ent(self, field->name, 1, field->name_hash);
    SV *ref = HeVAL(ent);

    if (!SvOK(ref)) {
        HV *hv = newHV();

        SvUPGRADE(ref, SVt_RV);
        SvROK_on(ref);
        SvRV_set(ref, (SV *) hv);

        return hv;
    } else {
        SV *ref = HeVAL(ent);
        if (!SvROK(ref) || SvTYPE(SvRV(ref)) != SVt_PVHV)
            croak("Value of field '%s' is not an hash reference", field->full_name().c_str());

        return (HV *) SvRV(ref);
    }
}

const char *MapperField::name() {
    return field->field_def->name();
}

bool MapperField::is_repeated() {
    return field->field_def->label() == UPB_LABEL_REPEATED;
}

bool MapperField::is_extension() {
    return field->field_def->is_extension();
}

bool MapperField::is_map() {
    return field->is_map;
}

bool MapperField::has_field(HV *self) {
    return hv_fetch_ent(self, field->name, 0, field->name_hash);
}

void MapperField::clear_field(HV *self) {
    hv_delete_ent(self, field->name, G_DISCARD, field->name_hash);
}

SV *MapperField::get_scalar(HV *self, SV *target) {
    SV *value = get_read_field(self);

    if (value) {
        return value;
    } else {
        copy_default(target);

        return target;
    }
}

void MapperField::copy_default(SV *target) {
    const FieldDef *field_def = field->field_def;

    switch (field_def->type()) {
    case UPB_TYPE_FLOAT:
        sv_setnv(target, field_def->default_float());
        break;
    case UPB_TYPE_DOUBLE:
        sv_setnv(target, field_def->default_double());
        break;
    case UPB_TYPE_BOOL:
        set_bool(aTHX_ target, field_def->default_bool());
        break;
    case UPB_TYPE_STRING: {
        size_t len;
        const char *str = field_def->default_string(&len);

        sv_setpvn(target, str, len);
        SvUTF8_on(target);
    }
        break;
    case UPB_TYPE_BYTES: {
        size_t len;
        const char *str = field_def->default_string(&len);

        sv_setpvn(target, str, len);
    }
        break;
    case UPB_TYPE_MESSAGE:
        sv_setsv(target, &PL_sv_undef);
        break;
    case UPB_TYPE_ENUM: {
        sv_setiv(target, field_def->default_int32());
    }
        break;
    case UPB_TYPE_INT32:
        sv_setiv(target, field_def->default_int32());
        break;
    case UPB_TYPE_UINT32:
        sv_setuv(target, field_def->default_uint32());
        break;
    case UPB_TYPE_INT64: {
        if (sizeof(IV) >= sizeof(int64_t))
            sv_setiv(target, field_def->default_int64());
        else {
            int64_t i64 = field_def->default_int64();

            set_bigint(aTHX_ target, (uint64_t) i64, i64 < 0);
        }
    }
        break;
    case UPB_TYPE_UINT64: {
        if (sizeof(IV) >= sizeof(uint64_t))
            sv_setuv(target, field_def->default_uint64());
        else {
            int64_t u64 = field_def->default_uint64();

            set_bigint(aTHX_ target, u64, false);
        }
    }
        break;
    default:
        croak("Unhandled field type %d for field '%s'", field->field_def->type(), field->full_name().c_str());
    }
}

void MapperField::clear_oneof(HV *self) {
    for (int i = 0, max = mapper->field_count(); i < max; ++i) {
        const Mapper::Field *other = mapper->get_field(i);

        if (other == field)
            continue;
        hv_delete_ent(self, other->name, G_DISCARD, other->name_hash);
    }
}

void MapperField::set_scalar(HV *self, SV *value) {
    if (field->oneof_index != -1)
        clear_oneof(self);

    SV *target = get_write_field(self);

    copy_value(target, value);
}

void MapperField::copy_value(SV *target, SV *value) {
    const FieldDef *field_def = field->field_def;

    switch (field->is_map ? field->map_value_type() : field_def->type()) {
    case UPB_TYPE_FLOAT:
        sv_setnv(target, SvNV(value));
        break;
    case UPB_TYPE_DOUBLE:
        sv_setnv(target, SvNV(value));
        break;
    case UPB_TYPE_BOOL:
        set_bool(aTHX_ target, SvTRUE(value));
        break;
    case UPB_TYPE_STRING: {
        STRLEN len;
        const char *str = SvPVutf8(value, len);

        sv_setpvn(target, str, len);
        SvUTF8_on(target);
    }
        break;
    case UPB_TYPE_BYTES: {
        STRLEN len;
        const char *str = SvPV(value, len);

        sv_setpvn(target, str, len);
    }
        break;
    case UPB_TYPE_MESSAGE:
        if (SvOK(value) && (!SvROK(value) || SvTYPE(SvRV(value)) != SVt_PVHV))
            croak("Value for message field '%s' is not an hash reference", field->full_name().c_str());
        sv_setsv(target, value);
        break;
    case UPB_TYPE_ENUM: {
        I32 i32 = SvIV(value);
        const STD_TR1::unordered_set<int32_t> &enum_values = field->is_map ?
            field->map_enum_values() :
            field->enum_values;
        if (enum_values.size() &&
                enum_values.find(i32) == field->enum_values.end())
            croak("Invalid value %d for enumeration field '%s'", i32, field->full_name().c_str());
        sv_setiv(target, i32);
    }
        break;
    case UPB_TYPE_INT32:
        sv_setiv(target, SvIV(value));
        break;
    case UPB_TYPE_UINT32:
        sv_setuv(target, SvUV(value));
        break;
    case UPB_TYPE_INT64: {
        if (sizeof(IV) >= sizeof(int64_t))
            sv_setiv(target, SvIV(value));
        else {
            int64_t i64 = get_int64(aTHX_ value);

            set_bigint(aTHX_ target, (uint64_t) i64, i64 < 0);
        }
    }
        break;
    case UPB_TYPE_UINT64: {
        if (sizeof(IV) >= sizeof(uint64_t))
            sv_setuv(target, SvUV(value));
        else {
            int64_t u64 = get_uint64(aTHX_ value);

            set_bigint(aTHX_ target, u64, false);
        }
    }
        break;
    default:
        croak("Unhandled field type %d for field '%s'", field->field_def->type(), field->full_name().c_str());
    }
}

SV *MapperField::get_item(HV *self, int index, SV *target) {
    AV *array = get_read_array(self);

    if (!array)
        croak("Accessing unset array field '%s'", field->full_name().c_str());
    int max = av_top_index(array);
    if (max == -1)
        croak("Accessing empty array field '%s'", field->full_name().c_str());
    if (index > max || index < -(max + 1))
        croak("Accessing out-of-bounds index %d for field '%s'", index, field->full_name().c_str());
    SV **value = av_fetch(array, index, 0);

    if (value) {
        return *value;
    } else {
        copy_default(target);

        return target;
    }
}

SV *MapperField::get_item(HV *self, SV *key, SV *target) {
    HV *hash = get_read_hash(self);

    if (!hash)
        croak("Accessing unset map field '%s'", field->full_name().c_str());
    if (HvTOTALKEYS(hash) == 0)
        croak("Accessing empty map field '%s'", field->full_name().c_str());
    HE *value = hv_fetch_ent(hash, key, 0, 0);

    if (value) {
        return HeVAL(value);
    } else {
        croak("Accessing non-existing key '%s' for field '%s'", SvPV_nolen(key), field->full_name().c_str());
    }
}

void MapperField::set_item(HV *self, int index, SV *value) {
    AV *array = get_write_array(self);
    SV **target = av_fetch(array, index, 1);

    copy_value(*target, value ? value : NULL);
}

void MapperField::set_item(HV *self, SV *key, SV *value) {
    HV *hash = get_write_hash(self);
    HE *target = hv_fetch_ent(hash, key, 1, 0);

    copy_value(HeVAL(target), value ? value : NULL);
}

void MapperField::add_item(HV *self, SV *value) {
    AV *array = get_write_array(self);
    SV **target = av_fetch(array, av_top_index(array) + 1, 1);

    copy_value(*target, value ? value : NULL);
}

int MapperField::list_size(HV *self) {
    AV *array = get_read_array(self);

    if (!array)
        return 0;

    return av_top_index(array) + 1;
}

SV *MapperField::get_list(HV *self) {
    SV *array_ref = get_read_array_ref(self);

    return array_ref ? array_ref : &PL_sv_undef;
}

void MapperField::set_list(HV *self, SV *ref) {
    if (!SvROK(ref) || SvTYPE(SvRV(ref)) != SVt_PVAV)
        croak("Value for field '%s' is not an array reference", field->full_name().c_str());
    SV *field_ref = get_write_field(self);

    if (!SvOK(field_ref)) {
        SvUPGRADE(field_ref, SVt_RV);
        SvROK_on(field_ref);
    } else {
        if (!SvROK(field_ref))
            croak("Value of field '%s' is not a reference", field->full_name().c_str());
        SvREFCNT_dec(SvRV(field_ref));
    }
    SvRV_set(field_ref, SvRV(ref));
    SvREFCNT_inc(SvRV(field_ref));
}

SV *MapperField::get_map(HV *self) {
    SV *hash_ref = get_read_hash_ref(self);

    return hash_ref ? hash_ref : &PL_sv_undef;
}

void MapperField::set_map(HV *self, SV *ref) {
    if (!SvROK(ref) || SvTYPE(SvRV(ref)) != SVt_PVHV)
        croak("Value for field '%s' is not an hash reference", field->full_name().c_str());
    SV *field_ref = get_write_field(self);

    if (!SvOK(field_ref)) {
        SvUPGRADE(field_ref, SVt_RV);
        SvROK_on(field_ref);
    } else {
        if (!SvROK(field_ref))
            croak("Value of field '%s' is not a reference", field->full_name().c_str());
        SvREFCNT_dec(SvRV(field_ref));
    }
    SvRV_set(field_ref, SvRV(ref));
    SvREFCNT_inc(SvRV(field_ref));
}

EnumMapper::EnumMapper(pTHX_ Dynamic *_registry, const upb::EnumDef *_enum_def) :
        registry(_registry),
        enum_def(_enum_def) {
    SET_THX_MEMBER;

    registry->ref();
}

EnumMapper::~EnumMapper() {
    // make sure this only goes away after inner destructors have completed
    refcounted_mortalize(aTHX_ registry);
}

SV *EnumMapper::enum_descriptor() const {
    SV *ref = newSV(0);

    sv_setref_iv(ref, "Google::ProtocolBuffers::Dynamic::EnumDef", (IV) enum_def);

    return ref;
}

WarnContext::WarnContext(pTHX) : chained_handler(NULL) {
    CV *handler = get_cv("Google::ProtocolBuffers::Dynamic::Mapper::handle_warning", 0);

    warn_handler = (SV*) handler;
}

void WarnContext::setup(pTHX) {
    CV *handler = get_cv("Google::ProtocolBuffers::Dynamic::Mapper::handle_warning", 0);

    CvXSUBANY(handler).any_ptr = new WarnContext(aTHX);
}

WarnContext *WarnContext::get(pTHX) {
    CV *handler = get_cv("Google::ProtocolBuffers::Dynamic::Mapper::handle_warning", 0);

    return (WarnContext *) CvXSUBANY(handler).any_ptr;
}

void WarnContext::localize_warning_handler(pTHX) {
    chained_handler = PL_warnhook;
    SAVEGENERICSV(PL_warnhook);
    PL_warnhook = SvREFCNT_inc_simple_NN(warn_handler);
}

void WarnContext::warn_with_context(pTHX_ SV *warning) const {
    SV *cxt = sv_2mortal(newSVpvs("While encoding field '"));

    for (Levels::const_iterator it = levels.begin(), en = levels.end(); it != en; ++it) {
        switch (it->kind) {
        case Array:
            sv_catpvf(cxt, "[%d].", it->index);
            break;
        case Hash:
            sv_catpvf(cxt, "{%" SVf "}.", it->index);
            break;
        case Message:
            sv_catpvf(cxt, "%" SVf ".", it->field->name);
            break;
        }
    }

    SvCUR_set(cxt, SvCUR(cxt) - 1); // chop last '.'

    sv_catpvs(cxt, "': ");
    sv_catsv(cxt, warning);

    if (chained_handler) {
        dSP;

        PUSHMARK(SP);
        XPUSHs(cxt);
        PUTBACK;

        call_sv(chained_handler, G_DISCARD|G_VOID);
    } else
        warn_sv(cxt);
}

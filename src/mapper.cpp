#include "mapper.h"
#include "dynamic.h"
#include "servicedef.h"

#include "perl_unpollute.h"

#include <upb/pb/encoder.h>
#include <upb/pb/decoder.h>

using namespace gpd;
using namespace gpd::transform;
using namespace std;
using namespace upb;
using namespace upb::pb;
using namespace upb::json;

#ifdef MULTIPLICITY
#    define THX_DECLARE_AND_GET tTHX my_perl = cxt->my_perl
#else
#    define THX_DECLARE_AND_GET
#endif

#define HAS_FULL_NOMG (PERL_VERSION >= 14)

namespace {
    void unref_on_scope_leave(void *ref) {
        ((Refcounted *) ref)->unref();
    }

    void refcounted_mortalize(pTHX_ const Refcounted *ref) {
        SAVEDESTRUCTOR(unref_on_scope_leave, ref);
    }

    void delete_upb_env(upb::Environment *env) {
        delete env;
    }

    upb::Environment *make_localized_environment(pTHX_ upb::Status *report_errors_to) {
        upb::Environment *env = new upb::Environment();

        env->ReportErrorsTo(report_errors_to);
        SAVEDESTRUCTOR(delete_upb_env, env);

        return env;
    }
}

Mapper::DecoderHandlers::DecoderHandlers(pTHX_ const Mapper *mapper) :
        target_ref(NULL),
        decoder_transform(NULL),
        transform_fieldtable(false),
        pending_transforms(aTHX) {
    SET_THX_MEMBER;
    mappers.push_back(mapper);
}

void Mapper::DecoderHandlers::push_mapper(const Mapper *mapper) {
    mappers.push_back(mapper);
    track_seen_fields = mapper->get_track_seen_fields();
}

void Mapper::DecoderHandlers::pop_mapper() {
    mappers.pop_back();
    track_seen_fields = mappers.back()->get_track_seen_fields();
}

void Mapper::DecoderHandlers::prepare(HV *target) {
    track_seen_fields = mappers[0]->get_track_seen_fields();

    mappers.resize(1);
    if (track_seen_fields) {
        seen_fields.resize(1);
        seen_fields.back().clear();
        seen_fields.back().resize(mappers.back()->fields.size());
    }
    if (int oneof_count = mappers.back()->oneof_count) {
        seen_oneof.resize(1);
        seen_oneof.back().clear();
        seen_oneof.back().resize(oneof_count, -1);
    }
    items.resize(1);
    items[0] = (SV *) target;
    map_keys.clear();
    fieldtable_entries.clear();
    target_ref = newRV_noinc(items[0]);
    error.clear();
    string = NULL;

    SAVEDESTRUCTOR(&DecoderTransformQueue::static_clear, &pending_transforms);
    SAVEDESTRUCTOR(&DecoderHandlers::static_clear, this);

    pending_transforms.clear();
    if (decoder_transform) {
        if (transform_fieldtable) {
            add_transform_fieldtable(target_ref, decoder_transform, NULL);
        } else {
            pending_transforms.add_transform(target_ref, decoder_transform, NULL);
        }
    }

    if (mappers[0]->get_decode_blessed())
        sv_bless(target_ref, mappers[0]->stash);
}

void Mapper::DecoderHandlers::finish() {
    if (transform_fieldtable) {
        finish_add_transform_fieldtable();
    }

    // this should be a no-op that only resizes the items vector
    DecoderHandlers::static_clear(this);
    // this can croak()
    apply_transforms();
}

void Mapper::DecoderHandlers::static_clear(DecoderHandlers *cxt) {
    THX_DECLARE_AND_GET;

    for (vector<SV *>::iterator it = cxt->items.begin(), en = cxt->items.end(); it != en; ++it) {
        SV *item = *it;

        // This can happen for the last entry (map value has not been decoded)
        if (!item)
            continue;
        // AVs/HVs are linked into the top-level target when they are created,
        // while map key/value need to be cleaned up manually
        if (SvTYPE(item) != SVt_PVHV && SvTYPE(item) != SVt_PVAV) {
            SvREFCNT_dec(item);
        }
    }

    cxt->items.clear();

    for (vector<MapKey>::iterator it = cxt->map_keys.begin(), en = cxt->map_keys.end(); it != en; ++it) {
        SvREFCNT_dec(it->key_sv);
    }

    cxt->map_keys.clear();

    for (vector<Fieldtable::Entry>::iterator it = cxt->fieldtable_entries.begin(), en = cxt->fieldtable_entries.end(); it != en; ++it) {
        SvREFCNT_dec(it->value);
    }

    cxt->fieldtable_entries.clear();
}

SV *Mapper::DecoderHandlers::get_and_mortalize_target() {
    return sv_2mortal(target_ref);
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

#if PERL_VERSION < 14
    inline void GPD_warn_sv(pTHX_ SV *mess) {
        warn("%" SVf, mess);
    }
    #define warn_sv(sv) GPD_warn_sv(aTHX_ (sv))
#endif

    inline void set_perl_bool(pTHX_ SV *target, bool value) {
        if (value)
            sv_setiv(target, 1);
        else
            sv_setpvn(target, "", 0);
    }

    inline void set_numeric_bool(pTHX_ SV *target, bool value) {
        sv_setiv(target, value ? 1 : 0);
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

    return map_fields[1].is_map_value() ?
        map_fields[1].field_def->type() :
        map_fields[0].field_def->type();
}

const STD_TR1::unordered_set<int32_t> &Mapper::Field::map_enum_values() const {
    const vector<Field> &map_fields = mapper->fields;

    return map_fields[1].is_map_value() ?
        map_fields[1].enum_values :
        map_fields[0].enum_values;
}

bool Mapper::DecoderHandlers::apply_defaults_and_check() {
    const Mapper *mapper = mappers.back();
    bool decode_explicit_defaults = mapper->decode_explicit_defaults;
    bool check_required_fields = mapper->check_required_fields;

    if (!decode_explicit_defaults && !check_required_fields)
        return true;

    const vector<bool> &seen = seen_fields.back();
    const vector<Mapper::Field> &fields = mapper->fields;

    for (int i = 0, n = fields.size(); i < n; ++i) {
        const Mapper::Field &field = fields[i];
        bool field_seen = seen[i];

        if (!field_seen && decode_explicit_defaults && field.has_default) {
            SV *target = get_target(&i);

            // this is the case where we merged multiple message instances,
            // so defaults have already been applied
            if (SvOK(target))
                continue;

            mapper->apply_default(field, target);
        } else if (!field_seen && check_required_fields && field.field_def->label() == UPB_LABEL_REQUIRED) {
            error = "Missing required field " + field.full_name();

            return false;
        }
    }

    return true;
}

bool Mapper::DecoderHandlers::on_end_message(DecoderHandlers *cxt, upb::Status *status) {
    if (!cxt->track_seen_fields)
        return true;
    else if (!status || status->ok()) {
        return cxt->apply_defaults_and_check();
    } else
        return false;
}

Mapper::DecoderHandlers *Mapper::DecoderHandlers::on_start_string(DecoderHandlers *cxt, const int *field_index, size_t size_hint) {
    THX_DECLARE_AND_GET;

    cxt->mark_seen(field_index);
    cxt->string = cxt->get_target(field_index);
    sv_grow(cxt->string, size_hint);
    SvPOK_on(cxt->string);
    SvCUR_set(cxt->string, 0);

    return cxt;
}

size_t Mapper::DecoderHandlers::on_append_string(DecoderHandlers *cxt, const int *field_index, const char *buf, size_t len) {
    THX_DECLARE_AND_GET;

    STRLEN cur = SvCUR(cxt->string);
    memcpy(SvPVX(cxt->string) + cur, buf, len);
    SvCUR_set(cxt->string, cur + len);

    return len;
}

bool Mapper::DecoderHandlers::on_end_string(DecoderHandlers *cxt, const int *field_index) {
    const Mapper *mapper = cxt->mappers.back();
    if (mapper->fields[*field_index].field_def->type() == UPB_TYPE_STRING)
        SvUTF8_on(cxt->string);
    cxt->string = NULL;

    return true;
}

void Mapper::DecoderHandlers::on_string(DecoderHandlers *cxt, const int *field_index, const char *buf, size_t len, bool is_utf8) {
    THX_DECLARE_AND_GET;

    cxt->mark_seen(field_index);

    SV *string = cxt->get_target(field_index);
    char *pv = sv_grow(string, len);
    SvPOK_on(string);
    memcpy(pv, buf, len);
    SvCUR_set(string, len);
    if (is_utf8)
        SvUTF8_on(string);
}

size_t Mapper::DecoderHandlers::on_string_key(DecoderHandlers *cxt, const int *field_index, const char *buf, size_t len) {
    cxt->map_keys.back().set_buffer(buf, len);

    return len;
}

Mapper::DecoderHandlers *Mapper::DecoderHandlers::on_start_sequence(DecoderHandlers *cxt, const int *field_index) {
    THX_DECLARE_AND_GET;

    cxt->mark_seen(field_index);
    SV *target = cxt->get_hash_item_target(field_index);
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
    SV *target = cxt->get_hash_item_target(field_index);
    HV *hv = NULL;

    if (!SvROK(target)) {
        hv = newHV();

        SvUPGRADE(target, SVt_RV);
        SvROK_on(target);
        SvRV_set(target, (SV *) hv);
    } else
        hv = (HV *) SvRV(target);

    cxt->push_mapper(mapper->fields[*field_index].mapper);
    cxt->items.push_back((SV *) hv);
    cxt->items.push_back(NULL);
    cxt->map_keys.push_back(MapKey(newSV(0)));

    return cxt;
}

bool Mapper::DecoderHandlers::on_end_map(DecoderHandlers *cxt, const int *field_index) {
    THX_DECLARE_AND_GET;

    cxt->pop_mapper();

    cxt->items.pop_back();
    cxt->items.pop_back();

    SvREFCNT_dec(cxt->map_keys.back().key_sv);
    cxt->map_keys.pop_back();

    return true;
}

Mapper::DecoderHandlers *Mapper::DecoderHandlers::on_start_sub_message(DecoderHandlers *cxt, const int *field_index) {
    THX_DECLARE_AND_GET;

    cxt->mark_seen(field_index);
    const Mapper *mapper = cxt->mappers.back();
    const Mapper *message_mapper = mapper->fields[*field_index].mapper;
    SV *target = cxt->get_target(field_index);

    if (!message_mapper->decoder_callbacks.transform_fieldtable) {
        HV *hv = NULL;

        if (!SvROK(target)) {
            hv = newHV();

            SvUPGRADE(target, SVt_RV);
            SvROK_on(target);
            SvRV_set(target, (SV *) hv);
        } else
            hv = (HV *) SvRV(target);

        cxt->items.push_back((SV *) hv);

        cxt->maybe_add_transform(
            target,
            message_mapper->decoder_callbacks.decoder_transform,
            mapper->fields[*field_index].decoder_transform);
    } else {
        cxt->add_transform_fieldtable(
            target,
            message_mapper->decoder_callbacks.decoder_transform,
            mapper->fields[*field_index].decoder_transform);

        cxt->items.push_back((SV *) target);
    }

    cxt->push_mapper(message_mapper);
    if (cxt->track_seen_fields) {
        cxt->seen_fields.resize(cxt->seen_fields.size() + 1);
        cxt->seen_fields.back().resize(cxt->mappers.back()->fields.size());
    }
    if (int oneof_count = cxt->mappers.back()->oneof_count) {
        cxt->seen_oneof.resize(cxt->seen_oneof.size() + 1);
        cxt->seen_oneof.back().resize(oneof_count, -1);
    }
    if (message_mapper->get_decode_blessed())
        sv_bless(target, message_mapper->stash);

    return cxt;
}

bool Mapper::DecoderHandlers::on_end_sub_message(DecoderHandlers *cxt, const int *field_index) {
    const Mapper *message_mapper = cxt->mappers.back();

    if (message_mapper->oneof_count)
        cxt->seen_oneof.pop_back();
    if (cxt->track_seen_fields)
        cxt->seen_fields.pop_back();
    cxt->pop_mapper();

    if (message_mapper->decoder_callbacks.transform_fieldtable) {
        cxt->finish_add_transform_fieldtable();
    }

    cxt->items.pop_back();

    return true;
}

bool Mapper::DecoderHandlers::on_end_map_entry(DecoderHandlers *cxt, const int *field_index) {
    THX_DECLARE_AND_GET;

    size_t size = cxt->items.size();
    HV *hash = (HV *) cxt->items[size - 2];
    SV *key = cxt->map_keys.back().key_sv;
    SV *value = (SV *) cxt->items[size - 1];

    if (SvOK(key)) {
        if (!value) {
            value = newSV(0);

            cxt->mappers.back()->apply_map_value_default(value);
        }

        hv_store_ent(hash, key, value, 0);
    } else {
        // having decoding of maps without keys is debatable
        warn("Incomplete map entry: missing key");
    }

    SvOK_off(key);

    cxt->items[size - 1] = NULL;
    if (cxt->track_seen_fields)
        cxt->seen_fields.back()[1] = false;

    return true;
}

bool Mapper::DecoderHandlers::on_end_string_map_entry(DecoderHandlers *cxt, const int *field_index) {
    THX_DECLARE_AND_GET;

    size_t size = cxt->items.size();
    HV *hash = (HV *) cxt->items[size - 2];
    const char *key_buffer = cxt->map_keys.back().key_buffer;
    size_t key_len = cxt->map_keys.back().key_len;
    SV *value = (SV *) cxt->items[size - 1];

    if (key_buffer) {
        if (!value) {
            value = newSV(0);

            cxt->mappers.back()->apply_map_value_default(value);
        }

        // Scanning through the string in order to set the correct flags makes
        // hv_common flaster and is an overall win, as silly as it might seem
        int flags = is_invariant_string((const U8 *) key_buffer, key_len) ? 0 : HVhek_UTF8;
        hv_common(hash, NULL,
                  key_buffer, key_len,
                  flags, HV_FETCH_ISSTORE, value, 0);
    } else {
        // having decoding of maps without keys is debatable
        warn("Incomplete map entry: missing key");
    }

    cxt->map_keys.back().set_buffer(NULL, 0);
    cxt->items[size - 1] = NULL;

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

bool Mapper::DecoderHandlers::on_perl_bool(DecoderHandlers *cxt, const int *field_index, bool val) {
    THX_DECLARE_AND_GET;

    cxt->mark_seen(field_index);
    set_perl_bool(aTHX_ cxt->get_target(field_index), val);

    return true;
}

bool Mapper::DecoderHandlers::on_numeric_bool(DecoderHandlers *cxt, const int *field_index, bool val) {
    THX_DECLARE_AND_GET;

    cxt->mark_seen(field_index);
    set_numeric_bool(aTHX_ cxt->get_target(field_index), val);

    return true;
}

bool Mapper::DecoderHandlers::on_json_bool(DecoderHandlers *cxt, const int *field_index, bool val) {
    THX_DECLARE_AND_GET;
    const Mapper *root_mapper = cxt->mappers[0];

    cxt->mark_seen(field_index);
    root_mapper->set_json_bool(cxt->get_target(field_index), val);

    return true;
}

SV *Mapper::DecoderHandlers::get_target(const int *field_index) {
    const Mapper *mapper = mappers.back();
    const Field &field = mapper->fields[*field_index];

    switch (field.field_target) {
    case TARGET_MAP_KEY:
        return map_keys.back().key_sv;
    case TARGET_MAP_VALUE: {
        // here we could use sv_newmortal(), it would be equally efficient,
        // but it makes the performance profile a bit more noisy
        SV *sv = newSV(0);

        items[items.size() - 1] = sv;

        return sv;
    }
    case TARGET_ARRAY_ITEM: {
        AV *av = (AV *) items.back();

        return *av_store(av, av_top_index(av) + 1, newSV(0));
    }
    case TARGET_HASH_ITEM:
    default: {
        HV *hv = (HV *) items.back();

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
    case TARGET_FIELDTABLE_ITEM: {
        SV *sv = newSV(0);

        fieldtable_entries.push_back(Fieldtable::Entry(field.field_def->number(), sv));

        return sv;
    }
    }
}

SV *Mapper::DecoderHandlers::get_hash_item_target(const int *field_index) {
    const Mapper *mapper = mappers.back();
    const Field &field = mapper->fields[*field_index];

    if (!mapper->decoder_callbacks.transform_fieldtable) {
        HV *hv = (HV *) items.back();

        return HeVAL(hv_fetch_ent(hv, field.name, 1, field.name_hash));
    } else {
        SV *sv = newSV(0);

        fieldtable_entries.push_back(Fieldtable::Entry(field.field_def->number(), sv));

        return sv;
    }
}

void Mapper::DecoderHandlers::add_transform_fieldtable(SV *target, const DecoderTransform *message_transform, const DecoderTransform *field_transform) {
    size_t transform_index = pending_transforms.add_transform(target, message_transform, field_transform);

    fieldtable_entries.push_back(Fieldtable::Entry(transform_index, NULL));
}

void Mapper::DecoderHandlers::finish_add_transform_fieldtable() {
    for (vector<Fieldtable::Entry>::reverse_iterator it = fieldtable_entries.rbegin(), en = fieldtable_entries.rend(); it != en; ++it) {
        if (it->value != NULL)
            continue;

        int size = it - fieldtable_entries.rbegin();
        pending_transforms.finish_add_transform(
            it->field,
            size, &*(it - 1)
        );
        fieldtable_entries.erase(fieldtable_entries.end() - size - 1, fieldtable_entries.end());

        break;
    }
}

Mapper::Mapper(pTHX_ Dynamic *_registry, const MessageDef *_message_def, const gpd::pb::Descriptor *_gpd_descriptor, HV *_stash, const MappingOptions &options) :
        registry(_registry),
        message_def(_message_def),
        gpd_descriptor(_gpd_descriptor),
        decoder_field_data(_gpd_descriptor),
        stash(_stash),
        decoder_callbacks(aTHX_ this),
        string_sink(&output_buffer) {
    SET_THX_MEMBER;

    SvREFCNT_inc(stash);

    registry->ref();
    pb_encoder_handlers = Encoder::NewHandlers(message_def);
    json_encoder_handlers = Printer::NewHandlers(message_def, false /* XXX option */);
    decoder_handlers = Handlers::New(message_def);
    oneof_count = message_def->oneof_count();
    decode_explicit_defaults = options.explicit_defaults;
    encode_defaults =
        (message_def->syntax() == UPB_SYNTAX_PROTO2 &&
         options.encode_defaults) ||
        (message_def->syntax() == UPB_SYNTAX_PROTO3 &&
         options.encode_defaults_proto3);
    check_enum_values = options.check_enum_values;
    decode_blessed = options.decode_blessed;
    boolean_style = options.boolean_style;

    if (options.boolean_style == MappingOptions::JSON) {
        load_module(PERL_LOADMOD_NOIMPORT, newSVpvs("JSON"), NULL);

        json_true = gv_fetchpvs("JSON::true", 0, SVt_PVGV);
        json_false = gv_fetchpvs("JSON::false", 0, SVt_PVGV);
    }

    // on older Perls it is not fully reliable because the check is performed before
    // the SetMAGIC() call, so it is better to disable it entirely
    fail_ref_coercion = HAS_FULL_NOMG ? options.fail_ref_coercion : false;
    warn_context = WarnContext::get(aTHX);

    if (!message_def->mapentry()) {
        if (!decoder_handlers->SetEndMessageHandler(UpbMakeHandler(DecoderHandlers::on_end_message)))
            croak("Unable to set upb end message handler for %s", message_def->full_name());
    }

    std::vector<Field*> fields_by_field_def_index;
    fields.reserve(message_def->field_count());
    fields_by_field_def_index.resize(message_def->field_count());

    bool map_entry = message_def->mapentry();
    bool has_required = false;
    for (MessageDef::const_field_iterator it = message_def->field_begin(), en = message_def->field_end(); it != en; ++it) {
        int index = fields.size();
        fields.push_back(Field());

        Field &field = fields.back();;
        const FieldDef *field_def = *it;
        const OneofDef *oneof_def = field_def->containing_oneof();

        fields_by_field_def_index[field_def->index()] = &fields.back();
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
        field.decoder_transform = NULL;
        field.oneof_index = -1;
        FieldData field_data;

        if (map_entry) {
            field.field_target = field_def->number() == 1 ?
                TARGET_MAP_KEY : TARGET_MAP_VALUE;
        } else if (field_def->label() == UPB_LABEL_REPEATED) {
            field.field_target = TARGET_ARRAY_ITEM;
        } else {
            field.field_target = TARGET_HASH_ITEM;
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
            field_data.action = FieldData::STORE_FLOAT;
            break;
        case UPB_TYPE_DOUBLE:
            GET_SELECTOR(DOUBLE, primitive);
            SET_VALUE_HANDLER(double, on_nv<double>);
            field.default_nv = field_def->default_double();
            field_data.action = FieldData::STORE_DOUBLE;
            break;
        case UPB_TYPE_BOOL:
            GET_SELECTOR(BOOL, primitive);
            switch(MappingOptions::BoolStyle(boolean_style)) {
            case MappingOptions::Perl:
                SET_VALUE_HANDLER(bool, on_perl_bool);
                field_data.action = FieldData::STORE_PERL_BOOL;
                break;
            case MappingOptions::Numeric:
                SET_VALUE_HANDLER(bool, on_numeric_bool);
                field_data.action = FieldData::STORE_NUMERIC_BOOL;
                break;
            case MappingOptions::JSON:
                SET_VALUE_HANDLER(bool, on_json_bool);
                field_data.action = FieldData::STORE_JSON_BOOL;
                break;
            }
            field.default_bool = field_def->default_bool();
            break;
        case UPB_TYPE_STRING:
        case UPB_TYPE_BYTES:
            GET_SELECTOR(STARTSTR, str_start);
            GET_SELECTOR(STRING, str_cont);
            GET_SELECTOR(ENDSTR, str_end);
            SET_HANDLER(StartString, on_start_string);
            SET_HANDLER(String, on_append_string);
            SET_HANDLER(EndString, on_end_string);
            field.default_str = field_def->default_string(&field.default_str_len);
            if (field.is_map_key()) {
                field_data.action = FieldData::STORE_STRING_KEY;
            } else if (field_def->type() == UPB_TYPE_STRING) {
                field_data.action = FieldData::STORE_STRING;
            } else {
                field_data.action = FieldData::STORE_BYTES;
            }
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
            if (field.is_map) {
                if (field_def->message_subdef()->FindFieldByNumber(1)->type() == UPB_TYPE_STRING) {
                    field_data.action = FieldData::STORE_STRING_MAP_MESSAGE;
                } else {
                    field_data.action = FieldData::STORE_MAP_MESSAGE;
                }
            } else {
                field_data.action = FieldData::STORE_MESSAGE;
            }
            break;
        case UPB_TYPE_ENUM: {
            GET_SELECTOR(INT32, primitive);
            if (check_enum_values)
                SET_VALUE_HANDLER(int32_t, on_enum);
            else
                SET_VALUE_HANDLER(int32_t, on_iv<int32_t>);
            field.default_iv = field_def->default_int32();
            field_data.action = check_enum_values ?
                                    FieldData::STORE_ENUM :
                                    FieldData::STORE_INT32;

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
            field_data.action =
                field_def->descriptor_type() == UPB_DESCRIPTOR_TYPE_SINT32 ?
                    FieldData::STORE_ZIGZAG :
                    FieldData::STORE_INT32;
            break;
        case UPB_TYPE_UINT32:
            GET_SELECTOR(UINT32, primitive);
            SET_VALUE_HANDLER(uint32_t, on_uv<uint32_t>);
            field.default_uv = field_def->default_uint32();
            field_data.action = FieldData::STORE_UINT32;
            break;
        case UPB_TYPE_INT64:
            GET_SELECTOR(INT64, primitive);
            if (options.use_bigints)
                SET_VALUE_HANDLER(int64_t, on_bigiv);
            else
                SET_VALUE_HANDLER(int64_t, on_iv<int64_t>);
            field.default_i64 = field_def->default_int64();
            if (options.use_bigints)
                field_data.action =
                    field_def->descriptor_type() == UPB_DESCRIPTOR_TYPE_SINT64 ?
                        FieldData::STORE_BIG_ZIGZAG :
                        FieldData::STORE_BIG_INT64;
            else
                field_data.action =
                    field_def->descriptor_type() == UPB_DESCRIPTOR_TYPE_SINT64 ?
                        FieldData::STORE_ZIGZAG :
                        FieldData::STORE_INT64;
            break;
        case UPB_TYPE_UINT64:
            GET_SELECTOR(UINT64, primitive);
            if (options.use_bigints)
                SET_VALUE_HANDLER(uint64_t, on_biguv);
            else
                SET_VALUE_HANDLER(uint64_t, on_uv<uint64_t>);
            field.default_u64 = field_def->default_uint64();
            if (options.use_bigints)
                field_data.action = FieldData::STORE_BIG_UINT64;
            else
                field_data.action = FieldData::STORE_UINT64;
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
                field_data.repeated_type = FieldData::MAP_FIELD;
            } else {
                SET_HANDLER(StartSequence, on_start_sequence);
                SET_HANDLER(EndSequence, on_end_sequence);
                field_data.repeated_type = FieldData::REPEATED_FIELD;
            }
        } else {
            field_data.repeated_type = FieldData::SCALAR_FIELD;
        }

#undef GET_SELECTOR
#undef SET_VALUE_HANDLER
#undef SET_HANDLER

        field_data.index = index;

        decoder_field_data.add_field(field_def->number(), field_data);

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
            Field *field = fields_by_field_def_index[field_def->index()];

            field->oneof_index = oneof_index;
        }
    }

    decoder_field_data.optimize_lookup();
    check_required_fields = has_required && options.check_required_fields;
}

Mapper::~Mapper() {
    for (vector<Field>::iterator it = fields.begin(), en = fields.end(); it != en; ++it) {
        if (it->mapper)
            it->mapper->unref();
        delete it->decoder_transform;
    }
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

const char *Mapper::package_name() const {
    return HvNAME(stash);
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
            croak("Not a hash reference");

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

    if (decode_blessed)
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
}

static SV *scalar_option(pTHX_ HV *options, const char *name) {
    SV **hv_value = hv_fetch(options, name, strlen(name), 0);
    if (!hv_value)
        return NULL;

    return *hv_value;
}

static HV *hash_option(pTHX_ HV *options, const char *name) {
    SV **hv_value = hv_fetch(options, name, strlen(name), 0);
    if (!hv_value)
        return NULL;
    if (!SvROK(*hv_value) || SvTYPE(SvRV(*hv_value)) != SVt_PVHV)
        croak("Expected hash reference for option key %s", name);

    return (HV *) SvRV(*hv_value);
}

static DecoderTransform *make_transform(pTHX_ SV *scalar, bool fieldtable) {
    if (SvROK(scalar)) {
        SV *maybe_cv = SvRV(scalar);
        if (SvTYPE(maybe_cv) != SVt_PVCV)
            croak("Transformation function is a reference but not a code reference");
        if (fieldtable)
            croak("Fieldtable transformation function must be written in C");

        return new DecoderTransform(maybe_cv);
    }

    if (SvIOK(scalar)) {
        if (fieldtable) {
            return new DecoderTransform(INT2PTR(CDecoderTransformFieldtable, SvIV(scalar)));
        } else {
            return new DecoderTransform(INT2PTR(CDecoderTransform, SvIV(scalar)));
        }
    }

    croak("Transformation function must be either a code reference or an integer representing a C function pointer");
}

void Mapper::set_decoder_options(HV *options) {
    SV *transform = scalar_option(aTHX_ options, "transform");
    SV *fieldtable_sv = scalar_option(aTHX_ options, "fieldtable");
    HV *transform_fields = hash_option(aTHX_ options, "transform_fields");
    bool fieldtable = fieldtable_sv ? SvTRUE(fieldtable_sv) : false;

    if (transform) {
        decoder_callbacks.decoder_transform = make_transform(aTHX_ transform, fieldtable);
    }

    if (fieldtable) {
        if (message_def->mapentry()) {
            croak("Can't use fieldtable for map messages");
        } else if (!transform) {
            croak("Can't use fieldtable without transform");
        }

        decoder_callbacks.transform_fieldtable = true;

        for (vector<Field>::iterator it = fields.begin(), en = fields.end(); it != en; ++it) {
            if (it->field_target == TARGET_HASH_ITEM) {
                it->field_target = TARGET_FIELDTABLE_ITEM;
            }
        }
    }

    if (transform_fields) {
        I32 keylen;
        char *key;
        hv_iterinit(transform_fields);
        while (SV *value = hv_iternextsv(transform_fields, &key, &keylen)) {
            string field_name = string(key, keylen);
            STD_TR1::unordered_map<std::string, Field *>::const_iterator field_it = field_map.find(field_name);
            if (field_it == field_map.end()) {
                croak("Unknown field name %s", field_name.c_str());
            }

            Field *field = field_it->second;
            if (field->is_map ||
                    field->field_def->label() == UPB_LABEL_REPEATED ||
                    field->field_def->type() != UPB_TYPE_MESSAGE) {
                croak("Can't apply transformation to field %s", field_name.c_str());
            }

            field->decoder_transform = make_transform(aTHX_ value, fieldtable);
        }
    }
}

bool Mapper::get_decode_blessed() const {
    return decode_blessed;
}

bool Mapper::get_track_seen_fields() const {
    return check_required_fields || decode_explicit_defaults;
}

void Mapper::set_bool(SV *target, bool value) const {
    switch (MappingOptions::BoolStyle(boolean_style)) {
    case MappingOptions::Perl:
        set_perl_bool(aTHX_ target, value);
        break;
    case MappingOptions::Numeric:
        set_numeric_bool(aTHX_ target, value);
        break;
    case MappingOptions::JSON:
        set_json_bool(target, value);
        break;
    }
}

void Mapper::set_json_bool(SV *target, bool value) const {
    sv_setsv(target, GvSV(value ? json_true : json_false));
}

SV *Mapper::encode(SV *ref) {
    if (pb_decoder_method.get() == NULL)
        croak("It looks like resolve_references() was not called (and please use map() anyway)");
    upb::Environment *env = make_localized_environment(aTHX_ &status);
    upb::pb::Encoder *pb_encoder = upb::pb::Encoder::Create(env, pb_encoder_handlers.get(), string_sink.input());
    status.Clear();
    output_buffer.clear();
    warn_context->clear();
    warn_context->localize_warning_handler(aTHX);
    SV *result = NULL;

#if HAS_FULL_NOMG
    SvGETMAGIC(ref);
#endif

    if (encode_value(pb_encoder->input(), &status, ref))
        result = newSVpvn(output_buffer.data(), output_buffer.size());
    output_buffer.clear();

    return result;
}

SV *Mapper::encode_json(SV *ref) {
    if (json_decoder_method.get() == NULL)
        croak("It looks like resolve_references() was not called (and please use map() anyway)");
    upb::Environment *env = make_localized_environment(aTHX_ &status);
    upb::json::Printer *json_encoder = upb::json::Printer::Create(env, json_encoder_handlers.get(), string_sink.input());
    status.Clear();
    output_buffer.clear();
    warn_context->clear();
    warn_context->localize_warning_handler(aTHX);
    SV *result = NULL;

#if HAS_FULL_NOMG
    SvGETMAGIC(ref);
#endif

    if (encode_value(json_encoder->input(), &status, ref))
        result = newSVpvn(output_buffer.data(), output_buffer.size());
    output_buffer.clear();

    return result;
}

SV *Mapper::decode_upb(const char *buffer, STRLEN bufsize) {
    if (pb_decoder_method.get() == NULL)
        croak("It looks like resolve_references() was not called (and please use map() anyway)");
    upb::Environment *env = make_localized_environment(aTHX_ &status);
    upb::pb::Decoder *pb_decoder = upb::pb::Decoder::Create(env, pb_decoder_method.get(), &decoder_sink);
    status.Clear();
    pb_decoder->Reset();
    decoder_callbacks.prepare(newHV());

    SV *result = NULL;
    if (BufferSource::PutBuffer(buffer, bufsize, pb_decoder->input())) {
        result = decoder_callbacks.get_and_mortalize_target();
    }

    decoder_callbacks.finish();

    return SvREFCNT_inc(result);
}

SV *Mapper::decode_bbpb(const char *buffer, STRLEN bufsize) {
    if (pb_decoder_method.get() == NULL)
        croak("It looks like resolve_references() was not called (and please use map() anyway)");

    status.Clear();
    decoder_callbacks.prepare(newHV());

    SV *result = NULL;
    if (run_bbpb_decoder(this, buffer, bufsize)) {
        result = decoder_callbacks.get_and_mortalize_target();
    }

    decoder_callbacks.finish();

    return SvREFCNT_inc(result);
}

SV *Mapper::decode_json(const char *buffer, STRLEN bufsize) {
    if (json_decoder_method.get() == NULL)
        croak("It looks like resolve_references() was not called (and please use map() anyway)");
    upb::Environment *env = make_localized_environment(aTHX_ &status);
    upb::json::Parser *json_decoder = upb::json::Parser::Create(env, json_decoder_method.get(), NULL, &decoder_sink, true);
    status.Clear();
    decoder_callbacks.prepare(newHV());

    SV *result = NULL;
    if (BufferSource::PutBuffer(buffer, bufsize, json_decoder->input())) {
        result = decoder_callbacks.get_and_mortalize_target();
    }

    decoder_callbacks.finish();

    return SvREFCNT_inc(result);
}

bool Mapper::run_bbpb_decoder(Mapper *root_mapper, const char *buffer, STRLEN bufsize) {
    DecoderHandlers *decoder_callbacks = &root_mapper->decoder_callbacks;
    gpd::pb::Decoder pb_decoder;

    pb_decoder.set_buffer(buffer, bufsize);
    pb_decoder.start_message(&root_mapper->decoder_field_data);

    const Mapper *mapper = root_mapper;
    vector<const Mapper *> mappers;

    for (;;) {
        switch (pb_decoder.next_token()) {
        case gpd::pb::TOKEN_FIELD: {
            const FieldDataEntry *entry = pb_decoder.get_field_entry<FieldDataEntry>();
            int idx = entry->data.index;

            switch (entry->data.action) {
            case FieldData::STORE_DOUBLE:
                DecoderHandlers::on_nv(decoder_callbacks, &idx, pb_decoder.get_double());
                break;
            case FieldData::STORE_FLOAT:
                DecoderHandlers::on_nv(decoder_callbacks, &idx, pb_decoder.get_float());
                break;
            case FieldData::STORE_INT64:
                DecoderHandlers::on_iv(decoder_callbacks, &idx, pb_decoder.get_long());
                break;
            case FieldData::STORE_BIG_INT64:
                DecoderHandlers::on_bigiv(decoder_callbacks, &idx, pb_decoder.get_long());
                break;
            case FieldData::STORE_INT32:
                DecoderHandlers::on_iv(decoder_callbacks, &idx, pb_decoder.get_int());
                break;
            case FieldData::STORE_UINT64:
                DecoderHandlers::on_uv(decoder_callbacks, &idx, pb_decoder.get_unsigned_long());
                break;
            case FieldData::STORE_BIG_UINT64:
                DecoderHandlers::on_biguv(decoder_callbacks, &idx, pb_decoder.get_unsigned_long());
                break;
            case FieldData::STORE_UINT32:
                DecoderHandlers::on_uv(decoder_callbacks, &idx, pb_decoder.get_unsigned_int());
                break;
            case FieldData::STORE_PERL_BOOL:
                DecoderHandlers::on_perl_bool(decoder_callbacks, &idx, pb_decoder.get_unsigned_int());
                break;
            case FieldData::STORE_NUMERIC_BOOL:
                DecoderHandlers::on_numeric_bool(decoder_callbacks, &idx, pb_decoder.get_unsigned_int());
                break;
            case FieldData::STORE_JSON_BOOL:
                DecoderHandlers::on_json_bool(decoder_callbacks, &idx, pb_decoder.get_unsigned_int());
                break;
            case FieldData::STORE_STRING:
                DecoderHandlers::on_string(decoder_callbacks, &idx, pb_decoder.get_string_buffer(), pb_decoder.get_string_length(), true);
                break;
            case FieldData::STORE_BYTES:
                DecoderHandlers::on_string(decoder_callbacks, &idx, pb_decoder.get_string_buffer(), pb_decoder.get_string_length(), false);
                break;
            case FieldData::STORE_STRING_KEY:
                DecoderHandlers::on_string_key(decoder_callbacks, &idx, pb_decoder.get_string_buffer(), pb_decoder.get_string_length());
                break;
            case FieldData::STORE_MESSAGE: {
                const Mapper *field_mapper = mapper->fields[idx].mapper;

                DecoderHandlers::on_start_sub_message(decoder_callbacks, &idx);

                pb_decoder.start_message(&field_mapper->decoder_field_data);
                mappers.push_back(mapper);
                mapper = field_mapper;
            }
                break;
            case FieldData::STORE_MAP_MESSAGE:
            case FieldData::STORE_STRING_MAP_MESSAGE: {
                const Mapper *field_mapper = mapper->fields[idx].mapper;

                pb_decoder.start_message(&field_mapper->decoder_field_data);
                mappers.push_back(mapper);
                mapper = field_mapper;
            }
                break;
            case FieldData::STORE_ENUM:
                DecoderHandlers::on_enum(decoder_callbacks, &idx, pb_decoder.get_long());
                break;
            case FieldData::STORE_ZIGZAG:
                DecoderHandlers::on_iv(decoder_callbacks, &idx, pb_decoder.get_zigzag_long());
                break;
            case FieldData::STORE_BIG_ZIGZAG:
                DecoderHandlers::on_bigiv(decoder_callbacks, &idx, pb_decoder.get_zigzag_long());
                break;
            }
        }
            break;
        case gpd::pb::TOKEN_START_SEQUENCE: {
            const FieldDataEntry *entry = pb_decoder.get_field_entry<FieldDataEntry>();

            if (entry->data.repeated_type == FieldData::REPEATED_FIELD) {
                DecoderHandlers::on_start_sequence(decoder_callbacks, &entry->data.index);
            } else {
                DecoderHandlers::on_start_map(decoder_callbacks, &entry->data.index);
            }
        }
            break;
        case gpd::pb::TOKEN_END_SEQUENCE: {
            const FieldDataEntry *entry = pb_decoder.get_field_entry<FieldDataEntry>();

            if (entry->data.repeated_type == FieldData::REPEATED_FIELD) {
                DecoderHandlers::on_end_sequence(decoder_callbacks, &entry->data.index);
            } else {
                DecoderHandlers::on_end_map(decoder_callbacks, &entry->data.index);
            }
        }
            break;
        case gpd::pb::TOKEN_END_MESSAGE: {
            if (!DecoderHandlers::on_end_message(decoder_callbacks, &root_mapper->status))
                return false;

            if (mappers.size() == 0) {
                return true;
            }

            pb_decoder.end_message();
            mapper = mappers.back();
            mappers.pop_back();

            const FieldDataEntry *entry = pb_decoder.get_field_entry<FieldDataEntry>();
            switch (entry->data.action) {
            case FieldData::STORE_MESSAGE:
                DecoderHandlers::on_end_sub_message(decoder_callbacks, &entry->data.index);
                break;
            case FieldData::STORE_MAP_MESSAGE:
                DecoderHandlers::on_end_map_entry(decoder_callbacks, &entry->data.index);
                break;
            case FieldData::STORE_STRING_MAP_MESSAGE:
                DecoderHandlers::on_end_string_map_entry(decoder_callbacks, &entry->data.index);
                break;
            }
        }
            break;
        case gpd::pb::TOKEN_ERROR: {
            const char *decoder_error = pb_decoder.get_error_message();

            if (decoder_error != NULL) {
                root_mapper->status.SetErrorMessage(decoder_error);
            }
        }
            return false;
        }
    }

    return true;
}

bool Mapper::check(SV *ref) {
    if (pb_decoder_method.get() == NULL)
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

    bool is_coerced_ref(pTHX_ Status *status, const Mapper::Field &fd, SV *sv) {
        // for overloaded values, we have no easy way to check if a specific
        // overload method has been defined, so just pass the values through
        //
        // the call to Gv_AMG is necessary because SVf_AMAGIC is set on all stashes
        // on modify, and only reset by Gv_AMG() if the stash does not have overload
        // magic
        if (!SvROK(sv) || (SvAMAGIC(sv) && Gv_AMG(SvSTASH(SvRV(sv)))))
            return false;

        status->SetFormattedErrorMessage(
            "Reference used when a scalar value is expected for field '%s'",
            fd.full_name().c_str()
        );

        return true;
    }

    inline bool SvPOK_utf8(SV *sv) {
        return ((SvFLAGS(sv) & (SVf_POK|SVf_UTF8)) == (SVf_POK|SVf_UTF8));
    }
#if PERL_VERSION < 32
    // this is a copied-and-modified version of SvPVutf8 and sv_2pvutf8
    inline char *SvPVutf8_nomg_impl(pTHX_ SV *sv, STRLEN *lp) {
        // condition and branch are from SvPVutf8
        if (SvPOK_utf8(sv)) {
            *lp = SvCUR(sv);

            return SvPVX(sv);
        } else {
            // from the body of sv_2pvutf8
            if (((SvREADONLY(sv) || SvFAKE(sv)) && !SvIsCOW(sv))
                || isGV_with_GP(sv) || SvROK(sv))
                sv = sv_mortalcopy(sv);
            sv_utf8_upgrade_nomg(sv);
            return SvPV_nomg(sv, *lp);
        }
    }

    #define SvPVutf8_nomg(sv, len) SvPVutf8_nomg_impl(aTHX_ sv, &len)
#endif
    uint64_t get_uint64_nomg(pTHX_ SV *src) {
        if (SvROK(src) && sv_derived_from(src, "Math::BigInt")) {
            bool negative = false;
            uint64_t value = extract_bits(aTHX_ src, &negative);

            return negative ? ~value + 1 : value;
        } else
            return SvUV(src);
    }

    uint64_t get_uint64(pTHX_ SV *src) {
        SvGETMAGIC(src);

        return get_uint64_nomg(aTHX_ src);
    }

    int64_t get_int64_nomg(pTHX_ SV *src) {
        if (SvROK(src) && sv_derived_from(src, "Math::BigInt")) {
            bool negative = false;
            uint64_t value = extract_bits(aTHX_ src, &negative);

            return negative ? -value : value;
        } else
            return SvIV(src);
    }

    int64_t get_int64(pTHX_ SV *src) {
        SvGETMAGIC(src);

        return get_int64_nomg(aTHX_ src);
    }

    #define SvIV64(sv) get_int64(aTHX_ sv)
    #define SvUV64(sv) get_uint64(aTHX_ sv)
    #define SvIV64_nomg(sv) get_int64_nomg(aTHX_ sv)
    #define SvUV64_nomg(sv) get_uint64_nomg(aTHX_ sv)

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

#if HAS_FULL_NOMG
    #define SvIV_enc SvIV_nomg
    #define SvIV64_enc SvIV64_nomg
    #define SvUV_enc SvUV_nomg
    #define SvUV64_enc SvUV64_nomg
    #define SvNV_enc SvNV_nomg
    #define SvPV_enc SvPV_nomg
    #define SvPVutf8_enc SvPVutf8_nomg
    #define SvTRUE_enc SvTRUE_nomg
#else
    #define SvIV_enc SvIV
    #define SvIV64_enc SvIV64
    #define SvUV_enc SvUV
    #define SvUV64_enc SvUV64
    #define SvNV_enc SvNV
    #define SvPV_enc SvPV
    #define SvPVutf8_enc SvPVutf8
    #define SvTRUE_enc SvTRUE
#endif

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
        NV operator()(pTHX_ SV *src) const { return SvNV_enc(src); }
    };

    class IVGetter {
    public:
        IV operator()(pTHX_ SV *src) const { return SvIV_enc(src); }
    };

    class UVGetter {
    public:
        UV operator()(pTHX_ SV *src) const { return SvUV_enc(src); }
    };

    class I64Getter {
    public:
        int64_t operator()(pTHX_ SV *src) const { return SvIV64_enc(src); }
    };

    class U64Getter {
    public:
        uint64_t operator()(pTHX_ SV *src) const { return SvUV64_enc(src); }
    };

    class StringGetter {
    public:
        void operator()(pTHX_ SV *src, string *dest) const {
            STRLEN len;
            const char *buf = SvPVutf8_enc(src, len);

            dest->assign(buf, len);
        }
    };

    class BytesGetter {
    public:
        void operator()(pTHX_ SV *src, string *dest) const {
            STRLEN len;
            const char *buf = SvPV_enc(src, len);

            dest->assign(buf, len);
        }
    };

    class BoolGetter {
    public:
        bool operator()(pTHX_ SV *src) const { return SvTRUE_enc(src); }
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

    int size = av_top_index(source) + 1;

    if (size == 0) {
        // when emitting a packed repeated field, we must avoid invoking
        // StartSequence if we have no values to encode. Not doing so causes us
        // to emit a packed field with length=0, which is illegal and breaks
        // decoders.
        return true;
    }

    if (!sink->StartSequence(fd.selector.seq_start, &sub))
        return false;

    WarnContext::Item &warn_cxt = warn_context->push_level(WarnContext::Array);
    for (int i = 0; i < size; ++i) {
        warn_cxt.index = i;
        SV **item = av_fetch(source, i, 0);
        if (!item)
            return false;

#if HAS_FULL_NOMG
        SvGETMAGIC(*item);
#endif

        if (fail_ref_coercion && is_coerced_ref(aTHX_ status, fd, *item))
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

#if HAS_FULL_NOMG
        SvGETMAGIC(*item);
#endif

        if (!sub.StartSubMessage(fd.selector.msg_start, &submsg))
            return false;
        if (!encode_value(&submsg, status, *item))
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

bool Mapper::encode_value(Sink *sink, Status *status, SV *ref) const {
#if !HAS_FULL_NOMG
    SvGETMAGIC(ref);
#endif

    if (!SvROK(ref) || SvTYPE(SvRV(ref)) != SVt_PVHV)
        croak("Not a hash reference when encoding a %s value", message_def->full_name());
    HV *hv = (HV *) SvRV(ref);

    if (!sink->StartMessage())
        return false;

    bool tied = SvTIED_mg((SV *) hv, PERL_MAGIC_tied);
    bool ok = true;
    WarnContext::Item &warn_cxt = warn_context->push_level(WarnContext::Message);
    vector<bool> seen_oneof;
    seen_oneof.resize(oneof_count);
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
            if (seen_oneof[it->oneof_index])
                continue;
            seen_oneof[it->oneof_index] = true;
        }

        SV *value = HeVAL(he);
#if HAS_FULL_NOMG
        SvGETMAGIC(value);
#endif

        if (it->is_map)
            ok = ok && encode_from_perl_hash(sink, status, *it, value);
        else if (it->field_def->label() == UPB_LABEL_REPEATED)
            ok = ok && encode_from_perl_array(sink, status, *it, value);
        else if (encode_defaults || !it->has_default)
            ok = ok && encode_field(sink, status, *it, value);
        else
            ok = ok && encode_field_nodefaults(sink, status, *it, value);
    }
    warn_context->pop_level();

    if (!sink->EndMessage(status))
        return false;

    return ok;
}

bool Mapper::encode_field(Sink *sink, Status *status, const Field &fd, SV *ref) const {
    int field_type = fd.field_def->type();

    if (fail_ref_coercion && field_type != UPB_TYPE_MESSAGE && is_coerced_ref(aTHX_ status, fd, ref))
        return false;

    switch (field_type) {
    case UPB_TYPE_FLOAT:
        return sink->PutFloat(fd.selector.primitive, SvNV_enc(ref));
    case UPB_TYPE_DOUBLE:
        return sink->PutDouble(fd.selector.primitive, SvNV_enc(ref));
    case UPB_TYPE_BOOL:
        return sink->PutBool(fd.selector.primitive, SvTRUE_enc(ref));
    case UPB_TYPE_STRING:
    case UPB_TYPE_BYTES: {
        STRLEN len;
        const char *str = fd.field_def->type() == UPB_TYPE_STRING ? SvPVutf8_enc(ref, len) : SvPV_enc(ref, len);
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
        if (!fd.mapper->encode_value(&sub, status, ref))
            return false;
        return sink->EndSubMessage(fd.selector.msg_end);
    }
    case UPB_TYPE_ENUM: {
        IV value = SvIV_enc(ref);
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
        return sink->PutInt32(fd.selector.primitive, SvIV_enc(ref));
    case UPB_TYPE_UINT32:
        return sink->PutUInt32(fd.selector.primitive, SvUV_enc(ref));
    case UPB_TYPE_INT64:
        if (sizeof(IV) >= sizeof(int64_t))
            return sink->PutInt64(fd.selector.primitive, SvIV_enc(ref));
        else
            return sink->PutInt64(fd.selector.primitive, SvIV64_enc(ref));
    case UPB_TYPE_UINT64:
        if (sizeof(UV) >= sizeof(int64_t))
            return sink->PutInt64(fd.selector.primitive, SvUV_enc(ref));
        else
            return sink->PutUInt64(fd.selector.primitive, SvUV64_enc(ref));
    default:
        return false; // just in case
    }
}

bool Mapper::encode_field_nodefaults(Sink *sink, Status *status, const Field &fd, SV *ref) const {
    if (fail_ref_coercion && is_coerced_ref(aTHX_ status, fd, ref))
        return false;

    switch (fd.field_def->type()) {
    case UPB_TYPE_FLOAT: {
        NV value = SvNV_enc(ref);
        if (value == fd.default_nv)
            return true;
        return sink->PutFloat(fd.selector.primitive, value);
    }
    case UPB_TYPE_DOUBLE: {
        NV value = SvNV_enc(ref);
        if (value == fd.default_nv)
            return true;
        return sink->PutDouble(fd.selector.primitive, value);
    }
    case UPB_TYPE_BOOL: {
        bool value = SvTRUE_enc(ref);
        if (value == fd.default_bool)
            return true;
        return sink->PutBool(fd.selector.primitive, value);
    }
    case UPB_TYPE_STRING:
    case UPB_TYPE_BYTES: {
        STRLEN len;
        const char *str = fd.field_def->type() == UPB_TYPE_STRING ? SvPVutf8_enc(ref, len) : SvPV_enc(ref, len);
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
        IV value = SvIV_enc(ref);
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
        IV value = SvIV_enc(ref);
        if (value == fd.default_iv)
            return true;
        return sink->PutInt32(fd.selector.primitive, value);
    }
    case UPB_TYPE_UINT32: {
        UV value = SvUV_enc(ref);
        if (value == fd.default_uv)
            return true;
        return sink->PutUInt32(fd.selector.primitive, value);
    }
    case UPB_TYPE_INT64: {
        int64_t value = sizeof(IV) >= sizeof(int64_t) ? SvIV_enc(ref) : SvIV64_enc(ref);
        if (value == fd.default_i64)
            return true;
        return sink->PutInt64(fd.selector.primitive, value);
    }
    case UPB_TYPE_UINT64: {
        uint64_t value = sizeof(UV) >= sizeof(int64_t) ? SvUV_enc(ref) : SvUV64_enc(ref);
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
        bool bval = keylen > 1 || (keylen == 1 && key[0] != '0');
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
    if (fields[0].is_map_key()) {
        if (!encode_key(sink, status, fields[0], key, keylen))
            return false;
        if (!encode_field(sink, status, fields[1], value))
            return false;
    } else {
        if (!encode_key(sink, status, fields[1], key, keylen))
            return false;
        if (!encode_field(sink, status, fields[0], value))
            return false;
    }
    if (!sink->EndMessage(status))
        return false;
    return true;
}

bool Mapper::encode_from_perl_hash(Sink *sink, Status *status, const Field &fd, SV *ref) const {
#if !HAS_FULL_NOMG
    SvGETMAGIC(ref);
#endif

    if (!SvROK(ref) || SvTYPE(SvRV(ref)) != SVt_PVHV)
        croak("Not a hash reference when encoding field '%s'", fd.full_name().c_str());
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

#if HAS_FULL_NOMG
        SvGETMAGIC(value);
#endif

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
#if !HAS_FULL_NOMG
    SvGETMAGIC(ref);
#endif
    if (!SvROK(ref) || SvTYPE(SvRV(ref)) != SVt_PVAV)
        croak("Not an array reference when encoding field '%s'", fd.full_name().c_str());
    AV *array = (AV *) SvRV(ref);

    switch (fd.field_def->type()) {
    case UPB_TYPE_FLOAT:
        return encode_from_array<NVGetter, FloatEmitter>(sink, status, fd, array);
    case UPB_TYPE_DOUBLE:
        return encode_from_array<NVGetter, DoubleEmitter>(sink, status, fd, array);
    case UPB_TYPE_BOOL:
        return encode_from_array<BoolGetter, BoolEmitter>(sink, status, fd, array);
    case UPB_TYPE_STRING:
        return encode_from_array<SVGetter, StringEmitter>(sink, status, fd, array);
    case UPB_TYPE_BYTES:
        return encode_from_array<SVGetter, StringEmitter>(sink, status, fd, array);
    case UPB_TYPE_MESSAGE:
        return fd.mapper->encode_from_message_array(sink, status, fd, array);
    case UPB_TYPE_ENUM:
        if (check_enum_values)
            return encode_from_array<IVGetter, EnumEmitter>(sink, status, fd, array);
        else
            return encode_from_array<IVGetter, Int32Emitter>(sink, status, fd, array);
    case UPB_TYPE_INT32:
        return encode_from_array<IVGetter, Int32Emitter>(sink, status, fd, array);
    case UPB_TYPE_UINT32:
        return encode_from_array<UVGetter, UInt32Emitter>(sink, status, fd, array);
    case UPB_TYPE_INT64:
        if (sizeof(IV) >= sizeof(int64_t))
            return encode_from_array<IVGetter, Int64Emitter>(sink, status, fd, array);
        else
            return encode_from_array<I64Getter, Int64Emitter>(sink, status, fd, array);
    case UPB_TYPE_UINT64:
        if (sizeof(IV) >= sizeof(int64_t))
            return encode_from_array<UVGetter, UInt64Emitter>(sink, status, fd, array);
        else
            return encode_from_array<U64Getter, UInt64Emitter>(sink, status, fd, array);
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
        croak("Not a hash reference when checking a %s value", message_def->full_name());
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

void Mapper::apply_default(const Field &field, SV *target) const {
    switch (field.field_def->type()) {
    case UPB_TYPE_FLOAT:
        sv_setnv(target, field.field_def->default_float());
        break;
    case UPB_TYPE_DOUBLE:
        sv_setnv(target, field.field_def->default_double());
        break;
    case UPB_TYPE_BOOL:
        set_bool(target, field.field_def->default_bool());
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

void Mapper::apply_map_value_default(SV *target) const {
    if (fields[1].is_map_value()) {
        apply_default(fields[1], target);
    } else {
        apply_default(fields[0], target);
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
        croak("Value of field '%s' is not a hash reference", field->full_name().c_str());

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
            croak("Value of field '%s' is not a hash reference", field->full_name().c_str());

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
        mapper->set_bool(target, field_def->default_bool());
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

        if (other->oneof_index != field->oneof_index || other == field)
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
        mapper->set_bool(target, SvTRUE(value));
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
            croak("Value for message field '%s' is not a hash reference", field->full_name().c_str());
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
            int64_t i64 = SvIV64(value);

            set_bigint(aTHX_ target, (uint64_t) i64, i64 < 0);
        }
    }
        break;
    case UPB_TYPE_UINT64: {
        if (sizeof(IV) >= sizeof(uint64_t))
            sv_setuv(target, SvUV(value));
        else {
            int64_t u64 = SvUV64(value);

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
        croak("Value for field '%s' is not a hash reference", field->full_name().c_str());
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

ServiceMapper::ServiceMapper(pTHX_ Dynamic *_registry, const gpd::ServiceDef *_service_def) :
        registry(_registry),
        service_def(_service_def) {
    SET_THX_MEMBER;

    registry->ref();
}

ServiceMapper::~ServiceMapper() {
    delete service_def;
    // make sure this only goes away after inner destructors have completed
    refcounted_mortalize(aTHX_ registry);
}

SV *ServiceMapper::service_descriptor() const {
    SV *ref = newSV(0);

    sv_setref_iv(ref, "Google::ProtocolBuffers::Dynamic::ServiceDef", (IV) service_def);

    return ref;
}

MethodMapper::MethodMapper(pTHX_ Dynamic *_registry, const string &method, const MessageDef *_input_def, const MessageDef *_output_def, bool client_streaming, bool server_streaming) :
        registry(_registry),
        input_def(_input_def),
        output_def(_output_def) {
    SET_THX_MEMBER;

    registry->ref();

    method_name_key_sv = newSVpvs_share("method");
    serialize_key_sv = newSVpvs_share("serialize");
    deserialize_key_sv = newSVpvs_share("deserialize");
    method_name_sv = newSVpvn_share(method.data(), method.length(), 0);
    serialize_sv = NULL;
    deserialize_sv = NULL;

    const char *grpc_name;
    if (client_streaming) {
        if (server_streaming) {
            grpc_name = "Grpc::Client::BaseStub::_bidiRequest";
        } else {
            grpc_name = "Grpc::Client::BaseStub::_clientStreamRequest";
        }
    } else {
        if (server_streaming) {
            grpc_name = "Grpc::Client::BaseStub::_serverStreamRequest";
        } else {
            grpc_name = "Grpc::Client::BaseStub::_simpleRequest";
        }
    }

    grpc_call_sv = get_cv(grpc_name, 0);
    if (grpc_call_sv == NULL)
        croak("Unable to resolve function '%s'", grpc_name);
}

MethodMapper::~MethodMapper() {
    SvREFCNT_dec(method_name_key_sv);
    SvREFCNT_dec(serialize_key_sv);
    SvREFCNT_dec(deserialize_key_sv);
    SvREFCNT_dec(method_name_sv);
    SvREFCNT_dec(serialize_sv);
    SvREFCNT_dec(deserialize_sv);
    SvREFCNT_dec(grpc_call_sv);
    // make sure this only goes away after inner destructors have completed
    refcounted_mortalize(aTHX_ registry);
}

void MethodMapper::resolve_input_output() {
    const Mapper *request_mapper = registry->find_mapper(input_def);
    const Mapper *response_mapper = registry->find_mapper(output_def);

    string request_encoder_name =
        request_mapper->package_name()
        + string("::_static_encode");
    SV *encode = (SV *) get_cv(request_encoder_name.c_str(), 0);
    if (encode == NULL)
        croak("Unable to find GRPC encoder in package '%s' for message '%s'",
              request_mapper->package_name(), request_mapper->full_name());
    serialize_sv = newRV_inc(encode);

    string response_decoder_name =
        response_mapper->package_name()
        + string("::_static_decode");
    SV *decode = (SV *) get_cv(response_decoder_name.c_str(), 0);
    if (decode == NULL)
        croak("Unable to find GRPC decoder in package '%s' for message '%s'",
              response_mapper->package_name(), response_mapper->full_name());
    deserialize_sv = newRV_inc(decode);
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
            sv_catpvs(cxt, "{");
            sv_catpvn(cxt, it->key, it->keylen);
            sv_catpvs(cxt, "}.");
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

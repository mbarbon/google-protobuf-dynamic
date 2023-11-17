#ifndef _GPD_XS_MAPPER_INCLUDED
#define _GPD_XS_MAPPER_INCLUDED

#include "perl_unpollute.h"

#include "ref.h"

#include <upb/pb/encoder.h>
#include <upb/pb/decoder.h>
#include <upb/json/printer.h>
#include <upb/json/parser.h>

#include "unordered_map.h"

#include "EXTERN.h"
#include "perl.h"
#include "ppport.h"

#include "thx_member.h"
#include "transform.h"
#include "vectorsink.h"
#include "fieldmap.h"
#include "pb/decoder.h"
#include "mapper_context.h"

#include <list>
#include <vector>

namespace gpd {

class Dynamic;
class MappingOptions;
class MapperField;
class WarnContext;

class Mapper : public Refcounted {
public:
    enum FieldTarget {
        TARGET_MAP_KEY          = 1,
        TARGET_MAP_VALUE        = 2,
        TARGET_ARRAY_ITEM       = 3,
        TARGET_HASH_ITEM        = 4,
        TARGET_FIELDTABLE_ITEM  = 5,
    };

    enum ValueAction {
        ACTION_INVALID          = 0,

        // scalar values
        ACTION_PUT_FLOAT        = 1,
        ACTION_PUT_FLOAT_ND     = 2,
        ACTION_PUT_DOUBLE       = 3,
        ACTION_PUT_DOUBLE_ND    = 4,
        ACTION_PUT_BOOL         = 5,
        ACTION_PUT_BOOL_ND      = 6,
        ACTION_PUT_STRING       = 7,
        ACTION_PUT_STRING_ND    = 8,
        ACTION_PUT_BYTES        = 9,
        ACTION_PUT_BYTES_ND     = 10,
        ACTION_PUT_ENUM         = 11,
        ACTION_PUT_ENUM_ND      = 12,
        ACTION_PUT_INT32        = 13,
        ACTION_PUT_INT32_ND     = 14,
        ACTION_PUT_UINT32       = 15,
        ACTION_PUT_UINT32_ND    = 16,
        ACTION_PUT_INT64        = 17,
        ACTION_PUT_INT64_ND     = 18,
        ACTION_PUT_UINT64       = 19,
        ACTION_PUT_UINT64_ND    = 20,

        // non-scalar values
        ACTION_PUT_MESSAGE      = 21,
        ACTION_PUT_REPEATED     = 22,
        ACTION_PUT_MAP          = 23,
        ACTION_PUT_FIELDTABLE   = 24,
    };

    struct Field {
        const upb::FieldDef *field_def;
        struct {
            upb_selector_t seq_start;
            upb_selector_t seq_end;
            union {
                struct {
                    upb_selector_t msg_start;
                    upb_selector_t msg_end;
                };
                struct {
                    upb_selector_t str_start;
                    upb_selector_t str_cont;
                    upb_selector_t str_end;
                };
                upb_selector_t primitive;
            };
        } selector;
        SV *name;
        U32 name_hash;
        bool has_default;
        bool is_map;
        FieldTarget field_target;
        ValueAction field_action, value_action;
        const Mapper *mapper; // for Message/Group fields
        gpd::transform::DecoderTransform *decoder_transform;
        gpd::transform::EncoderTransform *encoder_transform;
        UMS_NS::unordered_set<int32_t> enum_values;
        int field_index, oneof_index;
        union {
            struct {
                size_t default_str_len;
                const char *default_str;
            };
            bool default_bool;
            IV default_iv;
            UV default_uv;
            NV default_nv;
            int64_t default_i64;
            uint64_t default_u64;
        };

        std::string full_name() const;
        upb::FieldDef::Type map_value_type() const;
        const UMS_NS::unordered_set<int32_t> &map_enum_values() const;

        bool is_map_key() const { return field_target == TARGET_MAP_KEY; }
        bool is_map_value() const { return field_target == TARGET_MAP_VALUE; }
    };

    struct MapKey {
        SV *key_sv;
        const char *key_buffer;
        size_t key_len;

        MapKey(SV * _key_sv) : key_sv(_key_sv), key_buffer(NULL) { }

        void set_buffer(const char *_key_buffer, size_t _key_len) {
            key_buffer = _key_buffer;
            key_len = _key_len;
        }
    };

    struct DecoderHandlers {
        DECL_THX_MEMBER;
        SV *target_ref;
        std::vector<SV *> items;
        std::vector<MapKey> map_keys;
        std::vector<const Mapper *> mappers;
        std::vector<std::vector<bool> > seen_fields;
        std::vector<std::vector<int32_t> > seen_oneof;
        gpd::transform::DecoderTransformQueue pending_transforms;
        gpd::transform::DecoderTransform *decoder_transform;
        bool decoder_transform_fieldtable;
        std::string error;
        SV *string;
        bool track_seen_fields;
        std::vector<gpd::transform::DecoderFieldtable::Entry> fieldtable_entries;

        DecoderHandlers(pTHX_ const Mapper *mapper);
        ~DecoderHandlers();

        void prepare(HV *target);
        void finish();
        SV *get_and_mortalize_target();
        static void static_clear(DecoderHandlers *cxt);

        static bool on_end_message(DecoderHandlers *cxt, upb::Status *status);
        static DecoderHandlers *on_start_string(DecoderHandlers *cxt, const int *field_index, size_t size_hint);
        static size_t on_append_string(DecoderHandlers *cxt, const int *field_index, const char *buf, size_t len);
        static bool on_end_string(DecoderHandlers *cxt, const int *field_index);
        static void on_string(DecoderHandlers *cxt, const int *field_index, const char *buf, size_t len, bool is_utf8);
        static DecoderHandlers *on_start_sequence(DecoderHandlers *cxt, const int *field_index);
        static size_t on_string_key(DecoderHandlers *cxt, const int *field_index, const char *buf, size_t len);
        static bool on_end_sequence(DecoderHandlers *cxt, const int *field_index);
        static DecoderHandlers *on_start_map(DecoderHandlers *cxt, const int *field_index);
        static bool on_end_map(DecoderHandlers *cxt, const int *field_index);
        static DecoderHandlers *on_start_sub_message(DecoderHandlers *cxt, const int *field_index);
        static bool on_end_sub_message(DecoderHandlers *cxt, const int *field_index);
        static bool on_end_map_entry(DecoderHandlers *cxt, const int *field_index);
        static bool on_end_string_map_entry(DecoderHandlers *cxt, const int *field_index);

        template<class T>
        static bool on_nv(DecoderHandlers *cxt, const int *field_index, T val);

        template<class T>
        static bool on_iv(DecoderHandlers *cxt, const int *field_index, T val);

        template<class T>
        static bool on_uv(DecoderHandlers *cxt, const int *field_index, T val);

        static bool on_enum(DecoderHandlers *cxt, const int *field_index, int32_t val);
        static bool on_bigiv(DecoderHandlers *cxt, const int *field_index, int64_t val);
        static bool on_biguv(DecoderHandlers *cxt, const int *field_index, uint64_t val);

        static bool on_perl_bool(DecoderHandlers *cxt, const int *field_index, bool val);
        static bool on_numeric_bool(DecoderHandlers *cxt, const int *field_index, bool val);
        static bool on_json_bool(DecoderHandlers *cxt, const int *field_index, bool val);

        void push_mapper(const Mapper *mapper);
        void pop_mapper();

        bool apply_defaults_and_check();
        SV *get_target(const int *field_index);
        SV *get_hash_item_target(const int *field_index);
        void mark_seen(const int *field_index) {
            if (track_seen_fields)
                seen_fields.back()[*field_index] = true;
        }

        void maybe_add_transform(SV *target, const gpd::transform::DecoderTransform *message_transform, const gpd::transform::DecoderTransform *field_transform) {
            if (message_transform || field_transform)
                pending_transforms.add_transform(target, message_transform, field_transform);
        }
        void add_transform_fieldtable(SV *target, const gpd::transform::DecoderTransform *message_transform, const gpd::transform::DecoderTransform *field_transform);
        void finish_add_transform_fieldtable();
        void apply_transforms() {
            pending_transforms.apply_transforms();
        }
    };

    struct EncoderState {
        upb::Status *status;
        upb::Sink *sink;
        MapperContext *mapper_context;

        EncoderState(upb::Status *_status, MapperContext *_mapper_context);

        EncoderState(EncoderState &original, upb::Sink *_sink) :
                status(original.status),
                sink(_sink),
                mapper_context(original.mapper_context) {
        }

        void setup(upb::Sink *sink);
    };

public:
    Mapper(pTHX_ Dynamic *registry, const upb::MessageDef *message_def, const gpd::pb::Descriptor *gpd_descriptor, HV *stash, const MappingOptions &options);
    ~Mapper();

    const char *full_name() const;
    const char *package_name() const;

    void resolve_mappers();
    void create_encoder_decoder();
    void set_decoder_options(HV *options);
    void set_encoder_options(HV *options);

    SV *encode(SV *ref);
    SV *decode_upb(const char *buffer, STRLEN bufsize);
    SV *decode_bbpb(const char *buffer, STRLEN bufsize);
    SV *encode_json(SV *ref);
    SV *decode_json(const char *buffer, STRLEN bufsize);
    bool check(SV *ref);

    const char *last_error_message() const;

    int field_count() const;
    const Field *get_field(int index) const;

    MapperField *find_extension(const std::string &name) const;

    SV *message_descriptor() const;
    SV *make_object(SV *data) const;
    bool get_decode_blessed() const;
    bool get_track_seen_fields() const;

    void set_bool(SV *target, bool value) const;

private:
    static bool run_bbpb_decoder(Mapper *root_mapper, const char *buffer, STRLEN bufsize);

    bool encode_message(EncoderState &state, SV *ref) const {
        if (encoder_transform) {
            if (encoder_transform_fieldtable) {
                return encode_transformed_fieldtable_message(state, ref, encoder_transform);
            } else {
                return encode_transformed_message(state, ref, encoder_transform);
            }
        } else if (unknown_field_transform) {
            return encode_simple_message_iterate_hash(state, ref);
        } else {
            return encode_simple_message_iterate_fields(state, ref);
        }
    }

    bool encode_simple_message_iterate_fields(EncoderState &state, SV *ref) const;
    bool encode_simple_message_iterate_hash(EncoderState &state, SV *ref) const;
    bool encode_transformed_message(EncoderState &state, SV *ref, gpd::transform::EncoderTransform *encoder_transform) const;
    bool encode_transformed_fieldtable_message(EncoderState &state, SV *ref, gpd::transform::EncoderTransform *encoder_transform) const;
    bool encode_field(EncoderState &state, const Field &fd, SV *ref) const;
    bool encode_key(EncoderState &state, const Field &fd, const char *key, I32 keylen) const;
    bool encode_hash_kv(EncoderState &state, const char *key, STRLEN keylen, SV *value) const;
    bool encode_from_perl_array(EncoderState &state, const Field &fd, SV *ref) const;
    bool encode_from_perl_hash(EncoderState &state, const Field &fd, SV *ref) const;
    bool encode_from_message_array(EncoderState &state, const Mapper::Field &fd, AV *source) const;

    template<class G, class S>
    bool encode_from_array(EncoderState &state, const Mapper::Field &fd, AV *source) const;

    bool check(upb::Status *status, SV *ref) const;
    bool check(upb::Status *status, const Field &fd, SV *ref) const;
    bool check_from_perl_array(upb::Status *status, const Field &fd, SV *ref) const;
    bool check_from_message_array(upb::Status *status, const Mapper::Field &fd, AV *source) const;
    bool check_from_enum_array(upb::Status *status, const Mapper::Field &fd, AV *source) const;

    void apply_default(const Field &field, SV *target) const;
    void apply_map_value_default(SV *target) const;

    void set_json_bool(SV *target, bool value) const;

    struct FieldData {
        enum RepeatedType {
            SCALAR_FIELD       = 0,
            REPEATED_FIELD     = 1,
            MAP_FIELD          = 2,
        };

        enum Action {
            STORE_FLOAT                 = 1,
            STORE_DOUBLE                = 2,
            STORE_STRING                = 3,
            STORE_MESSAGE               = 4,
            STORE_INT32                 = 5,
            STORE_INT64                 = 6,
            STORE_UINT32                = 7,
            STORE_UINT64                = 8,
            STORE_ZIGZAG                = 9,
            STORE_PERL_BOOL             = 10,
            STORE_ENUM                  = 11,
            STORE_NUMERIC_BOOL          = 12,
            STORE_MAP_MESSAGE           = 13,
            STORE_BIG_INT64             = 14,
            STORE_BIG_UINT64            = 15,
            STORE_BIG_ZIGZAG            = 16,
            STORE_STRING_MAP_MESSAGE    = 17,
            STORE_STRING_KEY            = 18,
            STORE_BYTES                 = 19,
            STORE_JSON_BOOL             = 20,
        };

        int index;
        RepeatedType repeated_type;
        Action action;
    };
    typedef gpd::pb::DecoderFieldData<FieldData>::Entry FieldDataEntry;

    DECL_THX_MEMBER;
    Dynamic *registry;
    const upb::MessageDef *message_def;
    const gpd::pb::Descriptor *gpd_descriptor;
    int oneof_count; // cached here for performance
    HV *stash;
    upb::reffed_ptr<const upb::Handlers> pb_encoder_handlers, json_encoder_handlers;
    upb::reffed_ptr<upb::Handlers> decoder_handlers;
    upb::reffed_ptr<const upb::pb::DecoderMethod> pb_decoder_method;
    upb::reffed_ptr<const upb::json::ParserMethod> json_decoder_method;
    std::vector<Field> fields;
    std::vector<MapperField *> extension_mapper_fields;
    gpd::FieldMap<Field> field_map;
    upb::Status status;
    EncoderState encoder_state;
    DecoderHandlers decoder_callbacks;
    gpd::pb::DecoderFieldData<FieldData> decoder_field_data;
    gpd::transform::EncoderTransform *encoder_transform;
    bool encoder_transform_fieldtable;
    gpd::transform::UnknownFieldTransform *unknown_field_transform;
    upb::Sink decoder_sink;
    gpd::VectorSink vector_sink;
    bool check_required_fields, decode_explicit_defaults, encode_defaults, check_enum_values, decode_blessed, fail_ref_coercion, ignore_undef_fields;
    int boolean_style;
    int map_key_index, map_value_index;
    GV *json_false, *json_true;
    WarnContext *warn_context;
    MapperContext mapper_context;
};

class MapperField : public Refcounted {
public:
    MapperField(pTHX_ const Mapper *mapper, const Mapper::Field *field);
    ~MapperField();

    const char *name();
    bool is_repeated();
    bool is_extension();
    bool is_map();

    // presence
    bool has_field(HV *self);
    void clear_field(HV *self);

    // optional/oneof/required
    SV *get_scalar(HV *self, SV *target);
    void set_scalar(HV *self, SV *value);

    // repeated
    SV *get_item(HV *self, int index, SV *target);
    void set_item(HV *self, int index, SV *value);
    void add_item(HV *self, SV *value);
    int list_size(HV *self);
    SV *get_list(HV *self);
    void set_list(HV *self, SV *ref);

    // map
    SV *get_item(HV *self, SV *key, SV *target);
    void set_item(HV *self, SV *key, SV *value);
    SV *get_map(HV *self);
    void set_map(HV *self, SV *ref);

    static MapperField *find_extension(pTHX_ CV *cv, SV *extension);
    static MapperField *find_scalar_extension(pTHX_ CV *cv, SV *extension);
    static MapperField *find_repeated_extension(pTHX_ CV *cv, SV *extension);

private:
    DECL_THX_MEMBER;
    SV *get_read_field(HV *self);
    SV *get_write_field(HV *self);
    SV *get_read_array_ref(HV *self);
    AV *get_read_array(HV *self);
    AV *get_write_array(HV *self);
    SV *get_read_hash_ref(HV *self);
    HV *get_read_hash(HV *self);
    HV *get_write_hash(HV *self);
    void copy_default(SV *target);
    void copy_value(SV *target, SV *value);
    void clear_oneof(HV *self);

    const Mapper::Field *field;
    const Mapper *mapper;
};

class MethodMapper : public Refcounted {
public:
    MethodMapper(pTHX_ Dynamic *registry, const std::string &method, const upb::MessageDef *input_def, const upb::MessageDef *output_def, bool client_streaming, bool server_streaming);
    ~MethodMapper();

    SV *method_name_key() const { return method_name_key_sv; }
    SV *serialize_key() const { return serialize_key_sv; }
    SV *deserialize_key() const { return deserialize_key_sv; }

    SV *method_name() const { return method_name_sv; }
    SV *serialize() const { return serialize_sv; }
    SV *deserialize() const { return deserialize_sv; }
    SV *grpc_call() const { return (SV *) grpc_call_sv; }

    void resolve_input_output();

private:
    DECL_THX_MEMBER;
    Dynamic *registry;
    const upb::MessageDef *input_def, *output_def;
    SV *method_name_key_sv, *serialize_key_sv, *deserialize_key_sv;
    SV *method_name_sv, *serialize_sv, *deserialize_sv;
    CV *grpc_call_sv;
};

class WarnContext {
public:
    static void setup(pTHX);
    static WarnContext *get(pTHX);

    void warn_with_context(pTHX_ SV *warning) const;

    void set_context(MapperContext *cxt) { context = cxt; }
    void localize_warning_handler(pTHX);

private:
    WarnContext(pTHX);

    MapperContext *context;
    SV *chained_handler;
    SV *warn_handler;
};

};

#endif

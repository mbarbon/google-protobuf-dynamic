#ifndef _GPD_XS_MAPPER_INCLUDED
#define _GPD_XS_MAPPER_INCLUDED

#undef New

#include "ref.h"

#include <upb/pb/encoder.h>
#include <upb/pb/decoder.h>
#include <upb/json/printer.h>
#include <upb/json/parser.h>
#include <upb/bindings/stdc++/string.h>

#include "unordered_map.h"

#include "EXTERN.h"
#include "perl.h"

#include "thx_member.h"

#include <list>
#include <vector>

namespace gpd {

class Dynamic;
class MappingOptions;
class MapperField;
class WarnContext;

class Mapper : public Refcounted {
public:
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
        bool is_key;
        bool is_value;
        const Mapper *mapper; // for Message/Group fields
        STD_TR1::unordered_set<int32_t> enum_values;
        int oneof_index;
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
        const STD_TR1::unordered_set<int32_t> &map_enum_values() const;
    };

    struct DecoderHandlers {
        DECL_THX_MEMBER;
        std::vector<SV *> items;
        std::vector<const Mapper *> mappers;
        std::vector<std::vector<bool> > seen_fields;
        std::vector<std::vector<int32_t> > seen_oneof;
        std::string error;
        SV *string;

        DecoderHandlers(pTHX_ const Mapper *mapper);

        void prepare(HV *target);
        SV *get_target();
        void clear();

        static bool on_end_message(DecoderHandlers *cxt, upb::Status *status);
        static DecoderHandlers *on_start_string(DecoderHandlers *cxt, const int *field_index, size_t size_hint);
        static size_t on_string(DecoderHandlers *cxt, const int *field_index, const char *buf, size_t len);
        static bool on_end_string(DecoderHandlers *cxt, const int *field_index);
        static DecoderHandlers *on_start_sequence(DecoderHandlers *cxt, const int *field_index);
        static bool on_end_sequence(DecoderHandlers *cxt, const int *field_index);
        static DecoderHandlers *on_start_map(DecoderHandlers *cxt, const int *field_index);
        static bool on_end_map(DecoderHandlers *cxt, const int *field_index);
        static DecoderHandlers *on_start_sub_message(DecoderHandlers *cxt, const int *field_index);
        static bool on_end_sub_message(DecoderHandlers *cxt, const int *field_index);
        static bool on_end_map_entry(DecoderHandlers *cxt, const int *field_index);

        template<class T>
        static bool on_nv(DecoderHandlers *cxt, const int *field_index, T val);

        template<class T>
        static bool on_iv(DecoderHandlers *cxt, const int *field_index, T val);

        template<class T>
        static bool on_uv(DecoderHandlers *cxt, const int *field_index, T val);

        static bool on_enum(DecoderHandlers *cxt, const int *field_index, int32_t val);
        static bool on_bigiv(DecoderHandlers *cxt, const int *field_index, int64_t val);
        static bool on_biguv(DecoderHandlers *cxt, const int *field_index, uint64_t val);

        static bool on_bool(DecoderHandlers *cxt, const int *field_index, bool val);

        bool apply_defaults_and_check();
        SV *get_target(const int *field_index);
        void mark_seen(const int *field_index);
    };

public:
    Mapper(pTHX_ Dynamic *registry, const upb::MessageDef *message_def, HV *stash, const MappingOptions &options);
    ~Mapper();

    const char *full_name() const;

    void resolve_mappers();
    void create_encoder_decoder();

    SV *encode(SV *ref);
    SV *decode(const char *buffer, STRLEN bufsize);
    SV *encode_json(SV *ref);
    SV *decode_json(const char *buffer, STRLEN bufsize);
    bool check(SV *ref);

    const char *last_error_message() const;

    int field_count() const;
    const Field *get_field(int index) const;

    MapperField *find_extension(const std::string &name) const;

    SV *message_descriptor() const;
    SV *make_object(SV *data) const;

private:
    bool encode(upb::Sink *sink, upb::Status *status, SV *ref) const;
    bool encode(upb::Sink *sink, upb::Status *status, const Field &fd, SV *ref) const;
    bool encode_nodefaults(upb::Sink *sink, upb::Status *status, const Field &fd, SV *ref) const;
    bool encode_key(upb::Sink *sink, upb::Status *status, const Field &fd, const char *key, I32 keylen) const;
    bool encode_hash_kv(upb::Sink *sink, upb::Status *status, const char *key, STRLEN keylen, SV *value) const;
    bool encode_from_perl_array(upb::Sink *sink, upb::Status *status, const Field &fd, SV *ref) const;
    bool encode_from_perl_hash(upb::Sink *sink, upb::Status *status, const Field &fd, SV *ref) const;
    bool encode_from_message_array(upb::Sink *sink, upb::Status *status, const Mapper::Field &fd, AV *source) const;

    template<class G, class S>
    bool encode_from_array(upb::Sink *sink, upb::Status *status, const Mapper::Field &fd, AV *source) const;

    template<class G, class S>
    bool encode_from_array(upb::Sink *sink, const Mapper::Field &fd, AV *source) const {
        return encode_from_array<G, S>(sink, NULL, fd, source);
    }

    bool check(upb::Status *status, SV *ref) const;
    bool check(upb::Status *status, const Field &fd, SV *ref) const;
    bool check_from_perl_array(upb::Status *status, const Field &fd, SV *ref) const;
    bool check_from_message_array(upb::Status *status, const Mapper::Field &fd, AV *source) const;
    bool check_from_enum_array(upb::Status *status, const Mapper::Field &fd, AV *source) const;

    DECL_THX_MEMBER;
    Dynamic *registry;
    const upb::MessageDef *message_def;
    HV *stash;
    upb::reffed_ptr<const upb::Handlers> pb_encoder_handlers, json_encoder_handlers;
    upb::reffed_ptr<upb::Handlers> decoder_handlers;
    upb::reffed_ptr<const upb::pb::DecoderMethod> pb_decoder_method;
    upb::reffed_ptr<const upb::json::ParserMethod> json_decoder_method;
    std::vector<Field> fields;
    std::vector<MapperField *> extension_mapper_fields;
    STD_TR1::unordered_map<std::string, Field *> field_map;
    upb::Environment env;
    upb::Status status;
    DecoderHandlers decoder_callbacks;
    upb::Sink encoder_sink, decoder_sink;
    upb::pb::Decoder *pb_decoder;
    upb::json::Parser *json_decoder;
    std::string output_buffer;
    upb::StringSink string_sink;
    upb::pb::Encoder *pb_encoder;
    upb::json::Printer *json_encoder;
    bool check_required_fields, decode_explicit_defaults, encode_defaults, check_enum_values;
    WarnContext *warn_context;
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

class EnumMapper : public Refcounted {
public:
    EnumMapper(pTHX_ Dynamic *registry, const upb::EnumDef *enum_def);
    ~EnumMapper();

    SV *enum_descriptor() const;

private:
    DECL_THX_MEMBER;
    Dynamic *registry;
    const upb::EnumDef *enum_def;
};

class WarnContext {
public:
    enum Kind {
        Array   = 1,
        Hash    = 2,
        Message = 3,
    };

    struct Item {
        Kind kind;
        union {
            int index;
            const Mapper::Field *field;
            struct {
                const char *key;
                STRLEN keylen;
            };
        };

        Item(Kind _kind) : kind(_kind) { }
    };

    static void setup(pTHX);
    static WarnContext *get(pTHX);

    void warn_with_context(pTHX_ SV *warning) const;

    Item &push_level(Kind kind) {
        levels.push_back(Item(kind));

        return levels.back();
    }

    void pop_level() { levels.pop_back(); }
    void clear() { levels.clear(); }
    void localize_warning_handler(pTHX);

private:
    typedef std::list<Item> Levels;

    WarnContext(pTHX);

    Levels levels;
    SV *chained_handler;
    SV *warn_handler;
};

};

#endif

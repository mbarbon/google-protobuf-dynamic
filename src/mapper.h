#ifndef _GPD_XS_MAPPER_INCLUDED
#define _GPD_XS_MAPPER_INCLUDED

#undef New

#include "ref.h"

#include <upb/pb/encoder.h>
#include <upb/pb/decoder.h>
#include <upb/bindings/stdc++/string.h>

#include "unordered_map.h"

#include "EXTERN.h"
#include "perl.h"

#include "thx_member.h"

namespace gpd {

class Dynamic;
class MappingOptions;
class MapperField;

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
        const Mapper *mapper; // for Message/Group fields
        STD_TR1::unordered_set<int32_t> enum_values;
        int oneof_index;

        std::string full_name() const;
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
        static DecoderHandlers *on_start_sub_message(DecoderHandlers *cxt, const int *field_index);
        static bool on_end_sub_message(DecoderHandlers *cxt, const int *field_index);

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

    SV *encode(SV *ref);
    SV *decode(const char *buffer, STRLEN bufsize);

    const char *last_error_message() const;

    int field_count() const;
    const Field *get_field(int index) const;

    MapperField *find_extension(const std::string &name) const;

    SV *message_descriptor() const;

private:
    bool encode(upb::pb::Encoder* encoder, upb::Sink *sink, upb::Status *status, SV *ref) const;
    bool encode(upb::pb::Encoder* encoder, upb::Sink *sink, upb::Status *status, const Field &fd, SV *ref) const;
    bool encode_from_perl_array(upb::pb::Encoder* encoder, upb::Sink *sink, upb::Status *status, const Field &fd, SV *ref) const;
    bool encode_from_message_array(upb::pb::Encoder *encoder, upb::Sink *sink, upb::Status *status, const Mapper::Field &fd, AV *source) const;

    template<class G, class S>
    bool encode_from_array(upb::pb::Encoder *encoder, upb::Sink *sink, upb::Status *status, const Mapper::Field &fd, AV *source) const;

    template<class G, class S>
    bool encode_from_array(upb::pb::Encoder *encoder, upb::Sink *sink, const Mapper::Field &fd, AV *source) const {
        return encode_from_array<G, S>(encoder, sink, NULL, fd, source);
    }

    DECL_THX_MEMBER;
    Dynamic *registry;
    const upb::MessageDef *message_def;
    HV *stash;
    upb::reffed_ptr<const upb::Handlers> encoder_handlers;
    upb::reffed_ptr<upb::Handlers> decoder_handlers;
    upb::reffed_ptr<const upb::pb::DecoderMethod> decoder_method;
    std::vector<Field> fields;
    std::vector<MapperField *> extension_mapper_fields;
    upb::Environment env;
    upb::Status status;
    DecoderHandlers decoder_callbacks;
    upb::Sink encoder_sink, decoder_sink;
    upb::pb::Decoder *decoder;
    std::string output_buffer;
    upb::StringSink string_sink;
    upb::pb::Encoder *encoder;
    bool check_required_fields, decode_explicit_defaults, encode_defaults, check_enum_values;
};

class MapperField : public Refcounted {
public:
    MapperField(pTHX_ const Mapper *mapper, const Mapper::Field *field);
    ~MapperField();

    const char *name();
    bool is_repeated();
    bool is_extension();

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
    void copy_default(SV *target);
    void copy_value(SV *target, SV *value);
    void clear_oneof(HV *self);

    const Mapper::Field *field;
    const Mapper *mapper;
};

};

#endif

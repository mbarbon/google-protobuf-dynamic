#ifndef _GPD_XS_PB_DECODER_INCLUDED
#define _GPD_XS_PB_DECODER_INCLUDED

#include <unordered_map>
#include <vector>
#include <string>

#include "pb.h"

namespace gpd {
namespace pb {

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    #define GPD_PB_BIG_ENDIAN
#endif

enum PBToken {
    TOKEN_ERROR          = 0, // 0 so it is coerced to false
    TOKEN_FIELD          = 1,
    TOKEN_UNKNOWN_FIELD  = 2,
    TOKEN_START_SEQUENCE = 3,
    TOKEN_END_SEQUENCE   = 4,
    TOKEN_END_MESSAGE    = 5,
};

class Descriptor {
public:
    struct Entry {
        FieldNumber field;
        FieldType type;
        bool repeated;
        WireType wire_type;
        const Descriptor *message;
    };

    Descriptor();
    ~Descriptor();

    void add_field(FieldNumber field, FieldType type, bool repeated);
    void add_field(FieldNumber field, bool repeated, const Descriptor *message);

    const Entry *find_field(FieldNumber field) const;

private:
    typedef std::unordered_map<FieldNumber, struct Entry *> EntryMap;

    void add_field(FieldNumber field, FieldType type, bool repeated, const Descriptor *message);

    EntryMap entries;
};

class DescriptorSet {
public:
    DescriptorSet();
    ~DescriptorSet();

    void add_descriptor(const std::string &message_name, Descriptor *descriptor);
    const Descriptor *get_descriptor(const std::string &message_name) const;

private:
    typedef std::unordered_map<std::string, Descriptor *> DescriptorMap;

    DescriptorMap descriptors;
};

class DecoderFieldLookup {
public:
    struct Entry {
        const Descriptor::Entry *field;

        Entry(const Descriptor::Entry *_field) {
            field = _field;
        }
    };

    virtual const Entry *find_field(FieldNumber field) const = 0;

protected:
    static std::vector<FieldNumber> find_packed_fields(std::vector<FieldNumber> &all_fields);
};

template<class T>
class DecoderFieldData : public DecoderFieldLookup {
public:
    struct Entry : public DecoderFieldLookup::Entry {
        T data;

        Entry(const Descriptor::Entry *_field, T _data) :
                DecoderFieldLookup::Entry(_field) {
            data = _data;
        }

        Entry() : DecoderFieldLookup::Entry(NULL), data(T()) { }
    };

private:
    typedef std::vector<Entry> FieldVector;
    typedef std::unordered_map<FieldNumber, Entry> FieldMap;

public:
    DecoderFieldData(const Descriptor *_descriptor) {
        descriptor = _descriptor;
        top_packed_field = 0;
    }

    void add_field(FieldNumber field, T data) {
        const Descriptor::Entry *descriptor_field = descriptor->find_field(field);
        if (!field)
            return;

        sparse_fields.insert(std::make_pair(field, Entry(descriptor_field, data)));
    }

    void optimize_lookup() {
        std::vector<FieldNumber> all_ids;

        for (typename FieldMap::const_iterator it = sparse_fields.begin(), en = sparse_fields.end(); it != en; ++it)
            all_ids.push_back(it->first);

        std::vector<FieldNumber> packed_ids = find_packed_fields(all_ids);

        if (packed_ids.size() > 0) {
            top_packed_field = packed_ids.back();
            packed_fields.resize(top_packed_field + 1);

            for (std::vector<FieldNumber>::const_iterator it = packed_ids.begin(), en = packed_ids.end(); it != en; ++it) {
                packed_fields[*it] = sparse_fields[*it];
                sparse_fields.erase(*it);
            }
        }
    }

    virtual const DecoderFieldLookup::Entry *find_field(FieldNumber field) const {
        if (field <= top_packed_field) {
            const DecoderFieldLookup::Entry *entry = &packed_fields[field];

            return entry->field != NULL ? entry : NULL;
        } else {
            typename FieldMap::const_iterator it = sparse_fields.find(field);

            return it != sparse_fields.end() ? &it->second : NULL;
        }
    }

private:
    const Descriptor *descriptor;
    FieldVector packed_fields;
    FieldMap sparse_fields;
    FieldNumber top_packed_field;
};

class Decoder {
    enum State {
        STATE_FIELD                         = 1,
        STATE_START_REPEATED_FIELD          = 2,
        STATE_START_PACKED_REPEATED_FIELD   = 3,
        STATE_IN_REPEATED_FIELD             = 4,
        STATE_IN_PACKED_REPEATED_FIELD      = 5,
        STATE_END_REPEATED_FIELD            = 6,
        STATE_END_MESSAGE                   = 7,
        STATE_ERROR                         = 8,
    };

    struct Context {
        const DecoderFieldLookup *field_lookup;
        const unsigned char *message_end;
        const DecoderFieldLookup::Entry *field_entry;
        State state;

        Context(const DecoderFieldLookup *_field_lookup, const unsigned char *_message_end, const DecoderFieldLookup::Entry *_field_entry, State _state) {
            field_lookup = _field_lookup;
            message_end = _message_end;
            field_entry = _field_entry;
            state = _state;
        }
    };

public:
    Decoder();
    ~Decoder();

    void set_buffer(const unsigned char *buffer, std::size_t size);
    void set_buffer(const char *buffer, std::size_t size) {
        set_buffer(reinterpret_cast<const unsigned char *>(buffer), size);
    }
    bool at_end() const {
        return current == buffer_end;
    }

    PBToken next_token() {
        return next_token_internal();
    }

    void start_message(const DecoderFieldLookup *field_data);
    void end_message();
    bool at_message_end() const {
        return current == message_end;
    }

    template<class T>
    const T *get_field_entry() {
        return static_cast<const T *>(field_entry);
    }

    unsigned long get_unsigned_long() {
        return integral_number;
    }

    unsigned int get_unsigned_int() {
        return static_cast<unsigned int>(integral_int);
    }

    long get_long() {
        return static_cast<long>(integral_number);
    }

    int get_int() {
        return static_cast<int>(integral_int);
    }

    long get_zigzag_long() {
        return (integral_number >> 1) ^ (-(integral_number & 1));
    }

    float get_float() {
        return float_number;
    }

    double get_double() {
        return double_number;
    }

    const char *get_string_buffer() {
        return reinterpret_cast<const char *>(field_payload);
    }

    std::size_t get_string_length() {
        return integral_number;
    }

    const char *get_error_message() {
        return error_message;
    }

private:
    bool decode_varint() {
        if (current < danger_zone) {
            unsigned char byte = *current++;

            if (byte < 0x80) {
                integral_number = byte;
                return current <= message_end;
            }

            return decode_varint_rest_unsafe(byte);
        } else {
            return decode_varint_safe();
        }
    }

    bool decode_varint_rest_unsafe(unsigned char first_byte);
    bool decode_varint_safe();

    bool decode_fixed64() {
        return current < danger_zone ?
            decode_fixed64_unsafe() :
            decode_fixed64_safe();
    }

    bool decode_fixed64_unsafe() {
#ifdef GPD_PB_BIG_ENDIAN
        for (int i = 7; i >= 0; --i)
            fixedbytes[i] = *current++;
#else
        for (int i = 0; i < 8; ++i)
            fixedbytes[i] = *current++;
#endif

        return current <= message_end;
    }
    bool decode_fixed64_safe();

    bool decode_fixed32() {
        return current < danger_zone ?
            decode_fixed32_unsafe() :
            decode_fixed32_safe();
    }

    bool decode_fixed32_unsafe() {
        integral_number = 0;
#ifdef GPD_PB_BIG_ENDIAN
        for (int i = 3; i >= 0; --i)
            fixedbytes[i] = *current++;
#else
        for (int i = 0; i < 4; ++i)
            fixedbytes[i] = *current++;
#endif

        return current <= message_end;
    }
    bool decode_fixed32_safe();

    PBToken next_token_internal();
    bool decode_payload(WireType wire_type);
    bool parse_packed_field_internal();

    PBToken set_error(const char *message);
    PBToken set_error() { return set_error(NULL); }

    const unsigned char *buffer, *buffer_end, *danger_zone, *current, *message_end, *packed_end;

    std::vector<Context> state;
    State current_state;
    const DecoderFieldLookup::Entry *field_entry;
    const unsigned char *field_payload;
    union {
        unsigned char fixedbytes[8];
        unsigned long integral_number;
        unsigned int integral_int;
        double double_number;
        float float_number;
    };
    const char *error_message;
};

}
}

#endif


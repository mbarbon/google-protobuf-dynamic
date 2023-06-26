#ifndef _GPD_XS_PB_ENCODER_INCLUDED
#define _GPD_XS_PB_ENCODER_INCLUDED

#include "pb.h"

#include <vector>
#include <cstdint>

namespace gpd {
namespace pb {

class EncoderOutputMarker {
    friend class EncoderOutput;

    std::size_t encoded_bytes;
    unsigned long offset;
};

class EncoderOutput {
    enum OutputAction {
        OUTPUT_VARINT = 1,
        OUTPUT_BYTES  = 2,
        OUTPUT_BUFFER = 3,
    };

    struct OutputItem {
        OutputAction action;
        union {
            unsigned long varint;
            struct {
                std::uint32_t bytes_length;
                char bytes[10];
            };
            struct {
                const char *buffer;
                std::uint32_t buffer_length;
            };
        };
    };

    const int PADDED_ITEM_SIZE = 24;

    typedef std::vector<char> Slab;

public:
    EncoderOutput();

    void reset();
    bool write_to(std::vector<char> *output);

    void put_int32(FieldNumber field, std::int32_t value);
    void put_fint32(FieldNumber field, std::int32_t value);
    void put_sint32(FieldNumber field, std::int32_t value);

    void put_int64(FieldNumber field, std::int64_t value);
    void put_fint64(FieldNumber field, std::int64_t value);
    void put_sint64(FieldNumber field, std::int64_t value);

    void put_uint32(FieldNumber field, std::uint32_t value);
    void put_fuint32(FieldNumber field, std::uint32_t value);

    void put_uint64(FieldNumber field, std::uint64_t value);
    void put_fuint64(FieldNumber field, std::uint64_t value);

    void put_bool(FieldNumber field, bool value);
    void put_float(FieldNumber field, float value);
    void put_double(FieldNumber field, double value);
    void put_string_alias(FieldNumber field, const char *buffer, std::uint32_t length);

    void start_submessage(FieldNumber field, EncoderOutputMarker *marker);
    void end_submessage(EncoderOutputMarker *marker);

    void start_sequence(FieldNumber field, EncoderOutputMarker *marker);
    void end_sequence(EncoderOutputMarker *marker);

private:
    static uint32_t zig_zag(int32_t value) {
        return (value << 1) ^ (value >> 31);
    }

    static uint64_t zig_zag(int64_t value) {
        return (value << 1) ^ (value >> 63);
    }

    void put_tag(FieldNumber field, WireType wire_type);
    void put_varint(unsigned long varint);
    void put_fixed32(uint32_t value);
    void put_fixed64(uint64_t value);
    void put_buffer(const char *buffer, std::uint32_t length);
    unsigned long put_placeholder();
    void update_placeholder(unsigned long offset, unsigned long varint);

    void ensure_capacity(std::size_t capacity) {
        while (offset + capacity > buffer.size()) {
            buffer.resize(buffer.size() * 2);
        }
    }

    OutputItem *ensure_item();
    char *ensure_bytes_item(std::size_t capacity);
    void complete_bytes_item();

    std::size_t padding(std::size_t offset) {
        std::size_t align = offset % PADDED_ITEM_SIZE;

        return align == 0 ? 0 : PADDED_ITEM_SIZE - align;
    }

    std::int32_t bytes_item_offset;
    std::size_t offset;
    std::size_t encoded_bytes;

    Slab buffer;
};


}
}

#endif

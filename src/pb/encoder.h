#ifndef _GPD_XS_PB_ENCODER_INCLUDED
#define _GPD_XS_PB_ENCODER_INCLUDED

#include "pb.h"

#include <vector>
#include <cstdint>
#include <cstdio>

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

    static const int PADDED_ITEM_SIZE = 24;
    static const int MAX_VARINT_SIZE = 10;
    static const int MAX_TAG_SIZE = 5;
    static const size_t INVALID_BYTES_ITEM_OFFSET = (size_t) -1;

    typedef std::vector<char> Slab;

    template<class T>
    struct ToSelf {
        T operator ()(T value) {
            return value;
        }
    };

    template<class T>
    struct ToUnsigned {
        template<class V>
        T operator ()(V value) {
            return static_cast<T>(value);
        }
    };

    template<class T>
    struct ToZigZag {
        template<class V>
        T operator ()(V value) {
            return (value << 1) ^ (value >> (sizeof(V) * 8 - 1));
        }
    };

public:
    struct EncodedVarint {
        char bytes[MAX_TAG_SIZE];
        char size;

        EncodedVarint() : size(0) { }

        void clear() { size = 0; }
        bool is_empty() { return size == 0; }
    };

    EncoderOutput();

    void reset();
    bool write_to(std::vector<char> *output);

    template<class N>
    void put_int32(N field, std::int32_t value) {
        put_varint<ToUnsigned<std::uint32_t>>(field, value);
    }
    template<class N>
    void put_fint32(N field, std::int32_t value) {
        put_fixed32<ToUnsigned<std::uint32_t>>(field, value);
    }
    template<class N>
    void put_sint32(N field, std::int32_t value) {
        put_varint<ToZigZag<std::uint32_t>>(field, value);
    }

    template<class N>
    void put_int64(N field, std::int64_t value) {
        put_varint<ToUnsigned<std::uint64_t>>(field, value);
    }
    template<class N>
    void put_fint64(N field, std::int64_t value) {
        put_fixed64<ToUnsigned<std::uint64_t>>(field, value);
    }
    template<class N>
    void put_sint64(N field, std::int64_t value) {
        put_varint<ToZigZag<std::uint64_t>>(field, value);
    }

    template<class N>
    void put_uint32(N field, std::uint32_t value) {
        put_varint<ToSelf<std::uint32_t>>(field, value);
    }
    template<class N>
    void put_fuint32(N field, std::uint32_t value) {
        put_fixed32<ToSelf<std::uint32_t>>(field, value);
    }

    template<class N>
    void put_uint64(N field, std::uint64_t value) {
        put_varint<ToSelf<std::uint64_t>>(field, value);
    }
    template<class N>
    void put_fuint64(N field, std::uint64_t value) {
        put_fixed64<ToSelf<std::uint64_t>>(field, value);
    }

    template<class N>
    void put_bool(N field, bool value) {
        ensure_bytes_item(MAX_TAG_SIZE + 1);
        put_tag_unsafe(field, WIRE_VARINT);
        put_varint_unsafe(value);
    }
    template<class N>
    void put_float(N field, float value) {
        put_tag(field, WIRE_FIXED32);
        put_fixed32(*reinterpret_cast<uint32_t *>(&value));
    }
    template<class N>
    void put_double(N field, double value) {
        put_tag(field, WIRE_FIXED64);
        put_fixed64(*reinterpret_cast<uint64_t *>(&value));
    }

    template<class N>
    void put_string_alias(N field, const char *buffer, std::uint32_t length) {
        ensure_bytes_item(MAX_TAG_SIZE + MAX_VARINT_SIZE + PADDED_ITEM_SIZE);
        put_tag_unsafe(field, WIRE_LEN_DELIMITED);
        put_varint_unsafe(length);
        complete_bytes_item_unsafe();
        put_buffer_unsafe(buffer, length);
    }

    template<class N>
    void start_submessage(N field, EncoderOutputMarker *marker) {
        ensure_bytes_item(MAX_TAG_SIZE);
        put_tag_unsafe(field, WIRE_LEN_DELIMITED);
        marker->offset = put_placeholder();
        marker->encoded_bytes = encoded_bytes;
    }
    void end_submessage(EncoderOutputMarker *marker) {
        update_placeholder(marker->offset, encoded_bytes - marker->encoded_bytes);
    }

    template<class N>
    void start_sequence(N field, EncoderOutputMarker *marker) {
        put_tag(field, WIRE_LEN_DELIMITED);
        marker->offset = put_placeholder();
        marker->encoded_bytes = encoded_bytes;
    }
    void end_sequence(EncoderOutputMarker *marker) {
        update_placeholder(marker->offset, encoded_bytes - marker->encoded_bytes);
    }

    static EncodedVarint encode_tag(FieldNumber field, WireType wire_type);

private:
    template<class C, class N, class V>
    void put_varint(N field, V value) {
        ensure_bytes_item(MAX_TAG_SIZE + MAX_VARINT_SIZE);
        put_tag_unsafe(field, WIRE_VARINT);
        put_varint_unsafe(C()(value));
    }

    template<class C, class N, class V>
    void put_fixed32(N field, V value) {
        put_tag(field, WIRE_FIXED32);
        put_fixed32(C()(value));
    }

    template<class C, class N, class V>
    void put_fixed64(N field, V value) {
        put_tag(field, WIRE_FIXED64);
        put_fixed64(C()(value));
    }

    void put_tag(FieldNumber field, WireType wire_type) {
        ensure_bytes_item(MAX_TAG_SIZE);
        put_tag_unsafe(field, wire_type);
    }
    void put_tag(const EncodedVarint &field, WireType wire_type) {
        ensure_bytes_item(MAX_TAG_SIZE);
        put_tag_unsafe(field, wire_type);
    }
    void put_fixed32(uint32_t value);
    void put_fixed64(uint64_t value);
    void put_buffer_unsafe(const char *buffer, std::uint32_t length);

    unsigned long put_placeholder();
    void update_placeholder(unsigned long offset, unsigned long varint);

    void put_tag_unsafe(FieldNumber field, WireType wire_type) {
        if (field != (FieldNumber) -1)
            put_varint_unsafe((field << 3) | wire_type);
    }
    void put_tag_unsafe(const EncodedVarint &field, WireType wire_type);

    void put_varint_unsafe(unsigned long varint) {
        std::size_t used = encode_varint_to(varint, write_ptr);

        encoded_bytes += used;
        write_ptr += used;
    }

    static std::size_t encode_varint_to(unsigned long varint, char *start);
    static std::size_t varint_size(unsigned long varint);

    void ensure_capacity(std::size_t capacity) {
        if (write_ptr + capacity > write_end) {
            ensure_capacity_slow(capacity);
        }
    }
    void ensure_capacity_slow(std::size_t capacity);

    void ensure_bytes_item(std::size_t capacity) {
        ensure_capacity(capacity + PADDED_ITEM_SIZE);
        if (bytes_item_offset == INVALID_BYTES_ITEM_OFFSET) {
            init_bytes_item(capacity);
        }
    }
    void init_bytes_item(std::size_t capacity);
    void complete_bytes_item() {
        if (bytes_item_offset != INVALID_BYTES_ITEM_OFFSET) {
            complete_bytes_item_unsafe();
        }
    }
    void complete_bytes_item_unsafe();

    std::size_t padding(std::size_t offset) {
        std::size_t align = offset % PADDED_ITEM_SIZE;

        return align == 0 ? 0 : PADDED_ITEM_SIZE - align;
    }

    std::size_t write_offset() {
        return write_ptr - &buffer[0];
    }

    std::size_t bytes_item_offset;
    char *write_ptr, *write_end;
    std::size_t encoded_bytes;

    Slab buffer;
};


}
}

#endif

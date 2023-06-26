#include "encoder.h"

#include <cstddef>
#include <cstring>
#include <cstdio>

using namespace gpd::pb;
using namespace std;

const int MAX_VARINT_SIZE = 10;

namespace {
    size_t put_varint(unsigned long varint, char *start) {
        char *ptr = start;

        do {
            *ptr++ = (varint & 0x7f) | 0x80;
            varint >>= 7;
        } while (varint);

        ptr[-1] &= 0x7f;

        return ptr - start;
    }

    size_t varint_size(unsigned long varint) {
        size_t size = 0;

        do {
            size++;
            varint >>= 7;
        } while (varint);

        return size;
    }
}

gpd::pb::EncoderOutput::EncoderOutput() {
    buffer.resize(100);
}

void gpd::pb::EncoderOutput::reset() {
    bytes_item_offset = -1;
    offset = 0;
    encoded_bytes = 0;
}

bool gpd::pb::EncoderOutput::write_to(vector<char> *output) {
    complete_bytes_item();

    OutputItem *item = reinterpret_cast<OutputItem *>(&buffer[0]);
    OutputItem *end = reinterpret_cast<OutputItem *>(&buffer[offset]);

    output->resize(encoded_bytes);

    char *output_start = &(*output)[0], *output_pointer = output_start;
    while (item != end) {
        switch (item->action) {
        case OUTPUT_VARINT:
            output_pointer += ::put_varint(item->varint, output_pointer);
            item++;
            break;
        case OUTPUT_BYTES:
            memcpy(output_pointer, item->bytes, item->bytes_length);
            output_pointer += item->bytes_length;

            if (item->bytes_length > (PADDED_ITEM_SIZE - offsetof(OutputItem, bytes))) {
                size_t item_offset = reinterpret_cast<char *>(item) - &buffer[0];

                item_offset += PADDED_ITEM_SIZE;
                item_offset += item->bytes_length - (PADDED_ITEM_SIZE - offsetof(OutputItem, bytes));
                item_offset += padding(item_offset);

                item = reinterpret_cast<OutputItem *>(&buffer[0] + item_offset);
            } else {
                item++;
            }
            break;
        case OUTPUT_BUFFER:
            memcpy(output_pointer, item->buffer, item->buffer_length);
            output_pointer += item->buffer_length;
            item++;
            break;
        default:
            return false;
        }
    }

    return encoded_bytes == (output_pointer - output_start);
}

void gpd::pb::EncoderOutput::put_int32(FieldNumber field, int32_t value) {
    put_tag(field, WIRE_VARINT);
    put_varint(static_cast<uint32_t>(value));
}

void gpd::pb::EncoderOutput::put_fint32(FieldNumber field, int32_t value) {
    put_tag(field, WIRE_FIXED32);
    put_fixed32(static_cast<uint32_t>(value));
}

void gpd::pb::EncoderOutput::put_sint32(FieldNumber field, int32_t value) {
    put_tag(field, WIRE_VARINT);
    put_varint(zig_zag(value));
}

void gpd::pb::EncoderOutput::put_int64(FieldNumber field, int64_t value) {
    put_tag(field, WIRE_VARINT);
    put_varint(static_cast<uint64_t>(value));
}

void gpd::pb::EncoderOutput::put_fint64(FieldNumber field, int64_t value) {
    put_tag(field, WIRE_FIXED64);
    put_fixed64(static_cast<uint64_t>(value));
}

void gpd::pb::EncoderOutput::put_sint64(FieldNumber field, int64_t value) {
    put_tag(field, WIRE_VARINT);
    put_varint(zig_zag(value));
}

void gpd::pb::EncoderOutput::put_uint32(FieldNumber field, uint32_t value) {
    put_tag(field, WIRE_VARINT);
    put_varint(value);
}

void gpd::pb::EncoderOutput::put_fuint32(FieldNumber field, uint32_t value) {
    put_tag(field, WIRE_FIXED32);
    put_fixed32(value);
}

void gpd::pb::EncoderOutput::put_uint64(FieldNumber field, uint64_t value) {
    put_tag(field, WIRE_VARINT);
    put_varint(value);
}

void gpd::pb::EncoderOutput::put_fuint64(FieldNumber field, uint64_t value) {
    put_tag(field, WIRE_FIXED64);
    put_fixed64(value);
}

void gpd::pb::EncoderOutput::put_bool(FieldNumber field, bool value) {
    put_tag(field, WIRE_VARINT);
    put_varint(value);
}

void gpd::pb::EncoderOutput::put_float(FieldNumber field, float value) {
    put_tag(field, WIRE_FIXED32);
    put_fixed32(*reinterpret_cast<uint32_t *>(&value));
}

void gpd::pb::EncoderOutput::put_double(FieldNumber field, double value) {
    put_tag(field, WIRE_FIXED64);
    put_fixed64(*reinterpret_cast<uint64_t *>(&value));
}

void gpd::pb::EncoderOutput::put_string_alias(FieldNumber field, const char *buffer, uint32_t length) {
    put_tag(field, WIRE_LEN_DELIMITED);
    put_varint(length);
    put_buffer(buffer, length);
}

void gpd::pb::EncoderOutput::start_submessage(FieldNumber field, EncoderOutputMarker *marker) {
    put_tag(field, WIRE_LEN_DELIMITED);
    marker->offset = put_placeholder();
    marker->encoded_bytes = encoded_bytes;
}

void gpd::pb::EncoderOutput::end_submessage(EncoderOutputMarker *marker) {
    update_placeholder(marker->offset, encoded_bytes - marker->encoded_bytes);
}

void gpd::pb::EncoderOutput::start_sequence(FieldNumber field, EncoderOutputMarker *marker) {
    put_tag(field, WIRE_LEN_DELIMITED);
    marker->offset = put_placeholder();
    marker->encoded_bytes = encoded_bytes;
}

void gpd::pb::EncoderOutput::end_sequence(EncoderOutputMarker *marker) {
    update_placeholder(marker->offset, encoded_bytes - marker->encoded_bytes);
}

void gpd::pb::EncoderOutput::put_tag(FieldNumber field, WireType wire_type) {
    put_varint((field << 3) | wire_type);
}

void gpd::pb::EncoderOutput::put_varint(unsigned long varint) {
    char *start = ensure_bytes_item(MAX_VARINT_SIZE);
    size_t used = ::put_varint(varint, start);

    encoded_bytes += used;
    offset += used;
}

void gpd::pb::EncoderOutput::put_fixed32(uint32_t value) {
    char *start = ensure_bytes_item(4);
    *((uint32_t *) start) = value;

    encoded_bytes += 4;
    offset += 4;
}

void gpd::pb::EncoderOutput::put_fixed64(uint64_t value) {
    char *start = ensure_bytes_item(8);
    *((uint64_t *) start) = value;

    encoded_bytes += 8;
    offset += 8;
}

void gpd::pb::EncoderOutput::put_buffer(const char *buffer, uint32_t length) {
    OutputItem *item = ensure_item();

    item->action = OUTPUT_BUFFER;
    item->buffer = buffer;
    item->buffer_length = length;
    encoded_bytes += length;
}

unsigned long gpd::pb::EncoderOutput::put_placeholder() {
    OutputItem *item = ensure_item();

    item->action = OUTPUT_VARINT;
    item->varint = 0;

    return static_cast<char *>(static_cast<void *>(item)) - &buffer[0];
}

void gpd::pb::EncoderOutput::update_placeholder(unsigned long offset, unsigned long varint) {
    OutputItem *item = static_cast<OutputItem *>(static_cast<void *>(&buffer[0] + offset));

    item->varint = varint;
    encoded_bytes += varint_size(varint);
}

gpd::pb::EncoderOutput::OutputItem *gpd::pb::EncoderOutput::ensure_item() {
    complete_bytes_item();

    ensure_capacity(PADDED_ITEM_SIZE);

    OutputItem *item = reinterpret_cast<OutputItem *>(&buffer[offset]);

    offset += PADDED_ITEM_SIZE;

    return item;
}

char *gpd::pb::EncoderOutput::ensure_bytes_item(std::size_t capacity) {
    if (bytes_item_offset == -1) {
        ensure_capacity(capacity + PADDED_ITEM_SIZE);
        bytes_item_offset = offset / PADDED_ITEM_SIZE;

        OutputItem *bytes = reinterpret_cast<OutputItem *>(&buffer[offset]);
        bytes->action = OUTPUT_BYTES;
        bytes->bytes_length = 0;

        offset += offsetof(OutputItem, bytes);
    } else {
        ensure_capacity(capacity);
    }

    return &buffer[offset];
}

void gpd::pb::EncoderOutput::complete_bytes_item() {
    if (bytes_item_offset != -1) {
        OutputItem *bytes = reinterpret_cast<OutputItem *>(&buffer[0]) + bytes_item_offset;

        bytes->bytes_length = &buffer[offset] - bytes->bytes;
        bytes_item_offset = -1;

        offset += padding(offset);
    }
}

#include "encoder.h"

#include <cstddef>
#include <cstring>
#include <cstdio>

using namespace gpd::pb;
using namespace std;

gpd::pb::EncoderOutput::EncoderOutput() {
    buffer.resize(100);
}

void gpd::pb::EncoderOutput::reset() {
    bytes_item_offset = INVALID_BYTES_ITEM_OFFSET;
    write_ptr = &buffer[0];
    write_end = &buffer[0] + buffer.size();
    encoded_bytes = 0;
}

bool gpd::pb::EncoderOutput::write_to(vector<char> *output) {
    complete_bytes_item();

    OutputItem *item = reinterpret_cast<OutputItem *>(&buffer[0]);
    OutputItem *end = reinterpret_cast<OutputItem *>(&buffer[write_offset()]);

    output->resize(encoded_bytes);

    char *output_start = &(*output)[0], *output_pointer = output_start;
    while (item != end) {
        switch (item->action) {
        case OUTPUT_VARINT:
            output_pointer += encode_varint_to(item->varint, output_pointer);
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

void gpd::pb::EncoderOutput::put_tag_unsafe(const EncodedVarint &field, WireType wire_type) {
    write_ptr[0] = field.bytes[0];
    write_ptr[1] = field.bytes[1];
    write_ptr[2] = field.bytes[2];
    write_ptr[3] = field.bytes[3];
    write_ptr[4] = field.bytes[4];

    encoded_bytes += field.size;
    write_ptr += field.size;
}

gpd::pb::EncoderOutput::EncodedVarint gpd::pb::EncoderOutput::encode_tag(FieldNumber field, WireType wire_type) {
    EncodedVarint value;

    value.size = encode_varint_to((field << 3) | wire_type, value.bytes);

    return value;
}

void gpd::pb::EncoderOutput::put_fixed32(uint32_t value) {
    ensure_bytes_item(4);
    *((uint32_t *) write_ptr) = value;

    encoded_bytes += 4;
    write_ptr += 4;
}

void gpd::pb::EncoderOutput::put_fixed64(uint64_t value) {
    ensure_bytes_item(8);
    *((uint64_t *) write_ptr) = value;

    encoded_bytes += 8;
    write_ptr += 8;
}

void gpd::pb::EncoderOutput::put_buffer_unsafe(const char *buffer, uint32_t length) {
    OutputItem *item = reinterpret_cast<OutputItem *>(write_ptr);

    item->action = OUTPUT_BUFFER;
    item->buffer = buffer;
    item->buffer_length = length;

    encoded_bytes += length;
    write_ptr += PADDED_ITEM_SIZE;
}

unsigned long gpd::pb::EncoderOutput::put_placeholder() {
    complete_bytes_item();
    ensure_capacity(PADDED_ITEM_SIZE);

    OutputItem *item = reinterpret_cast<OutputItem *>(write_ptr);

    item->action = OUTPUT_VARINT;
    item->varint = 0;

    unsigned long offset = write_offset();

    write_ptr += PADDED_ITEM_SIZE;

    return offset;
}

void gpd::pb::EncoderOutput::update_placeholder(unsigned long offset, unsigned long varint) {
    OutputItem *item = reinterpret_cast<OutputItem *>(&buffer[0] + offset);

    item->varint = varint;
    encoded_bytes += varint_size(varint);
}

void gpd::pb::EncoderOutput::init_bytes_item(std::size_t capacity) {
    bytes_item_offset = write_offset();

    OutputItem *bytes = reinterpret_cast<OutputItem *>(write_ptr);
    bytes->action = OUTPUT_BYTES;
    bytes->bytes_length = 0;

    write_ptr += offsetof(OutputItem, bytes);
}

void gpd::pb::EncoderOutput::complete_bytes_item_unsafe() {
    OutputItem *bytes = reinterpret_cast<OutputItem *>(&buffer[bytes_item_offset]);

    bytes->bytes_length = write_ptr - bytes->bytes;
    bytes_item_offset = INVALID_BYTES_ITEM_OFFSET;

    write_ptr += padding(write_offset());
}

void gpd::pb::EncoderOutput::ensure_capacity_slow(size_t capacity) {
    std::size_t offset = write_offset();

    while (offset + capacity > buffer.size()) {
        buffer.resize(buffer.size() * 2);
    }

    write_ptr = &buffer[offset];
    write_end = &buffer[0] + buffer.size();
}

size_t gpd::pb::EncoderOutput::encode_varint_to(unsigned long varint, char *start) {
    char *ptr = start;

    do {
        *ptr++ = (varint & 0x7f) | 0x80;
        varint >>= 7;
    } while (varint);

    ptr[-1] &= 0x7f;

    return ptr - start;
}

size_t gpd::pb::EncoderOutput::varint_size(unsigned long varint) {
    size_t size = 0;

    do {
        size++;
        varint >>= 7;
    } while (varint);

    return size;
}

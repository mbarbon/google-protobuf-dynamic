#include "decoder.h"

#include <cstddef>
#include <csignal>
#include <algorithm>

using namespace std;

gpd::pb::Descriptor::Descriptor() {
}

gpd::pb::Descriptor::~Descriptor() {
    for (EntryMap::iterator it = entries.begin(), en = entries.end(); it != en; ++it)
        delete it->second;
}

void gpd::pb::Descriptor::add_field(FieldNumber field, FieldType type, bool repeated) {
    add_field(field, type, repeated, NULL);
}

void gpd::pb::Descriptor::add_field(FieldNumber field, bool repeated, const Descriptor *message) {
    add_field(field, TYPE_MESSAGE, repeated, message);
}

void gpd::pb::Descriptor::add_field(FieldNumber field, FieldType type, bool repeated, const Descriptor *message) {
    Entry *entry = new Entry();

    entry->field = field;
    entry->type = type;
    entry->repeated = repeated;
    entry->message = message;
    entry->wire_type = wire_type(entry->type);

    entries[field] = entry;
}

const gpd::pb::Descriptor::Entry *gpd::pb::Descriptor::find_field(FieldNumber field) const {
    EntryMap::const_iterator it = entries.find(field);

    return it == entries.end() ? NULL : it->second;
}

gpd::pb::DescriptorSet::DescriptorSet() {
}

gpd::pb::DescriptorSet::~DescriptorSet() {
    for (DescriptorMap::iterator it = descriptors.begin(), en = descriptors.end(); it != en; ++it)
        delete it->second;
}

void gpd::pb::DescriptorSet::add_descriptor(const string &message_name, Descriptor *descriptor) {
    descriptors.insert(make_pair(message_name, descriptor));
}

const gpd::pb::Descriptor *gpd::pb::DescriptorSet::get_descriptor(const string &message_name) const {
    DescriptorMap::const_iterator it = descriptors.find(message_name);

    return it == descriptors.end() ? NULL : it->second;
}

vector<gpd::pb::FieldNumber> gpd::pb::DecoderFieldLookup::find_packed_fields(vector<gpd::pb::FieldNumber> &all_fields) {
    sort(all_fields.begin(), all_fields.end());

    int last_included = -1;
    for (int i = 0, max = all_fields.size(); i < max; ++i) {
        if (all_fields[i] < 70) {
            last_included = i;
            continue;
        }
        // the load factor also counts the unused "0" entry
        int load_factor = (i + 1) * 100 / (all_fields[i] + 1);
        if (load_factor > 75) {
            last_included = i;
        }
    }

    all_fields.resize(last_included + 1);

    return all_fields;
}

gpd::pb::Decoder::Decoder() {
}

gpd::pb::Decoder::~Decoder() {
}

void gpd::pb::Decoder::set_buffer(const unsigned char *b, size_t s) {
    buffer = b;
    buffer_end = buffer + s;
    danger_zone = buffer_end - 10;
    field_payload = buffer;
    current = buffer_end;
    current_state = STATE_FIELD;
    field_entry = NULL;
    error_message = NULL;
}

gpd::pb::PBToken gpd::pb::Decoder::next_token_internal() {
    switch (current_state) {
    case STATE_FIELD: {
        if (at_message_end()) {
            current_state = STATE_END_MESSAGE;
            return TOKEN_END_MESSAGE;
        }

        if (!decode_varint()) {
            return set_error("Invalid/truncated field tag");
        }

        unsigned long field_number = integral_number >> 3;
        WireType wire_type = WireType(integral_number & 0x07);

        field_entry = state.back().field_lookup->find_field(field_number);

        if (!decode_payload(wire_type)) {
            return set_error();
        }

        if (!field_entry) {
            return TOKEN_UNKNOWN_FIELD;
        }

        if (field_entry->field->repeated) {
            if (wire_type == WIRE_LEN_DELIMITED &&
                    field_entry->field->wire_type != WIRE_LEN_DELIMITED) {
                packed_end = current;
                current = field_payload;

                if (!parse_packed_field_internal()) {
                    if (packed_end == field_payload) {
                        return set_error("Packed field with size 0");
                    }

                    return set_error();
                }

                current_state = STATE_START_PACKED_REPEATED_FIELD;
            } else if (field_entry->field->wire_type == wire_type) {
                current_state = STATE_START_REPEATED_FIELD;
            } else {
                return TOKEN_UNKNOWN_FIELD;
            }

            return TOKEN_START_SEQUENCE;
        }

        if (field_entry->field->wire_type == wire_type) {
            return TOKEN_FIELD;
        } else {
            return TOKEN_UNKNOWN_FIELD;
        }
    }
    case STATE_START_REPEATED_FIELD:
        current_state = STATE_IN_REPEATED_FIELD;
        return TOKEN_FIELD;
    case STATE_START_PACKED_REPEATED_FIELD:
        if (current == packed_end) {
            current_state = STATE_END_REPEATED_FIELD;
        } else {
            current_state = STATE_IN_PACKED_REPEATED_FIELD;
        }
        return TOKEN_FIELD;
    case STATE_IN_REPEATED_FIELD: {
        if (at_message_end()) {
            current_state = STATE_END_MESSAGE;
            return TOKEN_END_SEQUENCE;
        }

        const unsigned char *old_current = current;

        if (!decode_varint()) {
            return set_error("Invalid/truncated field tag");
        }

        unsigned long field_number = integral_number >> 3;
        WireType wire_type = WireType(integral_number & 0x07);

        if (field_number == field_entry->field->field) {
            if (!decode_payload(wire_type)) {
                return set_error();
            }
            return TOKEN_FIELD;
        } else {
            current = old_current;
            current_state = STATE_FIELD;
            return TOKEN_END_SEQUENCE;
        }
    }
        break;
    case STATE_IN_PACKED_REPEATED_FIELD:
        if (!parse_packed_field_internal()) {
            return set_error();
        }

        if (current == packed_end) {
            current_state = STATE_END_REPEATED_FIELD;
        }

        return TOKEN_FIELD;
    case STATE_ERROR:
        return TOKEN_ERROR;
    case STATE_END_MESSAGE:
        return TOKEN_END_MESSAGE;
    case STATE_END_REPEATED_FIELD:
        if (at_message_end()) {
            current_state = STATE_END_MESSAGE;
        } else {
            current_state = STATE_FIELD;
        }
        return TOKEN_END_SEQUENCE;
    default:
        return set_error();
    }
}

bool gpd::pb::Decoder::decode_payload(WireType wire_type) {
    switch (wire_type) {
    case WIRE_VARINT:
        if (!decode_varint()) {
            return set_error("Invalid/truncated varint field value");
        }
        return true;
    case WIRE_LEN_DELIMITED:
        if (!decode_varint()) {
            return set_error("Invalid/truncated field length");
        }
        field_payload = current;
        current += integral_number;

        if (current > message_end) {
            return set_error("Truncated length-delimited field");
        }

        return true;
    case WIRE_FIXED64:
        if (!decode_fixed64()) {
            return set_error("Truncated 64-bit fixed size field");
        }
        return true;
    case WIRE_FIXED32:
        if (!decode_fixed32()) {
            return set_error("Truncated 32-bit fixed size field");
        }
        return true;
    default:
        return set_error("Unrecognized protocol buffer wire type");
    }
}

bool gpd::pb::Decoder::parse_packed_field_internal() {
    switch (field_entry->field->wire_type) {
    case WIRE_VARINT:
        if (!decode_varint() || current > packed_end) {
            return set_error("Invalid/truncated varint packed field value");
        }
        break;
    case WIRE_FIXED64:
        if (!decode_fixed64() || current > packed_end) {
            return set_error("Invalid/truncated 64-bit fixed size packed field value");
        }
        break;
    case WIRE_FIXED32:
        if (!decode_fixed32() || current > packed_end) {
            return set_error("Invalid/truncated 32-bit fixed size packed field value");
        }
        break;
    }

    return true;
}

void gpd::pb::Decoder::start_message(const DecoderFieldLookup *field_lookup) {
    state.push_back(Context(field_lookup, current, field_entry, current_state));
    message_end = current;
    current = field_payload;
    field_entry = NULL;
    current_state = STATE_FIELD;
}

void gpd::pb::Decoder::end_message() {
    Context message_context = state.back();

    state.pop_back();
    message_end = state.back().message_end;
    current = message_context.message_end;
    field_entry = message_context.field_entry;
    current_state = message_context.state;
}

bool gpd::pb::Decoder::decode_varint_rest_unsafe(unsigned char first_byte) {
    unsigned long decoded = first_byte & 0x7fL;
    unsigned char byte;

#define PARSE_VARINT_BYTE(shift) \
    byte = *current++; decoded |= (byte & 0x7fL) << shift; if (byte < 0x80) goto done;

    PARSE_VARINT_BYTE(7);
    PARSE_VARINT_BYTE(14);
    PARSE_VARINT_BYTE(21);
    PARSE_VARINT_BYTE(28);
    PARSE_VARINT_BYTE(35);
    PARSE_VARINT_BYTE(42);
    PARSE_VARINT_BYTE(49);
    PARSE_VARINT_BYTE(56);
    PARSE_VARINT_BYTE(63);

#undef PARSE_VARINT_BYTE

done:
    integral_number = decoded;
    return byte < 0x80 && current <= message_end;
}

bool gpd::pb::Decoder::decode_varint_safe() {
    if (current >= buffer_end)
        return false;

    unsigned long decoded = 0;
    unsigned char byte;

#define PARSE_VARINT_BYTE(shift) \
    byte = *current++; decoded |= (byte & 0x7fL) << shift; if (byte < 0x80 || current == buffer_end) goto done;

    PARSE_VARINT_BYTE(0);
    PARSE_VARINT_BYTE(7);
    PARSE_VARINT_BYTE(14);
    PARSE_VARINT_BYTE(21);
    PARSE_VARINT_BYTE(28);
    PARSE_VARINT_BYTE(35);
    PARSE_VARINT_BYTE(42);
    PARSE_VARINT_BYTE(49);
    PARSE_VARINT_BYTE(56);
    PARSE_VARINT_BYTE(63);

#undef PARSE_VARINT_BYTE

done:
    integral_number = decoded;
    return byte < 0x80 && current <= message_end;
}

bool gpd::pb::Decoder::decode_fixed64_safe() {
    if (current > buffer_end - 8)
        return false;

    return decode_fixed64_unsafe();
}

bool gpd::pb::Decoder::decode_fixed32_safe() {
    if (current > buffer_end - 4)
        return false;

    return decode_fixed32_unsafe();
}

gpd::pb::PBToken gpd::pb::Decoder::set_error(const char *_error_message) {
    if (_error_message) {
        error_message = _error_message;
    }

    current_state = STATE_ERROR;
    return TOKEN_ERROR;
}

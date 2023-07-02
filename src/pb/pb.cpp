#include "pb.h"

gpd::pb::WireType gpd::pb::wire_type(gpd::pb::FieldType field_type) {
    switch (field_type) {
    case TYPE_DOUBLE:
    case TYPE_FIXED64:
    case TYPE_SFIXED64:
        return WIRE_FIXED64;
    case TYPE_FLOAT:
    case TYPE_FIXED32:
    case TYPE_SFIXED32:
        return WIRE_FIXED32;
    case TYPE_STRING:
    case TYPE_MESSAGE:
    case TYPE_BYTES:
        return WIRE_LEN_DELIMITED;
    default:
        return WIRE_VARINT;
    }
}

#ifndef _GPD_XS_PB_PB_INCLUDED
#define _GPD_XS_PB_PB_INCLUDED

namespace gpd {
namespace pb {

typedef unsigned long FieldNumber;

enum WireType {
    WIRE_VARINT        = 0,
    WIRE_FIXED64       = 1,
    WIRE_LEN_DELIMITED = 2,
    // no groups
    WIRE_FIXED32       = 5,
};

enum FieldType {
    TYPE_DOUBLE        = 1,
    TYPE_FLOAT         = 2,
    TYPE_INT64         = 3,
    TYPE_UINT64        = 4,
    TYPE_INT32         = 5,
    TYPE_FIXED64       = 6,
    TYPE_FIXED32       = 7,
    TYPE_BOOL          = 8,
    TYPE_STRING        = 9,
    // no groups
    TYPE_MESSAGE       = 11,
    TYPE_BYTES         = 12,
    TYPE_UINT32        = 13,
    TYPE_ENUM          = 14,
    TYPE_SFIXED32      = 15,
    TYPE_SFIXED64      = 16,
    TYPE_SINT32        = 17,
    TYPE_SINT64        = 18,
};

}
}

#endif

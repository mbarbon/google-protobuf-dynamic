#ifndef _GPD_XS_MAPPER_INCLUDED
#define _GPD_XS_MAPPER_INCLUDED

#undef New

#include <google/protobuf/message.h>
#include <tr1/unordered_map>

#include "EXTERN.h"
#include "perl.h"

namespace gpd {

    static inline void free_message(void *ptr) { delete (google::protobuf::Message *) ptr; }

class Mapper {
public:
    void decode_to_perl(const google::protobuf::Message *message, SV *target) const;
    void decode_to_perl(const google::protobuf::Message *message, const google::protobuf::FieldDescriptor *fd, SV *target) const;
    void decode_to_perl_array(const google::protobuf::Message *message, const google::protobuf::FieldDescriptor *fd, SV *target) const;

    void encode_from_perl(google::protobuf::Message *message, SV *ref) const;
    void encode_from_perl(google::protobuf::Message *message, const google::protobuf::FieldDescriptor *fd, SV *ref) const;
    void encode_from_perl_array(google::protobuf::Message *message, const google::protobuf::FieldDescriptor *fd, SV *ref) const;

    const Mapper *mapper_for_descriptor(const google::protobuf::Descriptor *descriptor) const;

public:
    typedef std::tr1::unordered_map<const google::protobuf::Descriptor *, const Mapper *> InnerMessagesMap;

    google::protobuf::MessageFactory *factory;
    const google::protobuf::Message *prototype;
    const google::protobuf::Reflection *reflection;
    InnerMessagesMap descriptor_map;
};

};

#endif

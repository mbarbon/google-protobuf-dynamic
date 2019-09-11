#ifndef _GPD_XS_DEFBUILDER_INCLUDED
#define _GPD_XS_DEFBUILDER_INCLUDED

#include <google/protobuf/descriptor.h>
#include <upb/def.h>

namespace gpd {

class DefBuilder {
public:
    const upb::MessageDef *GetMessageDef(const google::protobuf::Descriptor *descriptor);
    const upb::EnumDef *GetEnumDef(const google::protobuf::EnumDescriptor *descriptor);
};

};

#endif

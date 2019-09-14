#ifndef _GPD_XS_DEFBUILDER_INCLUDED
#define _GPD_XS_DEFBUILDER_INCLUDED

#include <google/protobuf/descriptor.h>
#include <upb/def.h>

#include "unordered_map.h"

namespace gpd {

class DefBuilder {
public:
    DefBuilder();
    ~DefBuilder();

    const upb::MessageDef *GetMessageDef(const google::protobuf::Descriptor *descriptor);
    const upb::EnumDef *GetEnumDef(const google::protobuf::EnumDescriptor *descriptor);

private:
    STD_TR1::unordered_set<std::string> mapped_files;
    upb::SymbolTable *symbol_table;
};

};

#endif

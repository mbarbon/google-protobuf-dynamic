#ifndef _GPD_XS_DEFBUILDER_INCLUDED
#define _GPD_XS_DEFBUILDER_INCLUDED

#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/descriptor.h>
#include <upb/def.hpp>

#include "unordered_map.h"

namespace gpd {

class DefBuilder {
public:
    DefBuilder();
    ~DefBuilder();

    const upb::MessageDefPtr GetMessageDef(const google::protobuf::Descriptor *descriptor);
    const upb::EnumDefPtr GetEnumDef(const google::protobuf::EnumDescriptor *descriptor);

private:
    void add_files(const google::protobuf::FileDescriptor *fd);

    static void add_unmapped_files(google::protobuf::FileDescriptorSet *fds, const google::protobuf::FileDescriptor *fd, STD_TR1::unordered_set<std::string> &mapped_files_new);

    STD_TR1::unordered_set<std::string> mapped_files;
    upb::SymbolTable symbol_table;
};

};

#endif

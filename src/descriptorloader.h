#ifndef _GPD_XS_DESCRIPTORLOADER_INCLUDED
#define _GPD_XS_DESCRIPTORLOADER_INCLUDED

#undef New
#undef Move
#undef do_open
#undef do_close

#include <google/protobuf/compiler/importer.h>

namespace gpd {

// a reimplementation of Importer, with a different DescriptorPool
class DescriptorLoader {
    class ErrorCollector : public google::protobuf::DescriptorPool::ErrorCollector {
    public:
        virtual void AddError(const std::string &filename, const std::string &element_name, const google::protobuf::Message *descriptor, google::protobuf::DescriptorPool::ErrorCollector::ErrorLocation location, const std::string &message);
        virtual void AddWarning(const std::string &filename, const std::string &element_name, const google::protobuf::Message *descriptor, google::protobuf::DescriptorPool::ErrorCollector::ErrorLocation location, const std::string &message);

        std::string errors;
    };

public:
    DescriptorLoader(google::protobuf::compiler::SourceTree *source_tree,
                     google::protobuf::compiler::MultiFileErrorCollector *error_collector);
    ~DescriptorLoader();

    const google::protobuf::FileDescriptor *load_proto(const std::string &filename);
    const std::vector<const google::protobuf::FileDescriptor *> load_serialized(const char *buffer, size_t length);

    inline const google::protobuf::DescriptorPool *pool() const {
        return &merged_pool;
    }

private:
    google::protobuf::compiler::SourceTreeDescriptorDatabase source_database;
    google::protobuf::DescriptorPoolDatabase binary_database;
    google::protobuf::MergedDescriptorDatabase merged_database;
    google::protobuf::DescriptorPool binary_pool, merged_pool;
};

}
#endif

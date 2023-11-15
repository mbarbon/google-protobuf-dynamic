#ifndef _GPD_XS_DESCRIPTORLOADER_INCLUDED
#define _GPD_XS_DESCRIPTORLOADER_INCLUDED

#include "perl_unpollute.h"

#include "sourcetree.h"

#include <google/protobuf/compiler/importer.h>

namespace gpd {

// alternative SourceTreeDescriptorDatabase for protobuf 3.6 or older
    class SourceTreeDescriptorDatabaseWithFallback : public google::protobuf::DescriptorDatabase {
public:
    SourceTreeDescriptorDatabaseWithFallback(google::protobuf::compiler::SourceTree *source_tree,
                                             google::protobuf::DescriptorDatabase *fallback_database);

    // forwarded SourceTreeDescriptorDatabase methods
    void RecordErrorsTo(google::protobuf::compiler::MultiFileErrorCollector* error_collector) {
        database.RecordErrorsTo(error_collector);
    }

    google::protobuf::DescriptorPool::ErrorCollector* GetValidationErrorCollector() {
        return database.GetValidationErrorCollector();
    }

    // DescriptorDatabase implementation
    bool FindFileByName(const std::string& filename,
                        google::protobuf::FileDescriptorProto* output);

    bool FindFileContainingSymbol(const std::string& symbol_name,
                                  google::protobuf::FileDescriptorProto* output) {
        return false;
    }

    bool FindFileContainingExtension(const std::string& containing_type,
                                     int field_number,
                                     google::protobuf::FileDescriptorProto* output) {
        return false;
    }

private:
    google::protobuf::compiler::SourceTree *source_tree;
    google::protobuf::compiler::SourceTreeDescriptorDatabase database;
    google::protobuf::DescriptorDatabase *fallback_database;
};

// a reimplementation of Importer, with a different DescriptorPool
class DescriptorLoader {
    class CollectMultiFileErrors : public google::protobuf::compiler::MultiFileErrorCollector {
    public:
        virtual void AddError(const std::string &filename, int line, int column, const std::string &message);
        virtual void AddWarning(const std::string &filename, int line, int column, const std::string &message);

        void maybe_croak();

    private:
        std::string errors;
    };

public:
    DescriptorLoader();
    ~DescriptorLoader();

    const google::protobuf::FileDescriptor *load_proto(const std::string &filename);
    const std::vector<const google::protobuf::FileDescriptor *> load_serialized(const char *buffer, size_t length);

    inline const google::protobuf::DescriptorPool *pool() const {
        return &merged_pool;
    }

    void map_disk_path(const std::string &virtual_path, const std::string &disk_path) {
        disk_source_tree.MapPath(virtual_path, disk_path);
    }

    void add_memory_file(const std::string &file_name, const char *data, size_t length) {
        memory_source_tree.AddFile(file_name, data, length);
    }

    void maybe_croak() {
        multifile_error_collector.maybe_croak();
    }

private:
    CollectMultiFileErrors multifile_error_collector;
    OverlaySourceTree overlay_source_tree;
    MemorySourceTree memory_source_tree;
    google::protobuf::compiler::DiskSourceTree disk_source_tree;
#if GOOGLE_PROTOBUF_VERSION >= 3007000
    google::protobuf::compiler::SourceTreeDescriptorDatabase source_database;
#else
    SourceTreeDescriptorDatabaseWithFallback source_database;
#endif
    google::protobuf::SimpleDescriptorDatabase binary_database;
    google::protobuf::DescriptorPoolDatabase generated_database;
    google::protobuf::MergedDescriptorDatabase merged_source_binary_database;
    google::protobuf::DescriptorPool merged_pool;
};

}
#endif

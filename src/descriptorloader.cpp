#include "descriptorloader.h"

#include <google/protobuf/duration.pb.h>
#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/wrappers.pb.h>

#include "EXTERN.h"
#include "perl.h"

using namespace google::protobuf::compiler;
using namespace google::protobuf;
using namespace gpd;
using namespace std;

namespace {
    const string wkt_prefix = "google/protobuf/";
    const int wkt_prefix_length = wkt_prefix.length();

    // it seems the only way to add a Descriptor to a pool is to go through the Proto object
    void add_descriptor_to_pool(DescriptorPool *pool, const Descriptor *descriptor) {
        const FileDescriptor *file_descriptor = descriptor->file();
        FileDescriptorProto file_proto;

        file_descriptor->CopyTo(&file_proto);
        pool->BuildFile(file_proto);
    }
}

void DescriptorLoader::ErrorCollector::AddError(const string &filename, const string &element_name, const Message *descriptor, DescriptorPool::ErrorCollector::ErrorLocation location, const string &message) {
    if (!errors.empty())
        errors += "\n";

    errors +=
        "Error processing serialized protobuf descriptor: " +
        filename +
        ": " +
        message;
}

void DescriptorLoader::ErrorCollector::AddWarning(const string &filename, const string &element_name, const Message *descriptor, DescriptorPool::ErrorCollector::ErrorLocation location, const string &message) {
    warn("Processing serialized protobuf descriptor: %s: %s", filename.c_str(), message.c_str());
}

DescriptorLoader::DescriptorLoader(SourceTree *source_tree,
                                   MultiFileErrorCollector *error_collector) :
        source_database(source_tree),
        binary_database(binary_pool),
        merged_database(&binary_database, &source_database),
        merged_pool(&merged_database, source_database.GetValidationErrorCollector()) {
    merged_pool.EnforceWeakDependencies(true);
    source_database.RecordErrorsTo(error_collector);

    #define ADD_WKT_FILE(name) add_descriptor_to_pool(&binary_pool, google::protobuf:: name ::descriptor())

    // only one WKT per .proto file is needed
    ADD_WKT_FILE(Duration);
    ADD_WKT_FILE(Timestamp);
    ADD_WKT_FILE(DoubleValue); // all types in wrappers.proto

    #undef ADD_WKT
}

DescriptorLoader::~DescriptorLoader() { }

const FileDescriptor *DescriptorLoader::load_proto(const string &filename) {
    return merged_pool.FindFileByName(filename);
}

const vector<const FileDescriptor *> DescriptorLoader::load_serialized(const char *buffer, size_t length) {
    FileDescriptorSet fds;
    DescriptorLoader::ErrorCollector collector;

    if (!fds.ParseFromArray(buffer, length))
        croak("Error deserializing message descriptors");
    vector<const FileDescriptor *> result;

    for (int i = 0, max = fds.file_size(); i < max; ++i) {
        const FileDescriptorProto &file = fds.file(i);
        const string &file_name = file.name();

        // for well-known-types, use the compiled-in version
        if (file_name.length() > wkt_prefix_length &&
            file_name.at(0) == 'g' &&
            file_name.compare(0, wkt_prefix_length, wkt_prefix) == 0 &&
            (file_name.compare(wkt_prefix_length, string::npos, "duration.proto") == 0 ||
             file_name.compare(wkt_prefix_length, string::npos, "timestamp.proto") == 0 ||
             file_name.compare(wkt_prefix_length, string::npos, "wrappers.proto") == 0)) {
            continue;
        }

        result.push_back(binary_pool.BuildFileCollectingErrors(file, &collector));
    }

    if (!collector.errors.empty())
        croak("%s", collector.errors.c_str());

    return result;
}

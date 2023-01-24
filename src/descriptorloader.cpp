#include "descriptorloader.h"
#include "unordered_map.h"

#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/duration.pb.h>
#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/wrappers.pb.h>

#include "EXTERN.h"
#include "perl.h"

using namespace google::protobuf::compiler;
using namespace google::protobuf;
using namespace gpd;
using namespace std;

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
        generated_database(*DescriptorPool::generated_pool()),
        source_database(source_tree, &generated_database),
        binary_database(binary_pool),
        merged_source_binary_database(&binary_database, &source_database),
        merged_pool(&merged_source_binary_database, source_database.GetValidationErrorCollector()) {
    merged_pool.EnforceWeakDependencies(true);
    source_database.RecordErrorsTo(error_collector);

    // make sure the descriptors are available in the generated pool (doing this for one descriptor
    // pulls in all the descriptors in the same file)
    Duration::descriptor();
    Timestamp::descriptor();
    FloatValue::descriptor();
    DescriptorProto::descriptor();
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

    for (int i = 0, max = fds.file_size(); i < max; ++i)
        result.push_back(binary_pool.BuildFileCollectingErrors(fds.file(i), &collector));

    if (!collector.errors.empty())
        croak("%s", collector.errors.c_str());

    return result;
}

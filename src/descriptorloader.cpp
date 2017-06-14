#include "descriptorloader.h"

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
        source_database(source_tree),
        binary_database(binary_pool),
        merged_database(&binary_database, &source_database),
        merged_pool(&merged_database, source_database.GetValidationErrorCollector()) {
    merged_pool.EnforceWeakDependencies(true);
    source_database.RecordErrorsTo(error_collector);
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
        croak(collector.errors.c_str());

    return result;
}

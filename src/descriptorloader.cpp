#include "descriptorloader.h"
#include "unordered_map.h"

#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/duration.pb.h>
#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/wrappers.pb.h>
#include <google/protobuf/io/zero_copy_stream.h>

#include "EXTERN.h"
#include "perl.h"

using namespace google::protobuf::compiler;
using namespace google::protobuf;
using namespace gpd;
using namespace std;

namespace {
    void PerlLogHandler(LogLevel level, const char *filename, int line,
                        const std::string &message) {
        const char *level_str = NULL;

        switch (level) {
        case LOGLEVEL_WARNING:
            level_str = "";
            break;
        case LOGLEVEL_ERROR:
            level_str = " error";
            break;
        case LOGLEVEL_FATAL:
            level_str = " fatal error";
            break;
        default:
            return;
        }

        warn("protobuf%s: %s [%s:%d]", level_str, message.c_str(), filename, line);
    }

    class CaptureWarnings {
    public:
        CaptureWarnings() {
            previous = SetLogHandler(PerlLogHandler);
        }

        ~CaptureWarnings() {
            SetLogHandler(previous);
        }

    private:
        LogHandler *previous;
    };
}

SourceTreeDescriptorDatabaseWithFallback::SourceTreeDescriptorDatabaseWithFallback(
    SourceTree *source_tree, DescriptorDatabase *fallback_database) :
        source_tree(source_tree),
        database(source_tree),
        fallback_database(fallback_database) {
}

bool SourceTreeDescriptorDatabaseWithFallback::FindFileByName(
        const std::string& filename,
        FileDescriptorProto* output) {
    // this will open the file twice, but I prefer that to copy more of
    // SourceTreeDescriptorDatabase implementation
    google::protobuf::io::ZeroCopyInputStream *input = source_tree->Open(filename);

    if (input == NULL) {
        if (fallback_database->FindFileByName(filename, output)) {
            return true;
        }
        // if file is not found, FindFileByName will report the error,
        // no need to do it here (other than it would be slightly more efficient)
    }

    delete input;
    return database.FindFileByName(filename, output);
}

void DescriptorLoader::CollectMultiFileErrors::AddError(const string &filename, int line, int column, const string &message) {
    if (!errors.empty())
        errors += "\n";

    errors +=
        "Error during protobuf parsing: " +
        filename + ":" + to_string(line) + ":" + to_string(column) + ": " +
        message;
}

void DescriptorLoader::CollectMultiFileErrors::AddWarning(const string &filename, int line, int column, const string &message) {
    // seems to never be called, warnings go to log
    warn("Parsing protobuf file: %s:%d:%d: %s", filename.c_str(), line, column, message.c_str());
}

void DescriptorLoader::CollectMultiFileErrors::maybe_croak() {
    if (errors.empty())
        return;

    string copy = errors;

    errors.clear();
    croak("%s", copy.c_str());
}

DescriptorLoader::DescriptorLoader() :
        overlay_source_tree(&memory_source_tree, &disk_source_tree),
        generated_database(*DescriptorPool::generated_pool()),
        source_database(&overlay_source_tree, &generated_database),
        merged_source_binary_database(&binary_database, &source_database),
        merged_pool(&merged_source_binary_database, source_database.GetValidationErrorCollector()) {
    merged_pool.EnforceWeakDependencies(true);
    source_database.RecordErrorsTo(&multifile_error_collector);

    // make sure the descriptors are available in the generated pool (doing this for one descriptor
    // pulls in all the descriptors in the same file)
    Duration::descriptor();
    Timestamp::descriptor();
    FloatValue::descriptor();
    DescriptorProto::descriptor();
}

DescriptorLoader::~DescriptorLoader() { }

const FileDescriptor *DescriptorLoader::load_proto(const string &filename) {
    CaptureWarnings capture_warnings;

    return merged_pool.FindFileByName(filename);
}

const vector<const FileDescriptor *> DescriptorLoader::load_serialized(const char *buffer, size_t length) {
    CaptureWarnings capture_warnings;
    FileDescriptorSet fds;

    if (!fds.ParseFromArray(buffer, length))
        croak("Error deserializing message descriptors");
    vector<const FileDescriptor *> result;

    for (int i = 0, max = fds.file_size(); i < max; ++i) {
        const FileDescriptorProto &file = fds.file(i);

        if (!binary_database.Add(file))
            break;

        const FileDescriptor *file_def = merged_pool.FindFileByName(file.name());
        if (file_def == NULL)
            break;
        result.push_back(file_def);
    }

    return result;
}

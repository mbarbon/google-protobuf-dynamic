#include "sourcetree.h"

#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

using namespace gpd;
using namespace std;
using namespace UMS_NS;
using namespace google::protobuf;
using namespace google::protobuf::io;
using namespace google::protobuf::compiler;

OverlaySourceTree::OverlaySourceTree(MemorySourceTree *_memory, SourceTree *_fallback) {
    memory = _memory;
    fallback = _fallback;
}

ZeroCopyInputStream *OverlaySourceTree::Open(const string &filename) {
    ZeroCopyInputStream *mem = memory->Open(filename);
    if (mem)
        return mem;

    return fallback->Open(filename);
}

string OverlaySourceTree::GetLastErrorMessage() {
    return fallback->GetLastErrorMessage();
}

void MemorySourceTree::AddFile(const string &filename, const char *data, size_t len) {
    sources[filename].assign(data, len);
}

ZeroCopyInputStream *MemorySourceTree::Open(const string &filename) {
    unordered_map<string, string>::iterator item = sources.find(filename);
    if (item == sources.end())
        return NULL;

    return new ArrayInputStream(item->second.data(), item->second.size());
}

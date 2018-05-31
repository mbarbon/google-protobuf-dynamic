#ifndef _GPD_XS_SOURCETREE_INCLUDED
#define _GPD_XS_SOURCETREE_INCLUDED

#undef New
#undef Move
#undef do_open
#undef do_close

#include <google/protobuf/compiler/importer.h>

#include "unordered_map.h"

namespace gpd {

class MemorySourceTree;

class OverlaySourceTree : public google::protobuf::compiler::SourceTree {
public:
    OverlaySourceTree(MemorySourceTree *memory, google::protobuf::compiler::SourceTree *fallback);

    virtual google::protobuf::io::ZeroCopyInputStream *Open(const std::string &filename);
    virtual std::string GetLastErrorMessage();

private:
    MemorySourceTree *memory;
    google::protobuf::compiler::SourceTree *fallback;
};

class MemorySourceTree : public google::protobuf::compiler::SourceTree {
public:
    void AddFile(const std::string &filename, const char *data, size_t len);
    virtual google::protobuf::io::ZeroCopyInputStream *Open(const std::string &filename);

private:
    STD_TR1::unordered_map<std::string, std::string> sources;
};

}
#endif

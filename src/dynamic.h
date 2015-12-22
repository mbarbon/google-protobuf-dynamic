#ifndef _GPD_XS_DYNAMIC_INCLUDED
#define _GPD_XS_DYNAMIC_INCLUDED

#undef New

#include <google/protobuf/compiler/importer.h>
#include "mapper.h"

#include <tr1/unordered_map>

#include "EXTERN.h"
#include "perl.h"

namespace gpd {

class Dynamic {
    class CollectErrors : public google::protobuf::compiler::MultiFileErrorCollector {
        virtual void AddError(const std::string &filename, int line, int column, const std::string &message);
    };

public:
    Dynamic(const std::string &root_directory);
    ~Dynamic();

    void load_file(const std::string &file);
    void map_message(const std::string &message, const std::string &package);
    void resolve_references();

private:
    google::protobuf::compiler::Importer importer;
    google::protobuf::compiler::DiskSourceTree source_tree;
    CollectErrors die_on_error;
    std::tr1::unordered_map<const google::protobuf::Descriptor *, const Mapper *> descriptor_map;
    std::tr1::unordered_map<std::string, const Mapper *> package_map;
    std::vector<Mapper *> pending;
};

};

#endif

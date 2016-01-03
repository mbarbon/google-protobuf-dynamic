#ifndef _GPD_XS_DYNAMIC_INCLUDED
#define _GPD_XS_DYNAMIC_INCLUDED

#undef New

#include <google/protobuf/compiler/importer.h>
#include <upb/bindings/googlepb/bridge.h>

#include "sourcetree.h"

#include <tr1/unordered_map>
#include <tr1/unordered_set>

#include "EXTERN.h"
#include "perl.h"

namespace gpd {

class Mapper;

class Dynamic {
    class CollectErrors : public google::protobuf::compiler::MultiFileErrorCollector {
        virtual void AddError(const std::string &filename, int line, int column, const std::string &message);
    };

public:
    Dynamic(const std::string &root_directory);
    ~Dynamic();

    void load_file(const std::string &file);
    void load_string(const std::string &file, SV *string);

    void map_message(const std::string &message, const std::string &perl_package);
    void map_package(const std::string &pb_package, const std::string &perl_package_prefix);
    void resolve_references();

    const Mapper *find_mapper(const upb::MessageDef *message_def) const;

private:
    void map_message(const google::protobuf::Descriptor *descriptor, const std::string &perl_package);

    google::protobuf::compiler::Importer importer;
    google::protobuf::compiler::DiskSourceTree disk_source_tree;
    MemorySourceTree memory_source_tree;
    OverlaySourceTree overlay_source_tree;
    upb::googlepb::DefBuilder def_builder;
    CollectErrors die_on_error;
    std::tr1::unordered_map<std::string, const Mapper *> descriptor_map;
    std::tr1::unordered_map<std::string, const Mapper *> package_map;
    std::tr1::unordered_set<const google::protobuf::FileDescriptor *> files;
    std::vector<Mapper *> pending;
};

};

#endif

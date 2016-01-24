#ifndef _GPD_XS_DYNAMIC_INCLUDED
#define _GPD_XS_DYNAMIC_INCLUDED

#undef New

#include <google/protobuf/compiler/importer.h>
#include <upb/bindings/googlepb/bridge.h>

#include "descriptorloader.h"
#include "sourcetree.h"
#include "ref.h"

#include <tr1/unordered_map>
#include <tr1/unordered_set>

#include "EXTERN.h"
#include "perl.h"

namespace gpd {

class Mapper;

struct MappingOptions {
    bool use_bigints;
    bool check_required_fields;
    bool explicit_defaults;
    bool encode_defaults;
    bool check_enum_values;
    bool generic_extension_methods;

    MappingOptions(pTHX_ SV *options_ref);
};

class Dynamic : public Refcounted {
    class CollectErrors : public google::protobuf::compiler::MultiFileErrorCollector {
        virtual void AddError(const std::string &filename, int line, int column, const std::string &message);
    };

public:
    Dynamic(const std::string &root_directory);
    ~Dynamic();

    void load_file(pTHX_ const std::string &file);
    void load_string(pTHX_ const std::string &file, SV *string);
    void load_serialized_string(pTHX_ SV *sv);

    void map_message(pTHX_ const std::string &message, const std::string &perl_package, const MappingOptions &options);
    void map_package(pTHX_ const std::string &pb_package, const std::string &perl_package_prefix, const MappingOptions &options);
    void map_enum(pTHX_ const std::string &enum_name, const std::string &perl_package, const MappingOptions &options);
    void resolve_references();

    const Mapper *find_mapper(const upb::MessageDef *message_def) const;

private:
    void map_message_recursive(pTHX_ const google::protobuf::Descriptor *descriptor, const std::string &perl_package, const MappingOptions &options);
    void map_message(pTHX_ const google::protobuf::Descriptor *descriptor, const std::string &perl_package, const MappingOptions &options);
    void map_enum(pTHX_ const google::protobuf::EnumDescriptor *descriptor, const std::string &perl_package, const MappingOptions &options);
    void check_package(pTHX_ const std::string &perl_package, const std::string &pb_name);

    OverlaySourceTree overlay_source_tree;
    DescriptorLoader descriptor_loader;
    google::protobuf::compiler::DiskSourceTree disk_source_tree;
    MemorySourceTree memory_source_tree;
    upb::googlepb::DefBuilder def_builder;
    CollectErrors die_on_error;
    std::tr1::unordered_map<std::string, const Mapper *> descriptor_map;
    std::tr1::unordered_set<std::string> used_packages;
    std::tr1::unordered_set<std::string> mapped_enums;
    std::tr1::unordered_set<const google::protobuf::FileDescriptor *> files;
    std::vector<Mapper *> pending;
};

};

#endif

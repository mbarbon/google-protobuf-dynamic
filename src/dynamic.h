#ifndef _GPD_XS_DYNAMIC_INCLUDED
#define _GPD_XS_DYNAMIC_INCLUDED

#include "perl_unpollute.h"

#include <google/protobuf/compiler/importer.h>

#include "upb/bridge.h"

#include "descriptorloader.h"
#include "sourcetree.h"
#include "ref.h"

#include "unordered_map.h"

#include "EXTERN.h"
#include "perl.h"

namespace gpd {

class Mapper;
class MethodMapper;
class ServiceDef;

struct MappingOptions {
    enum AccessorStyle {
        GetAndSet = 1,
        PlainAndSet = 2,
        SingleAccessor = 3,
        Plain = 4,
    };

    enum ClientService {
        Disable = 0,
        Noop = 1,
        GrpcXS = 2,
    };

    bool use_bigints;
    bool check_required_fields;
    bool explicit_defaults;
    bool encode_defaults;
    bool encode_defaults_proto3;
    bool check_enum_values;
    bool generic_extension_methods;
    bool implicit_maps;
    bool decode_blessed;
    bool fail_ref_coercion;
    bool numeric_bool;
    bool no_redefine_perl_names;
    AccessorStyle accessor_style;
    ClientService client_services;

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
    void map_package_prefix(pTHX_ const std::string &pb_prefix, const std::string &perl_package_prefix, const MappingOptions &options);
    void map_message_prefix(pTHX_ const std::string &message, const std::string &perl_package_prefix, const MappingOptions &options);
    void map_enum(pTHX_ const std::string &enum_name, const std::string &perl_package, const MappingOptions &options);
    void map_service(pTHX_ const std::string &service_name, const std::string &perl_package, const MappingOptions &options);
    void resolve_references();

    const Mapper *find_mapper(const upb::MessageDef *message_def) const;

    static bool is_proto3() {
        return GOOGLE_PROTOBUF_VERSION >= 3000000;
    }

    static bool has_proto3_optional() {
        return GOOGLE_PROTOBUF_VERSION >= 3012000;
    }

private:
    void map_package_or_prefix(pTHX_ const std::string &pb_package, bool is_prefix, const std::string &perl_package_prefix, const MappingOptions &options);
    void map_message_recursive(pTHX_ const google::protobuf::Descriptor *descriptor, const std::string &perl_package, const MappingOptions &options);
    void map_message_prefix_recursive(pTHX_ const google::protobuf::Descriptor *descriptor, const std::string &perl_package_prefix, const MappingOptions &options, STD_TR1::unordered_set<std::string> &recursed_names);
    void map_message(pTHX_ const google::protobuf::Descriptor *descriptor, const std::string &perl_package, const MappingOptions &options);
    void bind_message(pTHX_ const std::string &perl_package, Mapper *mapper, HV *stash, const MappingOptions &options);
    void map_enum(pTHX_ const google::protobuf::EnumDescriptor *descriptor, const std::string &perl_package, const MappingOptions &options);
    void map_service(pTHX_ const google::protobuf::ServiceDescriptor *descriptor, const std::string &perl_package, const MappingOptions &options);
    void map_service_noop(pTHX_ const google::protobuf::ServiceDescriptor *descriptor, const std::string &perl_package, const MappingOptions &options, ServiceDef *service_def);
    void map_service_grpc_xs(pTHX_ const google::protobuf::ServiceDescriptor *descriptor, const std::string &perl_package, const MappingOptions &options, ServiceDef *service_def);
    void check_package(pTHX_ const std::string &perl_package, const std::string &pb_name);
    std::string pbname_to_package(pTHX_ const std::string &pb_name, const std::string &perl_package_prefix);

    OverlaySourceTree overlay_source_tree;
    DescriptorLoader descriptor_loader;
    google::protobuf::compiler::DiskSourceTree disk_source_tree;
    MemorySourceTree memory_source_tree;
    upb::googlepb::DefBuilder def_builder;
    CollectErrors die_on_error;
    STD_TR1::unordered_map<std::string, const Mapper *> descriptor_map;
    STD_TR1::unordered_set<std::string> used_packages;
    STD_TR1::unordered_set<std::string> mapped_enums;
    STD_TR1::unordered_set<std::string> mapped_services;
    STD_TR1::unordered_set<const google::protobuf::FileDescriptor *> files;
    std::vector<Mapper *> pending;
    std::vector<MethodMapper *> pending_methods;
};

// equivalent to FieldDescriptor::has_presence() in src/google/protobuf/descriptor.h
inline bool has_presence(const upb::FieldDef* field_def) {
    if (field_def->IsSequence())
        return false;
    if (field_def->IsSubMessage() || field_def->containing_oneof())
        return true;
    if (field_def->containing_type() && field_def->containing_type()->syntax() == UPB_SYNTAX_PROTO2)
        return true;
    return false;
}

};

#endif

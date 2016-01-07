#include "dynamic.h"
#include "mapper.h"

#include <google/protobuf/dynamic_message.h>

using namespace gpd;
using namespace std;
using namespace google::protobuf;
using namespace upb;
using namespace upb::googlepb;

void Dynamic::CollectErrors::AddError(const string &filename, int line, int column, const string &message) {
    croak("Error during protobuf parsing: %s:%d:%d: %s", filename.c_str(), line, column, message.c_str());
}

MappingOptions::MappingOptions(pTHX_ SV *options_ref) :
        use_bigints(sizeof(IV) < sizeof(int64_t)) {
    if (options_ref == NULL || !SvOK(options_ref))
        return;
    if (!SvROK(options_ref) || SvTYPE(SvRV(options_ref)) != SVt_PVHV)
        croak("options must be an hash reference");
    HV *options = (HV *) SvRV(options_ref);
    SV **bigints = hv_fetchs(options, "use_bigints", 0);

    if (*bigints)
        use_bigints = SvTRUE(*bigints);
}

Dynamic::Dynamic(const string &root_directory) :
        overlay_source_tree(&memory_source_tree, &disk_source_tree),
        importer(&overlay_source_tree, &die_on_error) {
    if (!root_directory.empty())
        disk_source_tree.MapPath("", root_directory);
}

Dynamic::~Dynamic() {
}

void Dynamic::load_file(pTHX_ const string &file) {
    const FileDescriptor *loaded = importer.Import(file);

    if (loaded)
        files.insert(loaded);
}

void Dynamic::load_string(pTHX_ const string &file, SV *sv) {
    STRLEN len;
    const char *data = SvPV(sv, len);
    string actual_file = file.empty() ? "<string>" : file;

    memory_source_tree.AddFile(actual_file, data, len);
    load_file(aTHX_ actual_file);
}

namespace {
    int free_mapper(pTHX_ SV *sv, MAGIC *mg) {
        Mapper *mapper = (Mapper *) mg->mg_ptr;

        mapper->unref();
    }

    MGVTBL manage_mapper = {
        NULL, // get
        NULL, // set
        NULL, // len
        NULL, // clear
        free_mapper,
        NULL, // copy
        NULL, // dup
        NULL, // local
    };

    void copy_and_bind(pTHX_ const char *name, const string &perl_package, Mapper *mapper) {
        static const char prefix[] = "Google::ProtocolBuffers::Dynamic::Mapper::";
        char buffer[sizeof(prefix) + 30];

        memcpy(buffer, prefix, sizeof(prefix));
        strcpy(buffer + sizeof(prefix) - 1, name);

        CV *src = get_cv(buffer, 0);
        CV *new_xs = newXS((perl_package + "::" + name).c_str(), CvXSUB(src), __FILE__);

        CvXSUBANY(new_xs).any_ptr = mapper;
        sv_magicext((SV *) new_xs, NULL,
                    PERL_MAGIC_ext, &manage_mapper,
                    (const char *) mapper, 0);
        mapper->ref();
    }
}

void Dynamic::map_message(pTHX_ const string &message, const string &perl_package, const MappingOptions &options) {
    const DescriptorPool *pool = importer.pool();
    const Descriptor *descriptor = pool->FindMessageTypeByName(message);

    if (descriptor == NULL) {
        croak("Unable to find a descriptor for message '%s'", message.c_str());
    }

    map_message_recursive(aTHX_ descriptor, perl_package, options);
}

void Dynamic::map_package(pTHX_ const string &pb_package, const string &perl_package_prefix, const MappingOptions &options) {
    for (std::tr1::unordered_set<const FileDescriptor *>::iterator it = files.begin(), en = files.end(); it != en; ++it) {
        const FileDescriptor *file = *it;

        if (pb_package != file->package())
            continue;

        for (int i = 0, max = file->message_type_count(); i < max; ++i) {
            const Descriptor *descriptor = file->message_type(i);
            if (descriptor_map.find(descriptor->full_name()) != descriptor_map.end())
                continue;

            map_message_recursive(aTHX_ descriptor, perl_package_prefix + "::" + descriptor->name(), options);
        }
    }
}

void Dynamic::map_message_recursive(pTHX_ const Descriptor *descriptor, const string &perl_package, const MappingOptions &options) {
    for (int i = 0, max = descriptor->nested_type_count(); i < max; ++i) {
        const Descriptor *inner = descriptor->nested_type(i);
        if (descriptor_map.find(inner->full_name()) != descriptor_map.end())
            continue;

        map_message_recursive(aTHX_ inner, perl_package + "::" + inner->name(), options);
    }

    map_message(aTHX_ descriptor, perl_package, options);
}

void Dynamic::map_message(pTHX_ const Descriptor *descriptor, const string &perl_package, const MappingOptions &options) {
    if (package_map.find(perl_package) != package_map.end())
        croak("Package '%s' has already been used in a message mapping", perl_package.c_str());
    if (descriptor_map.find(descriptor->full_name()) != descriptor_map.end())
        croak("Message '%s' has already been mapped", descriptor->full_name().c_str());
    if (options.use_bigints)
        eval_pv("require Math::BigInt", 1);

    const MessageDef *message_def = def_builder.GetMessageDef(descriptor);
    Mapper *mapper = new Mapper(aTHX_ this, message_def, options);

    descriptor_map[message_def->full_name()] = mapper;
    package_map[perl_package] = mapper;
    pending.push_back(mapper);

    copy_and_bind(aTHX_ "decode_to_perl", perl_package, mapper);
    copy_and_bind(aTHX_ "encode_from_perl", perl_package, mapper);

    mapper->unref(); // reference from constructor
}

void Dynamic::resolve_references() {
    for (std::vector<Mapper *>::iterator it = pending.begin(), en = pending.end(); it != en; ++it)
        (*it)->resolve_mappers();
}

const Mapper *Dynamic::find_mapper(const MessageDef *message_def) const {
    std::tr1::unordered_map<string, const Mapper *>::const_iterator item = descriptor_map.find(message_def->full_name());

    if (item == descriptor_map.end())
        croak("Unknown type '%s'", message_def->full_name());

    return item->second;
}


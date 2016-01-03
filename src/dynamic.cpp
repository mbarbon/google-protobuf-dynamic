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

Dynamic::Dynamic(const string &root_directory) :
        overlay_source_tree(&memory_source_tree, &disk_source_tree),
        importer(&overlay_source_tree, &die_on_error) {
    if (!root_directory.empty())
        disk_source_tree.MapPath("", root_directory);
}

Dynamic::~Dynamic() {
}

void Dynamic::load_file(const string &file) {
    const FileDescriptor *loaded = importer.Import(file);

    if (loaded)
        files.insert(loaded);
}

void Dynamic::load_string(const string &file, SV *sv) {
    STRLEN len;
    const char *data = SvPV(sv, len);
    string actual_file = file.empty() ? "<string>" : file;

    memory_source_tree.AddFile(actual_file, data, len);
    load_file(actual_file);
}

namespace {
    void copy_and_bind(const char *name, const string &perl_package, Mapper *mapper) {
        static const char prefix[] = "Google::ProtocolBuffers::Dynamic::Mapper::";
        char buffer[sizeof(prefix) + 30];

        memcpy(buffer, prefix, sizeof(prefix));
        strcpy(buffer + sizeof(prefix) - 1, name);

        CV *src = get_cv(buffer, 0);
        CV *new_xs = newXS((perl_package + "::" + name).c_str(), CvXSUB(src), __FILE__);

        // XXX magic to free
        CvXSUBANY(new_xs).any_ptr = mapper;
    }
}

void Dynamic::map_message(const string &message, const string &perl_package) {
    const DescriptorPool *pool = importer.pool();
    const Descriptor *descriptor = pool->FindMessageTypeByName(message);

    if (descriptor == NULL) {
        croak("Unable to find a descriptor for message '%s'", message.c_str());
    }

    map_message(descriptor, perl_package);
}

void Dynamic::map_package(const string &pb_package, const string &perl_package_prefix) {
    for (std::tr1::unordered_set<const FileDescriptor *>::iterator it = files.begin(), en = files.end(); it != en; ++it) {
        const FileDescriptor *file = *it;

        if (pb_package != file->package())
            continue;

        for (int i = 0, max = file->message_type_count(); i < max; ++i) {
            const Descriptor *descriptor = file->message_type(i);
            if (descriptor_map.find(descriptor->full_name()) != descriptor_map.end())
                continue;

            map_message(descriptor, perl_package_prefix + "::" + descriptor->name());
        }
    }
}

void Dynamic::map_message(const Descriptor *descriptor, const string &perl_package) {
    if (package_map.find(perl_package) != package_map.end())
        croak("Package '%s' has already been used in a message mapping", perl_package.c_str());
    if (descriptor_map.find(descriptor->full_name()) != descriptor_map.end())
        croak("Message '%s' has already been mapped", descriptor->full_name().c_str());

    reffed_ptr<const MessageDef> message_def = def_builder.GetMessageDef(descriptor);
    Mapper *mapper = new Mapper(message_def);

    descriptor_map[message_def->full_name()] = mapper;
    package_map[perl_package] = mapper;
    pending.push_back(mapper);

    copy_and_bind("decode_to_perl", perl_package, mapper);
    copy_and_bind("encode_from_perl", perl_package, mapper);
}

void Dynamic::resolve_references() {
    for (std::vector<Mapper *>::iterator it = pending.begin(), en = pending.end(); it != en; ++it) {
        (*it)->resolve_mappers(this);

        // XXX nested types, extensions
    }
}

const Mapper *Dynamic::find_mapper(const MessageDef *message_def) const {
    std::tr1::unordered_map<string, const Mapper *>::const_iterator item = descriptor_map.find(message_def->full_name());

    if (item == descriptor_map.end())
        croak("Unknown type '%s'", message_def->full_name());

    return item->second;
}


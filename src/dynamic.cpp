#include "dynamic.h"
#include "mapper.h"

#include <google/protobuf/dynamic_message.h>

using namespace gpd;
using namespace std;
using namespace google::protobuf;

void Dynamic::CollectErrors::AddError(const string &filename, int line, int column, const string &message) {
    croak("Error during protobuf parsing: %s:%d:%d: %s", filename.c_str(), line, column, message.c_str());
}

Dynamic::Dynamic(const string &root_directory) :
        importer(&source_tree, &die_on_error) {
    source_tree.MapPath("", root_directory);
}

Dynamic::~Dynamic() {
}

void Dynamic::load_file(const string &file) {
    importer.Import(file);
}

namespace {
    void copy_and_bind(const char *name, const string &package, Mapper *mapper) {
        static const char prefix[] = "Google::ProtocolBuffers::Dynamic::Mapper::";
        char buffer[sizeof(prefix) + 30];

        memcpy(buffer, prefix, sizeof(prefix));
        strcpy(buffer + sizeof(prefix) - 1, name);

        CV *src = get_cv(buffer, 0);
        CV *new_xs = newXS((package + "::" + name).c_str(), CvXSUB(src), __FILE__);

        // XXX magic to free
        CvXSUBANY(new_xs).any_ptr = mapper;
    }
}

void Dynamic::map_message(const string &message, const string &package) {
    const DescriptorPool *pool = importer.pool();
    const Descriptor *descriptor = pool->FindMessageTypeByName(message);

    if (descriptor == NULL) {
        croak("Unable to find a descriptor for message '%s'", message.c_str());
    }

    Mapper *mapper = new Mapper();
    mapper->factory = new DynamicMessageFactory(pool);
    mapper->prototype = mapper->factory->GetPrototype(descriptor);
    mapper->reflection = mapper->prototype->GetReflection();

    descriptor_map[descriptor] = mapper;
    package_map[package] = mapper;
    pending.push_back(mapper);

    copy_and_bind("decode_to_perl", package, mapper);
    copy_and_bind("encode_from_perl", package, mapper);
}

void Dynamic::resolve_references() {
    for (std::vector<Mapper *>::iterator it = pending.begin(), en = pending.end(); it != en; ++it) {
        Mapper *mapper = *it;
        const Descriptor *descriptor = mapper->prototype->GetDescriptor();

        for (int i = 0, max = descriptor->field_count(); i < max; ++i) {
            const FieldDescriptor *fd = descriptor->field(i);
            if (fd->type() != FieldDescriptor::TYPE_GROUP &&
                    fd->type() != FieldDescriptor::TYPE_MESSAGE)
                continue;

            const Mapper *field_mapper = descriptor_map[fd->message_type()];
            if (!field_mapper)
                croak("Unknown type '%s'", fd->message_type()->full_name().c_str());

            mapper->descriptor_map[fd->message_type()] = field_mapper;
        }

        // XXX nested types, extensions
    }
}

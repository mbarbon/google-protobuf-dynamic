#include "defbuilder.h"

#include <upb/pb/decoder.h>

using namespace gpd;
using namespace std;
using namespace google::protobuf;
using namespace upb;
using namespace upb::pb;

DefBuilder::DefBuilder() {
}

DefBuilder::~DefBuilder() {
}

const MessageDefPtr DefBuilder::GetMessageDef(const Descriptor *descriptor) {
    add_files(descriptor->file());

    return symbol_table.LookupMessage(descriptor->full_name().c_str());
}

const EnumDefPtr DefBuilder::GetEnumDef(const EnumDescriptor *descriptor) {
    add_files(descriptor->file());

    return symbol_table.LookupEnum(descriptor->full_name().c_str());
}

void DefBuilder::add_files(const FileDescriptor *fd) {
    if (mapped_files.find(fd->name()) != mapped_files.end())
        return;

    STD_TR1::unordered_set<std::string> mapped_files_new(mapped_files);
    FileDescriptorSet fds_proto;
    add_unmapped_files(&fds_proto, fd, mapped_files_new);

    size_t size = fds_proto.ByteSizeLong();
    vector<uint8> buffer(size, 0);

    fds_proto.SerializeWithCachedSizesToArray(&buffer[0]);

    upb::Arena arena;
    google_protobuf_FileDescriptorSet *parsed = google_protobuf_FileDescriptorSet_parse((const char *) &buffer[0], size, arena.ptr());

    size_t n;
    const google_protobuf_FileDescriptorProto *const *files = google_protobuf_FileDescriptorSet_file(parsed, &n);
    for (size_t i = 0; i < n; ++i) {
        symbol_table.AddFile(files[i], NULL);
    }

    mapped_files = mapped_files_new;
}

void DefBuilder::add_unmapped_files(FileDescriptorSet *fds, const FileDescriptor *fd, STD_TR1::unordered_set<string> &mapped_files_new)
{
    fd->CopyTo(fds->mutable_file()->Add());
    mapped_files_new.insert(fd->name());

    for (int i = 0; i < fd->dependency_count(); ++i) {
        const FileDescriptor *dependency = fd->dependency(i);

        if (mapped_files_new.find(dependency->name()) == mapped_files_new.end())
            add_unmapped_files(fds, dependency, mapped_files_new);
    }
}

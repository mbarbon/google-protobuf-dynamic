#include "defbuilder.h"

#if 0

#include <upb/descriptor/reader.h>
#include <upb/pb/decoder.h>

using namespace gpd;
using namespace std;
using namespace google::protobuf;
using namespace upb;
using namespace upb::descriptor;
using namespace upb::pb;

DefBuilder::DefBuilder() :
        symbol_table(SymbolTable::New()) {
}

DefBuilder::~DefBuilder() {
    SymbolTable::Free(symbol_table);
}

const MessageDef *DefBuilder::GetMessageDef(const Descriptor *descriptor) {
    add_files(descriptor->file());

    return symbol_table->LookupMessage(descriptor->full_name().c_str());
}

const EnumDef *DefBuilder::GetEnumDef(const EnumDescriptor *descriptor) {
    add_files(descriptor->file());

    return upb_symtab_lookupenum(symbol_table, descriptor->full_name().c_str());
}

void DefBuilder::add_files(const FileDescriptor *fd) {
    if (mapped_files.find(fd->name()) != mapped_files.end())
        return;

    STD_TR1::unordered_set<std::string> mapped_files_new(mapped_files);
    FileDescriptorSet fds_proto;
    add_unmapped_files(&fds_proto, fd, mapped_files_new);

    Environment env;
    const Handlers *handlers = upb_descreader_newhandlers(&env);
    Reader *reader = Reader::Create(&env, handlers);
    upb::reffed_ptr<const DecoderMethod> pb_decoder_method = DecoderMethod::New(DecoderMethodOptions(handlers));

    size_t size = fds_proto.ByteSizeLong();
    vector<uint8> buffer(size, 0);

    fds_proto.SerializeWithCachedSizesToArray(&buffer[0]);
    upb::pb::Decoder *pb_decoder = upb::pb::Decoder::Create(&env, pb_decoder_method.get(), reader->input());
    pb_decoder->Reset();

    BufferSource::PutBuffer((const char *) &buffer[0], size, pb_decoder->input());

    for (size_t i = 0; i < reader->file_count(); ++i) {
        symbol_table->AddFile(reader->file(i), NULL);
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

#endif

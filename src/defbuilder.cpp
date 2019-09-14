#include "defbuilder.h"

#include <upb/descriptor/reader.h>
#include <upb/pb/decoder.h>

#include <google/protobuf/descriptor.pb.h>

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
    string full_name = descriptor->full_name();
    const FileDescriptor *fd = descriptor->file();
    FileDescriptorSet fds_proto;
    Environment env;
    const Handlers *handlers = upb_descreader_newhandlers(&env);
    Reader *reader = Reader::Create(&env, handlers);
    upb::reffed_ptr<const DecoderMethod> pb_decoder_method = DecoderMethod::New(DecoderMethodOptions(handlers));

    fd->CopyTo(fds_proto.mutable_file()->Add());
    size_t size = fds_proto.ByteSizeLong();
    vector<uint8> buffer(size, 0);

    fds_proto.SerializeWithCachedSizesToArray(&buffer[0]);
    upb::pb::Decoder *pb_decoder = upb::pb::Decoder::Create(&env, pb_decoder_method.get(), reader->input());
    pb_decoder->Reset();

    BufferSource::PutBuffer((const char *) &buffer[0], size, pb_decoder->input());

    symbol_table->AddFile(reader->file(0), NULL);

    return symbol_table->LookupMessage(full_name.c_str());
}

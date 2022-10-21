#ifndef _GPD_XS_PB_GPB_MAPPING_INCLUDED
#define _GPD_XS_PB_GPB_MAPPING_INCLUDED

namespace google {
namespace protobuf {
class Descriptor;
class DescriptorPool;
}
}

namespace gpd {
namespace pb {

class DescriptorSet;
class Descriptor;

const gpd::pb::Descriptor *map_pb_descriptor(gpd::pb::DescriptorSet *descriptor_set, const google::protobuf::Descriptor *descriptor, const google::protobuf::DescriptorPool *descriptor_pool);

}
}

#endif

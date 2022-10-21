#include "pb/gpb_mapping.h"
#include "pb/decoder.h"

#include <google/protobuf/descriptor.h>

namespace gpdp = gpd::pb;
namespace gp = google::protobuf;
using namespace std;

namespace {
    void populate_field(gpdp::DescriptorSet *descriptor_set, gpdp::Descriptor *gpd_descriptor, const gp::FieldDescriptor *field, const gp::DescriptorPool *descriptor_pool) {
        bool repeated = field->label() == gp::FieldDescriptor::LABEL_REPEATED;

        if (field->type() == gp::FieldDescriptor::TYPE_MESSAGE) {
            const gpdp::Descriptor *field_message = map_pb_descriptor(descriptor_set, field->message_type(), descriptor_pool);

            gpd_descriptor->add_field(field->number(), repeated, field_message);
        } else {
            gpd_descriptor->add_field(field->number(), gpdp::FieldType(field->type()), repeated);
        }
    }

    void populate_descriptor(gpdp::DescriptorSet *descriptor_set, gpdp::Descriptor *gpd_descriptor, const gp::Descriptor *descriptor, const gp::DescriptorPool *descriptor_pool) {
        for (int i = 0, max = descriptor->field_count(); i < max; ++i) {
            const gp::FieldDescriptor *field = descriptor->field(i);

            populate_field(descriptor_set, gpd_descriptor, field, descriptor_pool);
        }

        vector<const gp::FieldDescriptor *> extensions;
        descriptor_pool->FindAllExtensions(descriptor, &extensions);
        for (vector<const gp::FieldDescriptor *>::const_iterator it = extensions.begin(), en = extensions.end(); it != en; ++it) {
            populate_field(descriptor_set, gpd_descriptor, *it, descriptor_pool);
        }
    }
}

const gpdp::Descriptor *gpdp::map_pb_descriptor(gpdp::DescriptorSet *descriptor_set, const gp::Descriptor *descriptor, const gp::DescriptorPool *descriptor_pool) {
    string message_name = descriptor->full_name();
    const gpdp::Descriptor *gpd_descriptor = descriptor_set->get_descriptor(message_name);
    if (!gpd_descriptor) {
        gpdp::Descriptor *new_gpd_descriptor = new gpdp::Descriptor();
        descriptor_set->add_descriptor(message_name, new_gpd_descriptor);

        populate_descriptor(descriptor_set, new_gpd_descriptor, descriptor, descriptor_pool);
        gpd_descriptor = new_gpd_descriptor;
    }

    return gpd_descriptor;
}

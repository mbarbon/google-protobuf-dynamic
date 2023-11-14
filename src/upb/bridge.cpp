// This file is a stripped-down version of upb/bindings/googlepb/bridge.cpp
// from an old version of uPB.

#include "upb/bridge.h"

#include <stdio.h>
#include <map>
#include <string>
#include <upb/def.h>

#define ASSERT_STATUS(status) do { \
  if (!upb_ok(status)) { \
    fprintf(stderr, "upb status failure: %s\n", upb_status_errmsg(status)); \
    UPB_ASSERT(upb_ok(status)); \
  } \
  } while (0)

#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.pb.h>
namespace goog = ::google::protobuf;

#if GOOGLE_PROTOBUF_VERSION >= 2006000
#define GOOGLE_PROTOBUF_HAS_ONEOF
#endif

#if GOOGLE_PROTOBUF_VERSION >= 3012000
#define GOOGLE_PROTOBUF_HAS_PROTO3_OPTIONAL
#endif

namespace upb {
namespace googlepb {

/* DefBuilder  ****************************************************************/

const EnumDef* DefBuilder::GetEnumDef(const goog::EnumDescriptor* ed) {
  const EnumDef* cached = FindInCaches<EnumDef>(ed->full_name(), ed);
  if (cached) return cached;

  EnumDef* e = AddToCaches(ed->full_name(), ed, EnumDef::New());

  Status status;
  e->set_full_name(ed->full_name(), &status);
  for (int i = 0; i < ed->value_count(); i++) {
    const goog::EnumValueDescriptor* val = ed->value(i);
    bool success = e->AddValue(val->name(), val->number(), &status);
    UPB_ASSERT(success);
  }

  e->Freeze(&status);

  ASSERT_STATUS(&status);
  return e;
}

const MessageDef* DefBuilder::GetMaybeUnfrozenMessageDef(
    const goog::Descriptor* d, const goog::Message* m) {
  const MessageDef* cached = FindInCaches<MessageDef>(d->full_name(), d);
  if (cached) return cached;
  MessageDef* md = AddToCaches(d->full_name(), d, MessageDef::New());
  to_freeze_.push_back(upb::upcast(md));

  Status status;
  md->set_full_name(d->full_name(), &status);
  ASSERT_STATUS(&status);

#if GOOGLE_PROTOBUF_VERSION >= 3000000
  upb_msgdef_setmapentry(md, d->options().map_entry());
  if (d->file()->syntax() == goog::FileDescriptor::SYNTAX_PROTO3) {
    upb_msgdef_setsyntax(md, UPB_SYNTAX_PROTO3);
  }
#endif
  // Find all regular fields and extensions for this message.
  std::vector<const goog::FieldDescriptor*> fields;
  d->file()->pool()->FindAllExtensions(d, &fields);
  for (int i = 0; i < d->field_count(); i++) {
    fields.push_back(d->field(i));
  }

#ifdef GOOGLE_PROTOBUF_HAS_ONEOF
  // Oneof fields
  for (int i = 0, maxi = d->oneof_decl_count(); i < maxi; ++i) {
    const goog::OneofDescriptor *proto2_oneof = d->oneof_decl(i);
    reffed_ptr<OneofDef> oneof = NewOneofDef(proto2_oneof);
    for (int j = 0, maxj = proto2_oneof->field_count(); j < maxj; ++j) {
      const goog::FieldDescriptor *proto2_f = proto2_oneof->field(j);
      reffed_ptr<FieldDef> field = NewFieldDef(proto2_f, m);
#ifdef GOOGLE_PROTOBUF_HAS_PROTO3_OPTIONAL
      upb_fielddef_setproto3optional(field.get(), proto2_f->real_containing_oneof() == NULL);
#endif
      oneof->AddField(field, &status);
    }
    md->AddOneof(oneof, &status);
  }
#endif

  for (size_t i = 0; i < fields.size(); i++) {
    const goog::FieldDescriptor* proto2_f = fields[i];
    UPB_ASSERT(proto2_f);
#ifdef GOOGLE_PROTOBUF_HAS_ONEOF
    // already added when adding the containing oneof
    if (proto2_f->containing_oneof())
      continue;
#endif
    md->AddField(NewFieldDef(proto2_f, m), &status);
  }
  ASSERT_STATUS(&status);
  return md;
}

reffed_ptr<FieldDef> DefBuilder::NewFieldDef(const goog::FieldDescriptor* f,
                                             const goog::Message* m) {
  reffed_ptr<FieldDef> upb_f(FieldDef::New());
  Status status;
  upb_f->set_number(f->number(), &status);
  upb_f->set_label(FieldDef::ConvertLabel(f->label()));
  upb_f->set_descriptor_type(FieldDef::ConvertDescriptorType(f->type()));
  upb_f->set_packed(f->is_packed());

  if (f->is_extension()) {
    upb_f->set_name(f->full_name(), &status);
    upb_f->set_is_extension(true);
  } else {
    upb_f->set_name(f->name(), &status);
  }

  const goog::Message* subm = NULL;

  if (m) {
    if (upb_f->type() == UPB_TYPE_MESSAGE) {
      UPB_ASSERT(subm);
    } else {
      // Weak field: subm will be weak prototype even though the proto2
      // descriptor does not indicate a submessage field.
      upb_f->set_descriptor_type(UPB_DESCRIPTOR_TYPE_MESSAGE);
    }
  }

  switch (upb_f->type()) {
    case UPB_TYPE_INT32:
      upb_f->set_default_int32(f->default_value_int32());
      break;
    case UPB_TYPE_INT64:
      upb_f->set_default_int64(f->default_value_int64());
      break;
    case UPB_TYPE_UINT32:
      upb_f->set_default_uint32(f->default_value_uint32());
      break;
    case UPB_TYPE_UINT64:
      upb_f->set_default_uint64(f->default_value_uint64());
      break;
    case UPB_TYPE_DOUBLE:
      upb_f->set_default_double(f->default_value_double());
      break;
    case UPB_TYPE_FLOAT:
      upb_f->set_default_float(f->default_value_float());
      break;
    case UPB_TYPE_BOOL:
      upb_f->set_default_bool(f->default_value_bool());
      break;
    case UPB_TYPE_STRING:
    case UPB_TYPE_BYTES:
      upb_f->set_default_string(f->default_value_string(), &status);
      break;
    case UPB_TYPE_MESSAGE: {
      const goog::Descriptor* subd = f->message_type();
      upb_f->set_message_subdef(GetMaybeUnfrozenMessageDef(subd, subm),
                                &status);
      break;
    }
    case UPB_TYPE_ENUM:
      // We set the enum default numerically.
      upb_f->set_default_int32(f->default_value_enum()->number());
      upb_f->set_enum_subdef(GetEnumDef(f->enum_type()), &status);
      break;
  }

  ASSERT_STATUS(&status);
  return upb_f;
}

#ifdef GOOGLE_PROTOBUF_HAS_ONEOF
reffed_ptr<OneofDef> DefBuilder::NewOneofDef(const goog::OneofDescriptor* o) {
  reffed_ptr<OneofDef> upb_o(OneofDef::New());
  Status status;

  upb_o->set_name(o->name().c_str(), &status);

  ASSERT_STATUS(&status);
  return upb_o;
}
#endif

void DefBuilder::Freeze() {
  // avoid upb using &to_freeze_[0] on an empty vector
  if (to_freeze_.empty())
      return;

  upb::Status status;
  upb::Def::Freeze(to_freeze_, &status);
  ASSERT_STATUS(&status);
  to_freeze_.clear();
}

const MessageDef* DefBuilder::GetMessageDef(const goog::Descriptor* d) {
  const MessageDef* ret = GetMaybeUnfrozenMessageDef(d, NULL);
  Freeze();
  return ret;
}

const MessageDef* DefBuilder::GetMessageDefExpandWeak(
    const goog::Message& m) {
  const MessageDef* ret = GetMaybeUnfrozenMessageDef(m.GetDescriptor(), &m);
  Freeze();
  return ret;
}

}  // namespace googlepb
}  // namespace upb

// This file is a stripped-down version of upb/bindings/googlepb/bridge.h
// from an old version of uPB.

#ifndef UPB_GOOGLE_BRIDGE_H_
#define UPB_GOOGLE_BRIDGE_H_

#include <map>
#include <vector>
#include <upb/handlers.h>
#include <upb/upb.h>

namespace google {
namespace protobuf {
class FieldDescriptor;
class Descriptor;
class EnumDescriptor;
class Message;
class OneofDescriptor;
}  // namespace protobuf
}  // namespace google

namespace upb {

namespace googlepb {

// Builds upb::Defs from proto2::Descriptors, and caches all built Defs for
// reuse.  CodeCache (below) uses this internally; there is no need to use this
// class directly unless you only want Defs without corresponding Handlers.
//
// This class is NOT thread-safe.
class DefBuilder {
 public:
  // Functions to get or create a Def from a corresponding proto2 Descriptor.
  // The returned def will be frozen.
  //
  // The caller must take a ref on the returned value if it needs it long-term.
  // The DefBuilder will retain a ref so it can keep the Def cached, but
  // garbage-collection functionality may be added to DefBuilder later that
  // could unref the returned pointer.
  const EnumDef* GetEnumDef(const ::google::protobuf::EnumDescriptor* d);
  const MessageDef* GetMessageDef(const ::google::protobuf::Descriptor* d);

  // Gets or creates a frozen MessageDef, properly expanding weak fields.
  //
  // Weak fields are only represented as BYTES fields in the Descriptor (unless
  // you construct your descriptors in a somewhat complicated way; see
  // https://goto.google.com/weak-field-descriptor), but we can get their true
  // definitions relatively easily from the proto Message class.
  const MessageDef* GetMessageDefExpandWeak(
      const ::google::protobuf::Message& m);

 private:
  // Like GetMessageDef*(), except the returned def might not be frozen.
  // We need this function because circular graphs of MessageDefs need to all
  // be frozen together, to we have to create the graphs of defs in an unfrozen
  // state first.
  //
  // If m is non-NULL, expands weak message fields.
  const MessageDef* GetMaybeUnfrozenMessageDef(
      const ::google::protobuf::Descriptor* d,
      const ::google::protobuf::Message* m);

  // Returns a new-unfrozen FieldDef corresponding to this FieldDescriptor.
  // The return value is always newly created (never cached) and the returned
  // pointer is the only owner of it.
  //
  // If "m" is non-NULL, expands the weak field if it is one, and populates
  // *subm_prototype with a prototype of the submessage if this is a weak or
  // non-weak MESSAGE or GROUP field.
  reffed_ptr<FieldDef> NewFieldDef(const ::google::protobuf::FieldDescriptor* f,
                                   const ::google::protobuf::Message* m);

  // only defined if GOOGLE_PROTOBUF_HAS_ONEOF is defined
  reffed_ptr<OneofDef> NewOneofDef(const ::google::protobuf::OneofDescriptor* o);

  // Freeze all defs that haven't been frozen yet.
  void Freeze();

  template <class T>
  T* AddToCaches(const std::string &name, const void *proto2_descriptor, reffed_ptr<T> def) {
    UPB_ASSERT(def_cache_.find(proto2_descriptor) == def_cache_.end());
    UPB_ASSERT(named_def_cache_.find(name) == named_def_cache_.end());
    def_cache_[proto2_descriptor] = def;
    named_def_cache_[name] = def;
    return def.get();  // Continued lifetime is guaranteed by cache.
  }

  template <class T>
  const T* FindInCaches(const std::string &name, const void *proto2_descriptor) {
    DefCache::iterator iter = def_cache_.find(proto2_descriptor);
    if (iter != def_cache_.end())
        return upb::down_cast<const T*>(iter->second.get());

    // doing this lookup might be just papering over a but elsewhere :-(
    NamedDefCache::iterator named_iter = named_def_cache_.find(name);
    return named_iter == named_def_cache_.end() ? NULL :
        upb::down_cast<const T*>(named_iter->second.get());
  }

 private:
  // Maps a proto2 descriptor to the corresponding upb Def we have constructed.
  // The proto2 descriptor is void* because the proto2 descriptor types do not
  // share a common base.
  typedef std::map<const void*, reffed_ptr<upb::Def> > DefCache;
  DefCache def_cache_;

  typedef std::map<std::string, reffed_ptr<upb::Def> > NamedDefCache;
  NamedDefCache named_def_cache_;

  // Defs that have not been frozen yet.
  std::vector<Def*> to_freeze_;
};

}  // namespace googlepb
}  // namespace upb

#endif  // UPB_GOOGLE_BRIDGE_H_

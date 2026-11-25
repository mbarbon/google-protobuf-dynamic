#ifndef _GPD_XS_DYNAMIC_STRING_VIEW
#define _GPD_XS_DYNAMIC_STRING_VIEW

#ifndef GOOGLE_PROTOBUF_VERSION
#error "GOOGLE_PROTOBUF_VERSION is not defined"
#endif

namespace gpd {

#if GOOGLE_PROTOBUF_VERSION >= 4022000

typedef absl::string_view pb_string_ref;
typedef absl::string_view pb_string;

inline std::string view_to_string(absl::string_view view) {
  return std::string(view.data(), view.size());
}

#else

typedef const std::string & pb_string_ref;
typedef std::string pb_string;

inline std::string view_to_string(const std::string &view) { return view; }

#endif

}

#endif
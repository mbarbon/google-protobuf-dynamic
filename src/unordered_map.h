#ifndef _GPD_XS_UNORDERED_MAP_INCLUDED
#define _GPD_XS_UNORDERED_MAP_INCLUDED

#if defined(__clang__) || (defined(__GNUC__) && __cplusplus >= 201103L)

#include <unordered_map>
#include <unordered_set>

#define UMS_NS       std
#define UMS_NS_START namespace std {
#define UMS_NS_END   }

#else

#include <tr1/unordered_map>
#include <tr1/unordered_set>

#define UMS_NS       std::tr1
#define UMS_NS_START namespace std { namespace tr1 {
#define UMS_NS_END   } }

#endif

#endif

#ifndef _GPD_XS_UNORDERED_MAP_INCLUDED
#define _GPD_XS_UNORDERED_MAP_INCLUDED

#if defined(__clang__) || (defined(__GNUC__) && __cplusplus >= 201103L)
#include <unordered_map>
#include <unordered_set>
#define STD_TR1 std
#else
#include <tr1/unordered_map>
#include <tr1/unordered_set>
#define STD_TR1 std::tr1
#endif

#endif

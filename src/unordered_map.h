#ifndef _GPD_XS_UNORDERED_MAP_INCLUDED
#define _GPD_XS_UNORDERED_MAP_INCLUDED

#ifdef __clang__
#include <unordered_map>
#include <unordered_set>
#define STD_TR1 std
#else
#include <tr1/unordered_map>
#include <tr1/unordered_set>
#define STD_TR1 std::tr1
#endif

#endif

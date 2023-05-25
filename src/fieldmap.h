#ifndef _GPD_XS_FIELDMAP_INCLUDED
#define _GPD_XS_FIELDMAP_INCLUDED

#include "perl_unpollute.h"

#include "unordered_map.h"

#include "EXTERN.h"
#include "perl.h"

#include <vector>

namespace gpd {

class PerlString {
public:
    const char *buffer;
    STRLEN len;
    U32 hash;

    bool operator==(const PerlString &other) const {
        return buffer == other.buffer ||
            (len == other.len && memcmp(buffer, other.buffer, len) == 0);
    }

    void fill(pTHX_ const char *buffer_, STRLEN len_) {
        buffer = buffer_;
        len = len_;
        PERL_HASH(hash, buffer, len);
    }

    // sv needs to be a shared SV
    void fill(pTHX_ SV *sv) {
        buffer = SvPV(sv, len);
        hash = SvSHARED_HASH(sv);
    }

    void fill(pTHX_ HE *he) {
        if (HeKLEN(he) == HEf_SVKEY) {
            buffer = SvPV(HeKEY_sv(he), len);
        } else {
            buffer = HeKEY(he);
            // if the key is marked as UTF-8 and contains non-ASCII characters,
            // it will not be there anyway in the lookup
            len = abs(HeKLEN(he));
        }
        hash = HeHASH(he);
    }
};

}

namespace STD_TR1 {

template<>
struct hash<gpd::PerlString> {
    size_t operator()(const gpd::PerlString &s) const {
        return s.hash;
    }
};

};

namespace gpd {

template<class T> class FieldMap;

class FieldMapImpl {
    template<class T> friend class FieldMap;

    typedef STD_TR1::unordered_map<PerlString, void *> NameMap;
    typedef STD_TR1::unordered_map<unsigned int, void *> NumberMap;
    typedef std::vector<void *> NumberVector;

    FieldMapImpl() {
        top_packed_field = 0;
    }

    void *find_by_name(pTHX_ SV *name) const;
    void *find_by_name(pTHX_ HE *he) const;
    void *find_by_name(pTHX_ const char *name, STRLEN namelen) const;

    void *find_by_name(const PerlString &key) const {
        NameMap::const_iterator it = by_name.find(key);

        return it == by_name.end() ? NULL : it->second;
    }

    void *find_by_number(unsigned int number) const {
        if (number <= top_packed_field) {
            return packed_by_number[number];
        } else {
            typename NumberMap::const_iterator it = by_number.find(number);

            return it != by_number.end() ? it->second : NULL;
        }
    }

    void add(pTHX_ SV *name, unsigned int number, void *field);

    void optimize_lookup();

    unsigned int top_packed_field;
    NameMap by_name;
    NumberMap by_number;
    NumberVector packed_by_number;
};

template<class T>
class FieldMap {
public:
    void add(pTHX_ SV *name, unsigned int number, T *field) {
        impl.add(aTHX_ name, number, field);
    }

    void optimize_lookup() {
        impl.optimize_lookup();
    }

    // name needs to be a shared SV
    T *find_by_name(pTHX_ SV *name) const {
        return static_cast<T *>(impl.find_by_name(aTHX_ name));
    }

    T *find_by_name(pTHX_ HE *he) const {
        return static_cast<T *>(impl.find_by_name(aTHX_ he));
    }

    T *find_by_name(pTHX_ const char *name, STRLEN namelen) const {
        return static_cast<T *>(impl.find_by_name(aTHX_ name, namelen));
    }

    T *find_by_number(unsigned int number) const {
        return static_cast<T *>(impl.find_by_number(number));
    }

private:
    FieldMapImpl impl;
};

}

#endif

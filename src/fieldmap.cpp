#include "fieldmap.h"

#include "perl_unpollute.h"

#include <algorithm>

using namespace std;

namespace {
    vector<unsigned int> find_packed_fields(vector<unsigned int> &all_fields) {
        sort(all_fields.begin(), all_fields.end());

        int last_included = -1;
        for (int i = 0, max = all_fields.size(); i < max; ++i) {
            if (all_fields[i] < 70) {
                last_included = i;
                continue;
            }
            // the load factor also counts the unused "0" entry
            int load_factor = (i + 1) * 100 / (all_fields[i] + 1);
            if (load_factor > 75) {
                last_included = i;
            }
        }

        all_fields.resize(last_included + 1);

        return all_fields;
    }
}

void gpd::FieldMapImpl::add(pTHX_ SV *name, unsigned int number, void *field) {
    PerlString key;

    key.fill(aTHX_ name);
    by_name[key] = field;
    by_number[number] = field;
}

void gpd::FieldMapImpl::optimize_lookup() {
    vector<unsigned int> all_ids;

    for (typename NumberMap::const_iterator it = by_number.begin(), en = by_number.end(); it != en; ++it)
        all_ids.push_back(it->first);

    vector<unsigned int> packed_ids = find_packed_fields(all_ids);

    if (packed_ids.size() > 0) {
        top_packed_field = packed_ids.back();
        packed_by_number.resize(top_packed_field + 1);

        for (std::vector<unsigned int>::const_iterator it = packed_ids.begin(), en = packed_ids.end(); it != en; ++it) {
            packed_by_number[*it] = by_number[*it];
            by_number.erase(*it);
        }
    }
}

void *gpd::FieldMapImpl::find_by_name(pTHX_ SV *name) const {
    PerlString key;

    key.fill(aTHX_ name);
    return find_by_name(key);
}

void *gpd::FieldMapImpl::find_by_name(pTHX_ HE *he) const {
    PerlString key;

    key.fill(aTHX_ he);
    return find_by_name(key);
}

void *gpd::FieldMapImpl::find_by_name(pTHX_ const char *name, STRLEN namelen) const {
    PerlString key;

    key.fill(aTHX_ name, namelen);
    return find_by_name(key);
}

#ifndef _GPD_XS_MAPPER_CONTEXT_INCLUDED
#define _GPD_XS_MAPPER_CONTEXT_INCLUDED

#include "perl_unpollute.h"

#include "EXTERN.h"
#include "perl.h"
#include "ppport.h"

#include <vector>
#include <list>

namespace gpd {

class MapperContext {
public:
    enum Kind {
        Array       = 1,
        Hash        = 2,
        Message     = 3,
    };

    struct ArrayItem {
        AV *array;
        unsigned int index;
    };

    struct HashItem {
        HV *hash;
        SV *svkey;
        const char *keybuf;
        STRLEN keylen;
    };

    struct ExternalItem {
        int kind;
        unsigned int id;
        union {
            ArrayItem array_item;
            HashItem hash_item;
        };

        ExternalItem(Kind _kind, unsigned int _id) :
                kind(_kind), id(_id) {
        }
    };

    struct Item : public ExternalItem {
        unsigned int *nextId;

        Item(Kind _kind, unsigned int _id, unsigned int *_nextId) :
                ExternalItem(_kind, _id), nextId(_nextId)  {
        }

        void set_hash_key(SV *key) {
            hash_item.svkey = key;
            id = (*nextId)++;
        }

        void set_hash_key(const char *buffer, STRLEN len) {
            hash_item.keybuf = buffer;
            hash_item.keylen = len;
            id = (*nextId)++;
        }

        void set_hash_key(HE *entry) {
            if (HeKLEN(entry) == HEf_SVKEY) {
                set_hash_key(HeKEY_sv(entry));
            } else {
                set_hash_key(HeKEY(entry), HeKLEN(entry));
            }
        }

        void set_array_index(int index) {
            array_item.index = index;
            id = (*nextId)++;
        }
    };

    void clear();

    Item &push_level(AV *array) {
        Item &level = push_level(Array);

        level.array_item.array = array;

        return level;
    }

    Item &push_level(HV *hash, Kind kind) {
        return push_hash_level(hash, kind);
    }

    Item &push_level(SV *transformed, Kind kind) {
        return push_hash_level(NULL, kind);
    }

    void pop_level() { next_level--; }

    void fill_context(const ExternalItem * const **items, int *size);

private:
    Item &push_level(Kind kind);

    Item &push_hash_level(HV *hash, Kind kind) {
        Item &level = push_level(kind);

        level.hash_item.hash = hash;
        level.hash_item.svkey = NULL;
        level.hash_item.keybuf = NULL;

        return level;
    }

    typedef std::list<Item> LevelStorage;

    unsigned int nextId;
    LevelStorage level_storage;
    LevelStorage::iterator next_level;
    std::vector<ExternalItem*> levels;
};

}

#endif

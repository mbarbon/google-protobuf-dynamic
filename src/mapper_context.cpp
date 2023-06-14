#include "mapper_context.h"

using namespace gpd;
using namespace std;

void MapperContext::clear() {
    nextId = 1;
    next_level = level_storage.begin();
    levels.clear();
}

MapperContext::Item &MapperContext::push_level(Kind kind) {
    // implemented this way to reuse memory as much as possible
    if (next_level == level_storage.end()) {
        level_storage.push_back(Item(kind, nextId++, &nextId));
        return level_storage.back();
    }

    next_level->kind = kind;
    next_level->id = nextId++;

    return *(next_level++);
}

void MapperContext::fill_context(const ExternalItem * const **items, int *size) {
    levels.clear();

    for (LevelStorage::iterator it = level_storage.begin(); it != next_level; ++it) {
        levels.push_back(&*it);
    }

    *items = &levels[0];
    *size = levels.size();
}

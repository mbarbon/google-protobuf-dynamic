#ifndef _GPD_XS_REF_INCLUDED
#define _GPD_XS_REF_INCLUDED

#include <stdio.h>

namespace gpd {

class Refcounted {
public:
    void ref();
    void unref();

protected:
    Refcounted();
    virtual ~Refcounted();

private:
    int count;
};

inline Refcounted::Refcounted() : count(1) { }
inline Refcounted::~Refcounted() { }

inline void Refcounted::ref() {
    ++count;
}

inline void Refcounted::unref() {
    if (!--count)
        delete this;
}

}

#endif

#ifndef _GPD_XS_REF_INCLUDED
#define _GPD_XS_REF_INCLUDED

namespace gpd {

class Refcounted {
public:
    void ref() const;
    void unref() const;

protected:
    Refcounted();
    virtual ~Refcounted();

private:
    int count;
};

inline Refcounted::Refcounted() : count(1) { }
inline Refcounted::~Refcounted() { }

inline void Refcounted::ref() const {
    Refcounted *self = const_cast<Refcounted *>(this);

    ++self->count;
}

inline void Refcounted::unref() const {
    Refcounted *self = const_cast<Refcounted *>(this);

    if (!--self->count)
        delete self;
}

}

#endif

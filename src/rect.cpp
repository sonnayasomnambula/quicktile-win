#include <string>

#include "rect.h"

Rect::Rect()
{
    memset(this, 0, sizeof(*this));
}

Rect::Rect(const RECT &r)
{
    memcpy(this, &r, sizeof(*this));
}

Rect Rect::intersection(const Rect &other) const
{
    Rect r;
    r.left = std::max(left, other.left);
    r.right = std::min(right, other.right);
    r.top = std::max(top, other.top);
    r.bottom = std::min(bottom, other.bottom);

    if (!r.valid())
        memset(static_cast<void*>(&r), 0, sizeof(r));

    return r;
}

bool operator ==(const RECT& L, const RECT& R)
{
    return memcmp(&L, &R, sizeof(RECT)) == 0;
}

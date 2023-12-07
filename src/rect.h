#ifndef RECT_H
#define RECT_H

#include <windows.h>

struct Rect : RECT
{
    Rect();
    Rect(const RECT& r);

    inline LONG width() const { return right - left; }
    inline LONG height() const { return bottom - top; }
    inline LONG area() const { return width() * height(); }
    inline POINT center() const { return { left + width() / 2, top + height() / 2 }; }

    inline bool valid() const { return right > left && bottom > top; }

    inline bool contains(const POINT& pt) const {
        return pt.x >= left && pt.x < right && pt.y >= top && pt.y < bottom;
    }

    Rect intersection(const Rect& other) const;
};

bool operator ==(const RECT& L, const RECT& R);

#endif // RECT_H

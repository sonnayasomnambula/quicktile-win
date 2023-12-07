#ifndef DISPLAY_H
#define DISPLAY_H

#include <map>
#include <string>
#include <vector>

#include "rect.h"

struct Display : Rect
{
    static constexpr size_t INVALID_ID = ~0;

    static std::vector<Display> all;
    static std::wstring lastError;

    static bool init();

    static size_t indexOf(const Rect& rect);

    std::map<UINT, std::vector<RECT>> zones;

    Display(const RECT& r);

private:
    static BOOL addMonitor(HMONITOR monitor, HDC context, LPRECT rect, LPARAM data);
};


#endif // DISPLAY_H

#include "display.h"

std::vector<Display> Display::all;
std::wstring Display::lastError;

bool Display::init()
{
    all.clear();
    if (!EnumDisplayMonitors(NULL, NULL, addMonitor, 0)) {
        lastError = L"Display geometry initiaization failed: " + lastError;
        return false;
    }

    return true;
}

size_t Display::indexOf(const Rect &rect)
{
    size_t index = 0;
    LONG max_intersection = 0;
    POINT center = rect.center();
    for (size_t i = 0; i < all.size(); ++i) {
        if (all[i].contains(center))
            return i;

        LONG intersection = all[i].intersection(rect).area();
        if (intersection > max_intersection) {
            max_intersection = intersection;
            index = i;
        }
    }

    return index;
}

Display::Display(const RECT &r)
{
    left = r.left;
    right = r.right;
    top = r.top;
    bottom = r.bottom;

    LONG left13 = left + width() / 3;
    LONG left12 = left + width() / 2;
    LONG left23 = left + 2 * width() / 3;
    LONG top12 = top + height() / 2;

    zones[VK_NUMPAD7].push_back({ left, top, left23, top12 });
    zones[VK_NUMPAD7].push_back({ left, top, left12, top12 });
    zones[VK_NUMPAD7].push_back({ left, top, left13, top12 });

    zones[VK_NUMPAD4].push_back({ left, top, left23, bottom });
    zones[VK_NUMPAD4].push_back({ left, top, left12, bottom });
    zones[VK_NUMPAD4].push_back({ left, top, left13, bottom });

    zones[VK_NUMPAD1].push_back({ left, top12, left23, bottom });
    zones[VK_NUMPAD1].push_back({ left, top12, left12, bottom });
    zones[VK_NUMPAD1].push_back({ left, top12, left13, bottom });

    zones[VK_NUMPAD9].push_back({ left13, top, right, top12 });
    zones[VK_NUMPAD9].push_back({ left12, top, right, top12 });
    zones[VK_NUMPAD9].push_back({ left23, top, right, top12 });

    zones[VK_NUMPAD6].push_back({ left13, top, right, bottom });
    zones[VK_NUMPAD6].push_back({ left12, top, right, bottom });
    zones[VK_NUMPAD6].push_back({ left23, top, right, bottom });

    zones[VK_NUMPAD3].push_back({ left13, top12, right, bottom });
    zones[VK_NUMPAD3].push_back({ left12, top12, right, bottom });
    zones[VK_NUMPAD3].push_back({ left23, top12, right, bottom });

    zones[VK_NUMPAD8].push_back({ left, top, right, top12 });
    zones[VK_NUMPAD8].push_back({ left13, top, left23, top12 });

    zones[VK_NUMPAD5].push_back({ left, top, right, bottom });
    zones[VK_NUMPAD5].push_back({ left13, top, left23, bottom });

    zones[VK_NUMPAD2].push_back({ left, top12, right, bottom });
    zones[VK_NUMPAD2].push_back({ left13, top12, left23, bottom });
}

WINBOOL Display::addMonitor(HMONITOR monitor, HDC /*context*/, LPRECT /*rect*/, LPARAM /*data*/)
{
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    WINBOOL succ = GetMonitorInfoW(monitor, &mi);
    Rect rect(mi.rcWork);
    printf("Display %llu: %ld:%ld %ldx%ld\n", all.size(), rect.left, rect.top, rect.width(), rect.height()); fflush(stdout);
    if (succ)
        all.push_back(mi.rcWork);
    return succ;
}

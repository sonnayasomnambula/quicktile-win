#ifndef UNICODE
#define UNICODE
#endif

#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <memory>

#include <windows.h>
#include <winuser.h>
#include <commctrl.h>

#include "resource.h"

const wchar_t WINDOW_TITLE[]  = L"Quicktile";
const wchar_t CLASS_NAME[]    = L"quicktile-win";

const UINT WMAPP_NOTIFYCALLBACK = WM_APP + 1;

struct
{
    HWND window = NULL;
    UINT hotkey = 0;
    size_t pos = 0;
} prev;

struct Rect : RECT
{
    Rect() { memset(this, 0, sizeof(*this)); }
    Rect(const RECT& r) { memcpy(this, &r, sizeof(*this)); }

    inline LONG width() const { return right - left; }
    inline LONG height() const { return bottom - top; }
    inline LONG area() const { return width() * height(); }
    inline POINT center() const { return { left + width() / 2, top + height() / 2 }; }

    inline bool valid() const { return right > left && bottom > top; }

    inline bool contains(const POINT& pt) const {
        return pt.x >= left && pt.x < right && pt.y >= top && pt.y < bottom;
    }

    Rect intersection(const Rect& other) const {
        Rect r;
        r.left = std::max(left, other.left);
        r.right = std::min(right, other.right);
        r.top = std::max(top, other.top);
        r.bottom = std::min(bottom, other.bottom);

        if (!r.valid())
            memset(static_cast<void*>(&r), 0, sizeof(r));

        return r;
    }
};

static_assert(sizeof(Rect) == sizeof(RECT), "don't use memset & memcpy in Rect code");

bool operator ==(const RECT& L, const RECT& R)
{
    return memcmp(&L, &R, sizeof(RECT)) == 0;
}

class NotifyIcon
{
    NOTIFYICONDATAW nid = {};
public:
    NotifyIcon(HINSTANCE hInstance, HWND hwnd) {
        nid.cbSize              = sizeof(nid);
        nid.hWnd                = hwnd;
        nid.uFlags              = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        nid.uCallbackMessage    = WMAPP_NOTIFYCALLBACK;
        LoadIconMetric(hInstance, MAKEINTRESOURCE(IDI_ICON), LIM_SMALL, &nid.hIcon);
        wcsncpy(nid.szTip, WINDOW_TITLE, sizeof(nid.szTip) / sizeof(wchar_t));
        Shell_NotifyIcon(NIM_ADD, &nid);

        nid.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIcon(NIM_SETVERSION, &nid);
    }

    ~NotifyIcon() {
        Shell_NotifyIcon(NIM_DELETE, &nid);
    }
};

class Hotkeys
{
    HWND hwnd = NULL;
    std::vector<UINT> registered, not_registered;
    wchar_t lastError[MAX_PATH] = L"Unable to register hotkeys:\n";

public:
    inline static char toChar(UINT key) { return key - 48; }

    Hotkeys(HWND hwnd) : hwnd(hwnd) {
        for (UINT key = VK_NUMPAD0; key <= VK_NUMPAD9; ++key)
        {
            if (RegisterHotKey(hwnd, key, MOD_WIN, key))
                registered.push_back(key);
            else
                not_registered.push_back(key);
        }

        if (!not_registered.empty())
        {

            wchar_t name[MAX_PATH];
            BYTE keybstate[256] = {};
            GetKeyboardState(keybstate);
            for (UINT key: not_registered)
            {
                if (ToUnicode(key, MapVirtualKeyExW(key, MAPVK_VK_TO_VSC, NULL), keybstate, name, sizeof(name) / sizeof(wchar_t), 0))
                    wsprintfW(lastError + wcslen(lastError), L"Win+NUM%s ", name);
                else
                    wsprintfW(lastError + wcslen(lastError), L"0x%02X ", key);
            }
            MessageBoxW(hwnd, lastError, WINDOW_TITLE, MB_OK | MB_ICONERROR);
        }
    }

    ~Hotkeys() {
        for (UINT key: registered)
            UnregisterHotKey(hwnd, key);
    }

    bool empty() const { return registered.empty(); }
};

struct Display : Rect
{
    static constexpr size_t INVALID_ID = ~0;

    static std::vector<Display> all;
    static std::wstring lastError;

    static bool init(HWND hwnd) {
        all.clear();
        if (!EnumDisplayMonitors(NULL, NULL, addMonitor, 0)) {
            lastError = L"Display geometry initiaization failed: " + lastError;
            MessageBoxW(hwnd, lastError.c_str(), WINDOW_TITLE, MB_OK | MB_ICONERROR);
            return false;
        }

        return true;
    }

    static size_t indexOf(const Rect& rect) {
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

    std::map<UINT, std::vector<RECT>> positions;

    Display(const RECT& r) {
        left = r.left;
        right = r.right;
        top = r.top;
        bottom = r.bottom;

        LONG left13 = left + width() / 3;
        LONG left12 = left + width() / 2;
        LONG left23 = left + 2 * width() / 3;
        LONG top12 = top + height() / 2;

        positions[VK_NUMPAD7].push_back({ left, top, left13, top12 });
        positions[VK_NUMPAD7].push_back({ left, top, left12, top12 });
        positions[VK_NUMPAD7].push_back({ left, top, left23, top12 });

        positions[VK_NUMPAD4].push_back({ left, top, left13, bottom });
        positions[VK_NUMPAD4].push_back({ left, top, left12, bottom });
        positions[VK_NUMPAD4].push_back({ left, top, left23, bottom });

        positions[VK_NUMPAD1].push_back({ left, top12, left13, bottom });
        positions[VK_NUMPAD1].push_back({ left, top12, left12, bottom });
        positions[VK_NUMPAD1].push_back({ left, top12, left23, bottom });

        positions[VK_NUMPAD9].push_back({ left23, top, right, top12 });
        positions[VK_NUMPAD9].push_back({ left12, top, right, top12 });
        positions[VK_NUMPAD9].push_back({ left13, top, right, top12 });

        positions[VK_NUMPAD6].push_back({ left23, top, right, bottom });
        positions[VK_NUMPAD6].push_back({ left12, top, right, bottom });
        positions[VK_NUMPAD6].push_back({ left13, top, right, bottom });

        positions[VK_NUMPAD3].push_back({ left23, top12, right, bottom });
        positions[VK_NUMPAD3].push_back({ left12, top12, right, bottom });
        positions[VK_NUMPAD3].push_back({ left13, top12, right, bottom });

        positions[VK_NUMPAD8].push_back({ left13, top, left23, top12 });
        positions[VK_NUMPAD8].push_back({ left, top, right, top12 });

        positions[VK_NUMPAD5].push_back({ left13, top, left23, bottom });
        positions[VK_NUMPAD5].push_back({ left, top, right, bottom });

        positions[VK_NUMPAD2].push_back({ left13, top12, left23, bottom });
        positions[VK_NUMPAD2].push_back({ left, top12, right, bottom });
    }

    static Rect nextRect(HWND window, UINT hotkey, const Rect& rect) {
        size_t displayIndex = indexOf(rect);
        const Display& currentDisplay = all.at(displayIndex);

        printf("window is on display %llu\n", displayIndex); fflush(stdout);

        if (hotkey == VK_NUMPAD0)
        {
            // change display
            const Display& nextDisplay = all.at((displayIndex + 1) % all.size());

            for (const auto it: currentDisplay.positions) {
                for (size_t pos = 0; pos < it.second.size(); ++pos) {
                    if (rect == it.second.at(pos)) {
                        UINT key = it.first;
                        printf("window is on position %lld (NUM%c)\n", pos, Hotkeys::toChar(key)); fflush(stdout);
                        return nextDisplay.positions.at(key).at(pos);
                    }
                }
            }

            printf("window was not quicktiled yet\n"); fflush(stdout);

            LONG width  = std::min(rect.width(), nextDisplay.width());
            LONG height = std::min(rect.height(), nextDisplay.height());
            LONG dx = std::min(rect.left - currentDisplay.left, nextDisplay.width() - width);
            LONG dy = std::min(rect.top - currentDisplay.top, nextDisplay.height() - height);

            Rect r;
            r.left = nextDisplay.left + dx;
            r.right = r.left + width;
            r.top = nextDisplay.top + dy;
            r.bottom = r.top + height;
            return r;
        }

        bool same = prev.window == window && prev.hotkey == hotkey;
        prev.window = window;
        prev.hotkey = hotkey;

        const auto& rects = currentDisplay.positions.at(hotkey);

        if (same) {
            prev.pos = (prev.pos + 1) % rects.size();
            return rects.at(prev.pos);
        }

        for (size_t i = 0; i < rects.size(); ++i) {
            if (rects.at(i) == rect) {
                prev.pos = (i + 1) % rects.size();
                return rects.at(prev.pos);
            }
        }

        prev.pos = 0;
        return rects.at(prev.pos);
    }

private:
    static BOOL addMonitor(HMONITOR monitor, HDC /*context*/, LPRECT /*rect*/, LPARAM /*data*/) {
        MONITORINFO mi;
        mi.cbSize = sizeof(mi);
        WINBOOL succ = GetMonitorInfoW(monitor, &mi);
        Rect rect(mi.rcWork);
        printf("Display %llu: %ld:%ld %ldx%ld\n", all.size(), rect.left, rect.top, rect.width(), rect.height()); fflush(stdout);
        if (succ)
            all.push_back(mi.rcWork);
        return succ;
    }
};

std::vector<Display> Display::all;
std::wstring Display::lastError;

LRESULT CALLBACK DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

struct Window
{
    HINSTANCE instance = NULL;
    HWND hwnd = NULL;

    std::unique_ptr<NotifyIcon> notifyIcon;
    std::unique_ptr<Hotkeys> hotkeys;

    bool init() {
        if (!Display::init(hwnd))
            return false;

        SetTimer(hwnd, 0, 0, NULL);

        notifyIcon.reset(new NotifyIcon(instance, hwnd));
        hotkeys.reset(new Hotkeys(hwnd));
        return !hotkeys->empty();
    }

    void quit(const std::wstring& message)
    {
        MessageBoxW(hwnd, message.c_str(), WINDOW_TITLE, MB_OK | MB_ICONERROR);
        PostQuitMessage(EXIT_FAILURE);
    }

    LRESULT showContextMenu(POINT pt)
    {
        if (HMENU hMenu = LoadMenuW(instance, MAKEINTRESOURCEW(IDR_POPUP_MENU)))
        {
            if (HMENU hSubMenu = GetSubMenu(hMenu, 0))
            {
                UINT uFlags = TPM_RIGHTBUTTON | (GetSystemMetrics(SM_MENUDROPALIGNMENT) ? TPM_RIGHTALIGN : TPM_LEFTALIGN);
                TrackPopupMenuEx(hSubMenu, uFlags, pt.x, pt.y, hwnd, NULL);
            }
            DestroyMenu(hMenu);
        }
        return EXIT_SUCCESS;
    }

} window;

inline bool failedSilent(const char* message)
{
    printf(message);
    fflush(stdout);
    return false;
}

bool MoveCurrentWindow(UINT hotkey)
{
    HWND topWindow = GetForegroundWindow();
    if (!topWindow)
        return failedSilent("GetForegroundWindow failed\n");

    Rect rect;
    if (!GetWindowRect(topWindow, &rect))
        return failedSilent("GetWindowRect failed\n");

    printf("NUM%c\n", (Hotkeys::toChar(hotkey)) & 0xFF);

    printf("Current geometry: %ld:%ld %ldx%ld\n", rect.left, rect.top, rect.width(), rect.height()); fflush(stdout);

    rect = Display::nextRect(topWindow, hotkey, rect);

    printf("Next geometry: %ld:%ld %ldx%ld\n", rect.left, rect.top, rect.width(), rect.height()); fflush(stdout);

    MoveWindow(topWindow, rect.left, rect.top, rect.width(), rect.height(), TRUE);
    return true;
}

LRESULT CALLBACK DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
        window.hwnd = hwnd;
        if (!window.init())
            PostQuitMessage(EXIT_FAILURE);
        return TRUE;

    case WM_TIMER:
        KillTimer(hwnd, wParam);
        ShowWindow(hwnd, SW_HIDE);
        return TRUE;

    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return TRUE;

    case WM_DESTROY:
        PostQuitMessage(EXIT_SUCCESS);
        return TRUE;

    case WMAPP_NOTIFYCALLBACK:
        //printf("WMAPP_NOTIFYCALLBACK(0x%04LX, 0x%04LX)\n", wParam, lParam); fflush(stdout);
        return LOWORD(lParam) == WM_CONTEXTMENU ? window.showContextMenu({ LOWORD(wParam), HIWORD(wParam) }) : TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDM_OPTIONS:
            ShowWindow(hwnd, SW_NORMAL);
            return TRUE;
        case IDM_EXIT:
            PostQuitMessage(EXIT_SUCCESS);
            return TRUE;
        case IDOK:
            ShowWindow(hwnd, SW_HIDE);
            return TRUE;
        }

        printf("Unknown command %llu\n", wParam); fflush(stdout);
        return FALSE;

    case WM_HOTKEY:
        MoveCurrentWindow(HIWORD(lParam));
        return TRUE;

    case WM_DISPLAYCHANGE:
        if (!Display::init(hwnd))
            PostQuitMessage(EXIT_FAILURE);
        return TRUE;
    }

    return FALSE;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPWSTR /*lpCmdLine*/, int /*nShowCmd*/)
{
    window.instance = hInstance;
    InitCommonControls();
    return DialogBoxW(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, (DLGPROC)DialogProc);
}

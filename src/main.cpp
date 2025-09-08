#include <cstdio>
#include <memory>

#include <windows.h>
#include <commctrl.h>

#include "display.h"
#include "resource.h"

const wchar_t WINDOW_TITLE[]  = L"Quicktile";
const wchar_t CLASS_NAME[]    = L"quicktile-win";

const UINT WMAPP_NOTIFYCALLBACK = WM_APP + 1;

static_assert(sizeof(Rect) == sizeof(RECT), "don't use memset & memcpy in Rect code");

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

    inline UINT id(UINT key) const { return modifiers << 16 | key; }
    inline UINT key(UINT id) const { return id & 0xFF; }

public:
    inline static char toChar(UINT key) { return key - 48; }

    UINT modifiers = 0;

    Hotkeys(HWND hwnd, UINT modifiers = MOD_WIN) : hwnd(hwnd), modifiers(modifiers) {

        printf("register hotkeys with modifiers %d\n", modifiers); fflush(stdout);

        for (UINT key = VK_NUMPAD0; key <= VK_NUMPAD9; ++key)
        {
            UINT id = this->id(key);
            if (RegisterHotKey(hwnd, id, modifiers, key))
                registered.push_back(id);
            else
                not_registered.push_back(id);
        }

        if (!not_registered.empty())
        {
            wchar_t name[MAX_PATH];
            BYTE keybstate[256] = {};
            GetKeyboardState(keybstate);
            for (UINT id: not_registered)
            {
                UINT key = this->key(id);
                if (ToUnicode(key, MapVirtualKeyExW(key, MAPVK_VK_TO_VSC, NULL), keybstate, name, sizeof(name) / sizeof(wchar_t), 0))
                    wsprintfW(lastError + wcslen(lastError), L"NUM%s ", name);
                else
                    wsprintfW(lastError + wcslen(lastError), L"0x%02X ", key);
            }
            MessageBoxW(hwnd, lastError, WINDOW_TITLE, MB_OK | MB_ICONERROR);
        }
    }

    ~Hotkeys() {
        for (UINT id: registered)
            if (!UnregisterHotKey(hwnd, id))
                printf("Unable to unregister hotkey NUM%c: %s\n", toChar(key(id)), strerror(GetLastError()));
        fflush(stdout);
    }

    bool empty() const { return registered.empty(); }
};

class Config
{
public:
    DWORD modifiers = MOD_WIN;
    static constexpr wchar_t CONFIG_PATH[] = L"Software\\sonnayasomnambula\\quicktile-win";

    bool read() {
        HKEY hKey = NULL;
        LONG res = RegOpenKeyExW(HKEY_CURRENT_USER, CONFIG_PATH, 0, KEY_READ, &hKey);
        if (res != ERROR_SUCCESS)
            return false;
        DWORD size = sizeof(DWORD);
        res = RegQueryValueExW(hKey, L"modifiers", 0, NULL, (LPBYTE)&modifiers, &size);
        RegCloseKey(hKey);
        return res == ERROR_SUCCESS;
    }

    bool write() const {
        HKEY hKey = NULL;
        LONG res = RegOpenKeyEx(HKEY_CURRENT_USER, CONFIG_PATH, 0, KEY_SET_VALUE, &hKey);
        if (res == ERROR_FILE_NOT_FOUND)
            res = RegCreateKeyExW(HKEY_CURRENT_USER, CONFIG_PATH, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hKey, NULL);
        if (res == ERROR_SUCCESS)
            res = RegSetValueExW(hKey, L"modifiers", 0, REG_DWORD, (const BYTE*)&modifiers, sizeof(modifiers));
        RegCloseKey(hKey);
        return res == ERROR_SUCCESS;
    }

    Config() { read(); }
   ~Config() { write(); }
};

LRESULT CALLBACK DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

inline WINBOOL EnableDlgButton(HWND hwndDlg, int nIDDlgItem, bool enable = true) {
    return EnableWindow(GetDlgItem(hwndDlg, nIDDlgItem), enable);
}

struct Window
{
    HINSTANCE instance = NULL;
    HWND hwnd = NULL;

    std::unique_ptr<NotifyIcon> notifyIcon;
    std::unique_ptr<Hotkeys> hotkeys;

    bool init()
    {
        if (!Display::init())
        {
            MessageBoxW(hwnd, Display::lastError.c_str(), WINDOW_TITLE, MB_OK | MB_ICONERROR);
            return false;
        }

        HICON icon = NULL;
        LoadIconMetric(instance, MAKEINTRESOURCE(IDI_ICON), LIM_SMALL, &icon);
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)icon);

        Config config;

        CheckDlgButton(hwnd, CK_WIN, config.modifiers & MOD_WIN ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, CK_CTRL, config.modifiers & MOD_CONTROL ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, CK_SHIFT, config.modifiers & MOD_SHIFT ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, CK_ALT, config.modifiers & MOD_ALT ? BST_CHECKED : BST_UNCHECKED);
        EnableDlgButton(hwnd, BTN_APPLY, false);

        SetTimer(hwnd, 0, 0, NULL);

        notifyIcon.reset(new NotifyIcon(instance, hwnd));
        hotkeys.reset(new Hotkeys(hwnd, config.modifiers));
        return !hotkeys->empty();
    }

    void cleanup()
    {
        notifyIcon.reset();
        hotkeys.reset();
    }

    void showContextMenu(POINT pt)
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
    }

    UINT modifiers() const
    {
        return (IsDlgButtonChecked(hwnd, CK_WIN)   ? MOD_WIN     : 0) |
               (IsDlgButtonChecked(hwnd, CK_ALT)   ? MOD_ALT     : 0) |
               (IsDlgButtonChecked(hwnd, CK_SHIFT) ? MOD_SHIFT   : 0) |
               (IsDlgButtonChecked(hwnd, CK_CTRL)  ? MOD_CONTROL : 0);
    }

    bool applyModifiers(UINT modifiers)
    {
        UINT old = hotkeys ? hotkeys->modifiers : CK_WIN;
        hotkeys.reset(new Hotkeys(hwnd, modifiers));
        if (hotkeys->empty())
        {
            printf("restore modifiers to %d\n", old); fflush(stdout);
            hotkeys.reset(new Hotkeys(hwnd, old));
            return false;
        }

        Config conf;
        conf.modifiers = modifiers;
        return true;
    }

} window;

inline bool failedSilent(const char* message)
{
    printf(message);
    fflush(stdout);
    return false;
}

Rect nextRect(HWND window, UINT hotkey, const Rect &rect)
{
    struct
    {
        HWND window = NULL;
        UINT hotkey = 0;
        size_t pos = 0;
    } static previous;

    size_t displayIndex = Display::indexOf(rect);
    const Display& currentDisplay = Display::all.at(displayIndex);

    printf("window is on display %llu\n", displayIndex); fflush(stdout);

    if (hotkey == VK_NUMPAD0)
    {
        // change display
        const Display& nextDisplay = Display::all.at((displayIndex + 1) % Display::all.size());

        for (const auto it: currentDisplay.zones) {
            for (size_t pos = 0; pos < it.second.size(); ++pos) {
                if (rect == it.second.at(pos)) {
                    UINT key = it.first;
                    printf("window is on position %lld (NUM%c)\n", pos, Hotkeys::toChar(key)); fflush(stdout);
                    return nextDisplay.zones.at(key).at(pos);
                }
            }
        }

        printf("window was not quicktiled yet\n"); fflush(stdout);

        LONG width  = std::min(rect.width(), nextDisplay.width());
        LONG height = std::min(rect.height(), nextDisplay.height());
        LONG dx_l = std::min(std::max(0l, rect.left - currentDisplay.left), nextDisplay.width() - width);
        LONG dx_r = std::min(std::max(0l, currentDisplay.right - rect.right), nextDisplay.width() - width);
        LONG dy_t = std::min(std::max(0l, rect.top - currentDisplay.top), nextDisplay.height() - height);
        LONG dy_b = std::min(std::max(0l, currentDisplay.bottom - rect.bottom), nextDisplay.height() - height);

        Rect r;
        r.left = dx_l < dx_r ? nextDisplay.left + dx_l : nextDisplay.right - width - dx_r;
        r.right = r.left + width;
        r.top = dy_t < dy_b ? nextDisplay.top + dy_t : nextDisplay.bottom - height - dy_b;
        r.bottom = r.top + height;
        return r;
    }

    bool is_same = previous.window == window && previous.hotkey == hotkey;
    previous.window = window;
    previous.hotkey = hotkey;

    const auto& rects = currentDisplay.zones.at(hotkey);

    if (is_same) {
        // the window will not always be able to shrink to the desired size,
        // so it is wrong to calculate the index based on the current size only
        previous.pos = (previous.pos + 1) % rects.size();
        return rects.at(previous.pos);
    }

    for (size_t i = 0; i < rects.size(); ++i) {
        if (rects.at(i) == rect) {
            previous.pos = (i + 1) % rects.size();
            return rects.at(previous.pos);
        }
    }

    previous.pos = 0;
    return rects.at(previous.pos);
}

bool MoveCurrentWindow(UINT hotkey)
{
    HWND topWindow = GetForegroundWindow();
    if (!topWindow)
        return failedSilent("GetForegroundWindow failed\n");

    WINDOWPLACEMENT placement = {};
    placement.length = sizeof(placement);
    if (!GetWindowPlacement(topWindow, &placement))
        return failedSilent("GetWindowPlacement failed\n");

    printf("\nNUM%c\n", (Hotkeys::toChar(hotkey)) & 0xFF);

    Rect rect = placement.rcNormalPosition;
    printf("Current geometry: %ld:%ld %ldx%ld\n", rect.left, rect.top, rect.width(), rect.height()); fflush(stdout);

    rect = nextRect(topWindow, hotkey, rect);

    printf("Next geometry: %ld:%ld %ldx%ld\n", rect.left, rect.top, rect.width(), rect.height()); fflush(stdout);

    placement.rcNormalPosition = rect;
    placement.showCmd = SW_SHOWNORMAL;

    for (const Display& display: Display::all)
        if (rect == display)
            placement.showCmd = SW_MAXIMIZE;

    if (placement.showCmd == SW_MAXIMIZE && hotkey == VK_NUMPAD0)
    {
        // one does not simply move maximized window
        placement.showCmd = SW_SHOWNORMAL;
        SetWindowPlacement(topWindow, &placement);
        placement.showCmd = SW_MAXIMIZE;
        SetWindowPlacement(topWindow, &placement);
    }
    else
    {
        SetWindowPlacement(topWindow, &placement);
    }

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
        window.cleanup();
        PostQuitMessage(EXIT_SUCCESS);
        return TRUE;

    case WMAPP_NOTIFYCALLBACK:
        if (LOWORD(lParam) == WM_CONTEXTMENU) {
            POINT pt = { LOWORD(wParam), HIWORD(wParam) };
            window.showContextMenu(pt);
            return TRUE;
        } else if (LOWORD(lParam) == WM_LBUTTONDBLCLK) {
            ShowWindow(hwnd, SW_NORMAL);
            return TRUE;
        }
        return FALSE;

    case WM_COMMAND:
        if ((HWND)lParam)
        {
            const WORD controlID = LOWORD(wParam);
            switch (controlID)
            {
            case CK_WIN:
            case CK_ALT:
            case CK_SHIFT:
            case CK_CTRL:
                EnableDlgButton(hwnd, BTN_APPLY,
                                window.hotkeys && window.hotkeys->modifiers != window.modifiers());
                return TRUE;
            case BTN_APPLY:
                if (window.applyModifiers(window.modifiers()))
                    EnableDlgButton(hwnd, BTN_APPLY, false);
                return TRUE;
            default:
                printf("unknown control %d\n", LOWORD(wParam));
                fflush(stdout);
                return FALSE;
            }
        }
        else
        {
            // menu
            switch (LOWORD(wParam))
            {
            case MENUITEM_OPTIONS:
                ShowWindow(hwnd, SW_NORMAL);
                return TRUE;
            case MENUITEM_EXIT:
                DestroyWindow(window.hwnd);
                return TRUE;
            case IDCANCEL: // Esc
                SendMessageW(window.hwnd, WM_CLOSE, 0, 0);
                return TRUE;
            default:
                printf("unknown menu %d\n", LOWORD(wParam));
                fflush(stdout);
                return FALSE;
            }
        }

    case WM_HOTKEY:
        MoveCurrentWindow(HIWORD(lParam));
        return TRUE;

    case WM_SETTINGCHANGE:
    case WM_DISPLAYCHANGE:
        if (uMsg == WM_SETTINGCHANGE && wParam != SPI_SETWORKAREA)
            break;

        if (!Display::init())
        {
            MessageBoxW(hwnd, Display::lastError.c_str(), WINDOW_TITLE, MB_OK | MB_ICONERROR);
            PostQuitMessage(EXIT_FAILURE);
        }
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

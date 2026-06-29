#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>

#include "resource.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr wchar_t kMainWindowClass[] = L"MaxBHandyClipboard.MainWindow";
constexpr wchar_t kPopupWindowClass[] = L"MaxBHandyClipboard.PopupWindow";
constexpr wchar_t kSettingsWindowClass[] = L"MaxBHandyClipboard.SettingsWindow";
constexpr wchar_t kHotKeyInputClass[] = L"MaxBHandyClipboard.HotKeyInput";
constexpr wchar_t kAppName[] = L"MaxB Handy Clipboard";

constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kHotKeyId = 1;
constexpr UINT kTrayId = 1;
constexpr UINT kTimerCaptureRetry = 1;
constexpr UINT kSettingsClearHistory = 2002;
constexpr UINT kSettingsClose = 2003;
constexpr UINT kSettingsExit = 2004;
constexpr UINT kSettingsHotKey = 2005;
constexpr UINT kSettingsApplyHotKey = 2006;
constexpr UINT kSettingsStartup = 2007;
constexpr UINT kHistoryListId = 3001;

constexpr size_t kMaxHistoryItems = 20;
constexpr size_t kMaxStoredBytesPerItem = 4 * 1024 * 1024;
constexpr int kPopupWidth = 720;
constexpr int kPopupDefaultHeight = 378;
constexpr int kPopupHeaderHeight = 113;
constexpr int kPopupContentLeft = 40;
constexpr int kPopupListLeft = 40;
constexpr int kPopupListTop = 123;
constexpr int kPopupListBottomMargin = 35;
constexpr int kPopupScrollbarLaneWidth = 15;
constexpr int kPopupScrollbarWidth = 5;
constexpr int kPopupScrollbarMinHeight = 36;
constexpr int kHistoryItemHeight = 56;
constexpr int kHistoryDeleteButtonSize = 24;
constexpr int kHistoryDeleteButtonMargin = 15;
constexpr int kPopupCloseButtonSize = 36;
constexpr int kPopupCloseButtonMargin = 47;
constexpr int kWindowCornerRadius = 6;
constexpr int kPopupMinWidth = 520;
constexpr int kPopupMinHeight = 300;
constexpr int kPopupResizeBorder = 8;
constexpr DWORD kPopupUiVersion = 2;
constexpr int kSettingsClientWidth = 500;
constexpr int kSettingsClientHeight = 520;
constexpr int kSettingsResizeBorder = 8;
constexpr int kSettingsHeaderHeight = 104;
constexpr int kElementCornerRadius = 6;

constexpr COLORREF kColorWindow = RGB(246, 247, 251);
constexpr COLORREF kColorSurface = RGB(255, 255, 255);
constexpr COLORREF kColorSurfaceHover = RGB(246, 249, 255);
constexpr COLORREF kColorAccent = RGB(0, 95, 184);
constexpr COLORREF kColorAccentSoft = RGB(226, 239, 255);
constexpr COLORREF kColorText = RGB(30, 41, 59);
constexpr COLORREF kColorMutedText = RGB(100, 116, 139);
constexpr COLORREF kColorBorder = RGB(218, 226, 235);

constexpr DWORD kDwmWindowCornerPreference = 33;
constexpr DWORD kDwmBorderColor = 34;
constexpr DWORD kDwmCaptionColor = 35;
constexpr DWORD kDwmTextColor = 36;
constexpr int kDwmRoundCorners = 2;
constexpr COLORREF kDwmColorNone = 0xFFFFFFFE;
constexpr wchar_t kRegistryPath[] = L"Software\\MaxBHandyClipboard";
constexpr wchar_t kPopupXValue[] = L"HistoryWindowX";
constexpr wchar_t kPopupYValue[] = L"HistoryWindowY";
constexpr wchar_t kPopupWidthValue[] = L"HistoryWindowWidth";
constexpr wchar_t kPopupHeightValue[] = L"HistoryWindowHeight";
constexpr wchar_t kPopupUiVersionValue[] = L"HistoryWindowUiVersion";
constexpr wchar_t kRunRegistryPath[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValueName[] = L"MaxB Handy Clipboard";

struct FormatData {
    UINT format = 0;
    std::vector<BYTE> bytes;
};

struct ClipboardItem {
    std::vector<FormatData> formats;
    std::wstring preview;
    std::uint64_t hash = 0;
    size_t totalBytes = 0;
};

struct HotKeyBinding {
    bool ctrl = false;
    bool alt = false;
    bool shift = false;
    bool win = false;
    UINT vk = 0;
};

std::deque<ClipboardItem> g_history;
HWND g_mainWindow = nullptr;
HWND g_popupWindow = nullptr;
HWND g_settingsWindow = nullptr;
HWND g_listBox = nullptr;
HWND g_settingsTitleLabel = nullptr;
HWND g_hotKeyLabel = nullptr;
HWND g_startupCheckBox = nullptr;
HWND g_themeLabel = nullptr;
HWND g_languageLabel = nullptr;
HWND g_applyHotKeyButton = nullptr;
HWND g_exitButton = nullptr;
HWND g_hotKeyControl = nullptr;
HWND g_hotKeyStatusLabel = nullptr;
HWND g_targetWindow = nullptr;
WNDPROC g_originalListProc = nullptr;
HFONT g_titleFont = nullptr;
HFONT g_popupTitleFont = nullptr;
HFONT g_bodyFont = nullptr;
HFONT g_smallFont = nullptr;
HBRUSH g_windowBrush = nullptr;
HBRUSH g_surfaceBrush = nullptr;
HBRUSH g_editBrush = nullptr;
bool g_suppressNextClipboardUpdate = false;
bool g_startWithWindows = false;
bool g_darkThemeSelected = false;
bool g_englishLanguageSelected = true;
bool g_hotKeyRegistered = false;
HotKeyBinding g_hotKeyBinding{true, false, true, false, L'V'};
HotKeyBinding g_pendingHotKeyBinding = g_hotKeyBinding;
HHOOK g_keyboardHook = nullptr;
HHOOK g_hotKeyCaptureHook = nullptr;
bool g_hookCtrlDown = false;
bool g_hookAltDown = false;
bool g_hookShiftDown = false;
bool g_hookWinDown = false;
bool g_modifierHotKeyActive = false;
bool g_capturingHotKey = false;
bool g_captureCtrlDown = false;
bool g_captureAltDown = false;
bool g_captureShiftDown = false;
bool g_captureWinDown = false;
HANDLE g_instanceMutex = nullptr;
RECT g_popupSavedRect{};
bool g_popupRectLoaded = false;
bool g_popupHasSavedRect = false;
bool g_popupScrollbarDragging = false;
int g_popupScrollbarDragOffsetY = 0;
int g_hoveredHistoryIndex = -1;

const wchar_t* UiText(const wchar_t* english, const wchar_t* russian) {
    return g_englishLanguageSelected ? english : russian;
}

void UpdateSettingsWindow();
void PopulateListBox();
void RefreshVisiblePopupHistory();
void PositionPopup();
void SavePopupPlacement();
RECT PopupCloseButtonRect(RECT clientRect);
void DrawPopupScrollbar(HWND hwnd, HDC dc);
void DrawXGlyph(HDC dc, const RECT& rect, COLORREF color, int inset);

COLORREF ThemeWindowColor() {
    return g_darkThemeSelected ? RGB(20, 24, 31) : kColorWindow;
}

COLORREF ThemeSurfaceColor() {
    return g_darkThemeSelected ? RGB(30, 36, 46) : kColorSurface;
}

COLORREF ThemeSurfaceHoverColor() {
    return g_darkThemeSelected ? RGB(38, 46, 60) : kColorSurfaceHover;
}

COLORREF ThemeInputColor() {
    return g_darkThemeSelected ? RGB(24, 30, 39) : RGB(248, 250, 252);
}

COLORREF ThemeAccentColor() {
    return g_darkThemeSelected ? RGB(65, 156, 255) : kColorAccent;
}

COLORREF ThemeAccentSoftColor() {
    return g_darkThemeSelected ? RGB(28, 58, 92) : kColorAccentSoft;
}

COLORREF ThemeTextColor() {
    return g_darkThemeSelected ? RGB(226, 232, 240) : kColorText;
}

COLORREF ThemeMutedTextColor() {
    return g_darkThemeSelected ? RGB(148, 163, 184) : kColorMutedText;
}

COLORREF ThemeBorderColor() {
    return g_darkThemeSelected ? RGB(61, 72, 88) : kColorBorder;
}

COLORREF BlendColor(COLORREF foreground, COLORREF background, int alpha) {
    alpha = std::clamp(alpha, 0, 255);
    const int inverseAlpha = 255 - alpha;
    return RGB((GetRValue(foreground) * alpha +
                GetRValue(background) * inverseAlpha) /
                   255,
               (GetGValue(foreground) * alpha +
                GetGValue(background) * inverseAlpha) /
                   255,
               (GetBValue(foreground) * alpha +
                GetBValue(background) * inverseAlpha) /
                   255);
}

COLORREF ThemeHistoryDividerColor() {
    return BlendColor(ThemeBorderColor(), ThemeSurfaceColor(), 120);
}

COLORREF ThemeSelectorFillColor() {
    return g_darkThemeSelected ? RGB(41, 49, 63) : RGB(238, 244, 251);
}

COLORREF ThemeSelectedBorderColor() {
    return g_darkThemeSelected ? RGB(82, 151, 231) : RGB(147, 197, 253);
}

void ResetThemeBrushes() {
    if (g_windowBrush) {
        DeleteObject(g_windowBrush);
        g_windowBrush = nullptr;
    }
    if (g_surfaceBrush) {
        DeleteObject(g_surfaceBrush);
        g_surfaceBrush = nullptr;
    }
    if (g_editBrush) {
        DeleteObject(g_editBrush);
        g_editBrush = nullptr;
    }
}

HFONT CreateUiFont(int pointSize, int weight) {
    HDC screen = GetDC(nullptr);
    const int height = -MulDiv(pointSize, GetDeviceCaps(screen, LOGPIXELSY), 72);
    ReleaseDC(nullptr, screen);

    return CreateFontW(height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

void EnsureUiResources() {
    if (!g_titleFont) {
        g_titleFont = CreateUiFont(14, FW_SEMIBOLD);
    }
    if (!g_popupTitleFont) {
        g_popupTitleFont = CreateUiFont(16, FW_SEMIBOLD);
    }
    if (!g_bodyFont) {
        g_bodyFont = CreateUiFont(10, FW_NORMAL);
    }
    if (!g_smallFont) {
        g_smallFont = CreateUiFont(9, FW_NORMAL);
    }
    if (!g_windowBrush) {
        g_windowBrush = CreateSolidBrush(ThemeWindowColor());
    }
    if (!g_surfaceBrush) {
        g_surfaceBrush = CreateSolidBrush(ThemeSurfaceColor());
    }
    if (!g_editBrush) {
        g_editBrush = CreateSolidBrush(ThemeInputColor());
    }
}

void CleanupUiResources() {
    if (g_titleFont) {
        DeleteObject(g_titleFont);
        g_titleFont = nullptr;
    }
    if (g_popupTitleFont) {
        DeleteObject(g_popupTitleFont);
        g_popupTitleFont = nullptr;
    }
    if (g_bodyFont) {
        DeleteObject(g_bodyFont);
        g_bodyFont = nullptr;
    }
    if (g_smallFont) {
        DeleteObject(g_smallFont);
        g_smallFont = nullptr;
    }
    if (g_windowBrush) {
        DeleteObject(g_windowBrush);
        g_windowBrush = nullptr;
    }
    if (g_surfaceBrush) {
        DeleteObject(g_surfaceBrush);
        g_surfaceBrush = nullptr;
    }
    if (g_editBrush) {
        DeleteObject(g_editBrush);
        g_editBrush = nullptr;
    }
}

void FillSolidRect(HDC dc, const RECT& rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

void DrawRoundedRect(HDC dc, const RECT& rect, int radius, COLORREF fill,
                     COLORREF border) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, pen);

    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom,
              radius * 2, radius * 2);

    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void DrawRoundedOutline(HDC dc, const RECT& rect, int radius, COLORREF border) {
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    HGDIOBJ oldPen = SelectObject(dc, pen);

    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom,
              radius * 2, radius * 2);

    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
}

void DrawTextInRect(HDC dc, std::wstring_view text, RECT rect, HFONT font,
                    COLORREF color, UINT format) {
    HGDIOBJ oldFont = SelectObject(dc, font);
    const int oldMode = SetBkMode(dc, TRANSPARENT);
    const COLORREF oldColor = SetTextColor(dc, color);

    DrawTextW(dc, text.data(), static_cast<int>(text.size()), &rect, format);

    SetTextColor(dc, oldColor);
    SetBkMode(dc, oldMode);
    SelectObject(dc, oldFont);
}

HICON LoadAppIcon(int width, int height) {
    HICON icon = reinterpret_cast<HICON>(LoadImageW(
        GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
        width, height, LR_DEFAULTCOLOR | LR_SHARED));
    if (!icon) {
        icon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    return icon;
}

void ApplyWindowVisualStyle(HWND hwnd, bool smallCorners) {
    const int cornerPreference = kDwmRoundCorners;
    DwmSetWindowAttribute(hwnd, kDwmWindowCornerPreference, &cornerPreference,
                          sizeof(cornerPreference));

    const COLORREF borderColor = smallCorners ? kDwmColorNone : ThemeBorderColor();
    DwmSetWindowAttribute(hwnd, kDwmBorderColor, &borderColor,
                          sizeof(borderColor));

    if (!smallCorners) {
        const COLORREF captionColor =
            g_darkThemeSelected ? RGB(20, 24, 31) : RGB(249, 250, 252);
        const COLORREF textColor = ThemeTextColor();
        DwmSetWindowAttribute(hwnd, kDwmCaptionColor, &captionColor,
                              sizeof(captionColor));
        DwmSetWindowAttribute(hwnd, kDwmTextColor, &textColor,
                              sizeof(textColor));
    }
}

void ApplyRoundedRegion(HWND hwnd, int radius) {
    RECT windowRect{};
    GetWindowRect(hwnd, &windowRect);
    const int width = windowRect.right - windowRect.left;
    const int height = windowRect.bottom - windowRect.top;
    if (width <= 0 || height <= 0) {
        return;
    }

    HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1,
                                     radius * 2, radius * 2);
    if (region && !SetWindowRgn(hwnd, region, TRUE)) {
        DeleteObject(region);
    }
}

int RectWidth(const RECT& rect) {
    return rect.right - rect.left;
}

int RectHeight(const RECT& rect) {
    return rect.bottom - rect.top;
}

bool ReadRegistryDword(HKEY key, const wchar_t* name, DWORD& value) {
    DWORD type = 0;
    DWORD size = sizeof(value);
    return RegQueryValueExW(key, name, nullptr, &type,
                            reinterpret_cast<BYTE*>(&value), &size) ==
               ERROR_SUCCESS &&
           type == REG_DWORD && size == sizeof(value);
}

std::wstring AppExecutablePath() {
    std::wstring path(MAX_PATH, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, path.data(),
                                      static_cast<DWORD>(path.size()));
    while (length == path.size()) {
        path.resize(path.size() * 2, L'\0');
        length = GetModuleFileNameW(nullptr, path.data(),
                                    static_cast<DWORD>(path.size()));
    }

    path.resize(length);
    return path;
}

bool IsStartupEnabled() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunRegistryPath, 0, KEY_QUERY_VALUE,
                      &key) != ERROR_SUCCESS) {
        return false;
    }

    DWORD type = 0;
    DWORD size = 0;
    const LONG result = RegQueryValueExW(key, kRunValueName, nullptr, &type,
                                         nullptr, &size);
    RegCloseKey(key);
    return result == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ) &&
           size > sizeof(wchar_t);
}

bool SetStartupEnabled(bool enabled) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunRegistryPath, 0, nullptr, 0,
                        KEY_SET_VALUE, nullptr, &key, nullptr) !=
        ERROR_SUCCESS) {
        return false;
    }

    LONG result = ERROR_SUCCESS;
    if (enabled) {
        const std::wstring path = AppExecutablePath();
        const std::wstring runValue = L"\"" + path + L"\"";
        result = RegSetValueExW(
            key, kRunValueName, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(runValue.c_str()),
            static_cast<DWORD>((runValue.size() + 1) * sizeof(wchar_t)));
    } else {
        result = RegDeleteValueW(key, kRunValueName);
        if (result == ERROR_FILE_NOT_FOUND) {
            result = ERROR_SUCCESS;
        }
    }

    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

bool LoadPopupPlacement(RECT& rect) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegistryPath, 0, KEY_QUERY_VALUE,
                      &key) != ERROR_SUCCESS) {
        return false;
    }

    DWORD x = 0;
    DWORD y = 0;
    DWORD width = 0;
    DWORD height = 0;
    DWORD version = 0;
    const bool loaded = ReadRegistryDword(key, kPopupXValue, x) &&
                        ReadRegistryDword(key, kPopupYValue, y) &&
                        ReadRegistryDword(key, kPopupWidthValue, width) &&
                        ReadRegistryDword(key, kPopupHeightValue, height) &&
                        ReadRegistryDword(key, kPopupUiVersionValue, version) &&
                        version == kPopupUiVersion;
    RegCloseKey(key);

    if (!loaded || width == 0 || height == 0) {
        return false;
    }

    rect.left = static_cast<LONG>(x);
    rect.top = static_cast<LONG>(y);
    rect.right = rect.left + static_cast<LONG>(width);
    rect.bottom = rect.top + static_cast<LONG>(height);
    return true;
}

void WriteRegistryDword(HKEY key, const wchar_t* name, int value) {
    const DWORD stored = static_cast<DWORD>(value);
    RegSetValueExW(key, name, 0, REG_DWORD,
                   reinterpret_cast<const BYTE*>(&stored), sizeof(stored));
}

RECT WorkAreaForMonitor(HMONITOR monitor) {
    MONITORINFO monitorInfo{sizeof(MONITORINFO)};
    if (monitor && GetMonitorInfoW(monitor, &monitorInfo)) {
        return monitorInfo.rcWork;
    }

    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    return work;
}

RECT WorkAreaForPopupStart() {
    HMONITOR monitor = nullptr;
    if (g_targetWindow && IsWindow(g_targetWindow)) {
        monitor = MonitorFromWindow(g_targetWindow, MONITOR_DEFAULTTONEAREST);
    }
    if (!monitor) {
        POINT cursor{};
        GetCursorPos(&cursor);
        monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    }
    return WorkAreaForMonitor(monitor);
}

RECT DefaultPopupRect() {
    const RECT work = WorkAreaForPopupStart();
    const int workWidth = RectWidth(work);
    const int workHeight = RectHeight(work);
    const int width = std::min(kPopupWidth, std::max(kPopupMinWidth, workWidth - 32));
    const int height = std::min(kPopupDefaultHeight,
                                std::max(kPopupMinHeight, workHeight - 32));
    const int x = work.left + (workWidth - width) / 2;
    const int y = work.top + (workHeight - height) / 2;
    return RECT{x, y, x + width, y + height};
}

RECT NormalizePopupRect(RECT rect) {
    HMONITOR monitor = MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);
    const RECT work = WorkAreaForMonitor(monitor);
    const int workWidth = RectWidth(work);
    const int workHeight = RectHeight(work);
    const int maxWidth = std::max(kPopupMinWidth, workWidth);
    const int maxHeight = std::max(kPopupMinHeight, workHeight);
    const int width = std::clamp(RectWidth(rect), kPopupMinWidth, maxWidth);
    const int height = std::clamp(RectHeight(rect), kPopupMinHeight, maxHeight);
    const int maxLeft = work.right - width;
    const int maxTop = work.bottom - height;
    const int left = static_cast<int>(rect.left);
    const int top = static_cast<int>(rect.top);
    const int x = maxLeft >= work.left
                      ? std::clamp(left, static_cast<int>(work.left), maxLeft)
                      : work.left + (workWidth - width) / 2;
    const int y = maxTop >= work.top
                      ? std::clamp(top, static_cast<int>(work.top), maxTop)
                      : work.top + (workHeight - height) / 2;

    return RECT{x, y, x + width, y + height};
}

void SavePopupPlacement() {
    if (!g_popupWindow || !IsWindow(g_popupWindow)) {
        return;
    }

    RECT rect{};
    if (!GetWindowRect(g_popupWindow, &rect) ||
        RectWidth(rect) <= 0 || RectHeight(rect) <= 0) {
        return;
    }

    g_popupSavedRect = rect;
    g_popupHasSavedRect = true;
    g_popupRectLoaded = true;

    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegistryPath, 0, nullptr, 0,
                        KEY_SET_VALUE, nullptr, &key, nullptr) !=
        ERROR_SUCCESS) {
        return;
    }

    WriteRegistryDword(key, kPopupXValue, rect.left);
    WriteRegistryDword(key, kPopupYValue, rect.top);
    WriteRegistryDword(key, kPopupWidthValue, RectWidth(rect));
    WriteRegistryDword(key, kPopupHeightValue, RectHeight(rect));
    WriteRegistryDword(key, kPopupUiVersionValue,
                       static_cast<int>(kPopupUiVersion));
    RegCloseKey(key);
}

std::uint64_t HashBytes(std::uint64_t hash, const void* data, size_t size) {
    constexpr std::uint64_t kPrime = 1099511628211ull;
    const auto* bytes = static_cast<const BYTE*>(data);
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= kPrime;
    }
    return hash;
}

void HashValue(std::uint64_t& hash, const void* data, size_t size) {
    hash = HashBytes(hash, data, size);
}

std::wstring CollapseWhitespace(std::wstring text) {
    std::wstring result;
    result.reserve(text.size());

    bool pendingSpace = false;
    for (wchar_t ch : text) {
        if (ch == L'\r' || ch == L'\n' || ch == L'\t' || ch == L' ') {
            pendingSpace = !result.empty();
            continue;
        }

        if (pendingSpace) {
            result.push_back(L' ');
            pendingSpace = false;
        }
        result.push_back(ch);
    }

    if (result.size() > 160) {
        result.resize(157);
        result.append(L"...");
    }

    return result.empty() ? L"(пустой текст)" : result;
}

std::wstring ReadUnicodeTextPreview(HANDLE data) {
    if (!data) {
        return {};
    }

    const auto size = GlobalSize(data);
    if (size < sizeof(wchar_t)) {
        return {};
    }

    const auto* locked = static_cast<const wchar_t*>(GlobalLock(data));
    if (!locked) {
        return {};
    }

    const size_t maxChars = size / sizeof(wchar_t);
    size_t chars = 0;
    while (chars < maxChars && locked[chars] != L'\0') {
        ++chars;
    }

    std::wstring preview(locked, locked + chars);
    GlobalUnlock(data);
    return CollapseWhitespace(std::move(preview));
}

void PushHistoryItem(ClipboardItem item) {
    const auto sameHash = [hash = item.hash](const ClipboardItem& other) {
        return other.hash == hash;
    };

    auto duplicate = std::find_if(g_history.begin(), g_history.end(), sameHash);
    if (duplicate != g_history.end()) {
        g_history.erase(duplicate);
    }

    g_history.push_front(std::move(item));
    while (g_history.size() > kMaxHistoryItems) {
        g_history.pop_back();
    }

    RefreshVisiblePopupHistory();
    UpdateSettingsWindow();
}

void RefreshVisiblePopupHistory() {
    if (!g_popupWindow || !g_listBox || !IsWindowVisible(g_popupWindow)) {
        return;
    }

    PopulateListBox();
    SendMessageW(g_listBox, LB_SETTOPINDEX, 0, 0);
    InvalidateRect(g_listBox, nullptr, TRUE);
    InvalidateRect(g_popupWindow, nullptr, TRUE);
}

void MoveHistoryItemToFront(size_t index) {
    if (index == 0 || index >= g_history.size()) {
        return;
    }

    ClipboardItem item = std::move(g_history[index]);
    g_history.erase(g_history.begin() + static_cast<std::ptrdiff_t>(index));
    g_history.push_front(std::move(item));
    UpdateSettingsWindow();
}

void DeleteHistoryItem(size_t index) {
    if (index >= g_history.size()) {
        return;
    }

    g_history.erase(g_history.begin() + static_cast<std::ptrdiff_t>(index));

    RefreshVisiblePopupHistory();
    UpdateSettingsWindow();
}

bool CaptureClipboard() {
    if (!OpenClipboard(g_mainWindow)) {
        SetTimer(g_mainWindow, kTimerCaptureRetry, 80, nullptr);
        return false;
    }

    KillTimer(g_mainWindow, kTimerCaptureRetry);

    ClipboardItem item;
    item.hash = 14695981039346656037ull;

    HANDLE unicodeText = GetClipboardData(CF_UNICODETEXT);
    if (!unicodeText) {
        CloseClipboard();
        return false;
    }

    item.preview = ReadUnicodeTextPreview(unicodeText);

    const SIZE_T size = GlobalSize(unicodeText);
    if (size < sizeof(wchar_t) || size > kMaxStoredBytesPerItem) {
        CloseClipboard();
        return false;
    }

    const void* locked = GlobalLock(unicodeText);
    if (!locked) {
        CloseClipboard();
        return false;
    }

    const auto* text = static_cast<const wchar_t*>(locked);
    const size_t maxChars = size / sizeof(wchar_t);
    size_t chars = 0;
    while (chars < maxChars && text[chars] != L'\0') {
        ++chars;
    }

    FormatData stored;
    stored.format = CF_UNICODETEXT;
    stored.bytes.resize((chars + 1) * sizeof(wchar_t));
    memcpy(stored.bytes.data(), text, chars * sizeof(wchar_t));
    reinterpret_cast<wchar_t*>(stored.bytes.data())[chars] = L'\0';
    GlobalUnlock(unicodeText);
    CloseClipboard();

    HashValue(item.hash, &stored.format, sizeof(stored.format));
    const auto storedSize = static_cast<std::uint64_t>(stored.bytes.size());
    HashValue(item.hash, &storedSize, sizeof(storedSize));
    HashValue(item.hash, stored.bytes.data(), stored.bytes.size());

    item.totalBytes = stored.bytes.size();
    item.formats.push_back(std::move(stored));

    if (item.preview.empty()) {
        item.preview = L"(пустой текст)";
    }

    PushHistoryItem(std::move(item));
    return true;
}

bool RestoreClipboardItem(const ClipboardItem& item) {
    if (item.formats.empty() || !OpenClipboard(g_mainWindow)) {
        return false;
    }

    EmptyClipboard();

    bool restoredAny = false;
    for (const auto& format : item.formats) {
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, format.bytes.size());
        if (!memory) {
            continue;
        }

        void* target = GlobalLock(memory);
        if (!target) {
            GlobalFree(memory);
            continue;
        }

        memcpy(target, format.bytes.data(), format.bytes.size());
        GlobalUnlock(memory);

        if (SetClipboardData(format.format, memory)) {
            restoredAny = true;
            memory = nullptr;
        }

        if (memory) {
            GlobalFree(memory);
        }
    }

    CloseClipboard();
    g_suppressNextClipboardUpdate = restoredAny;
    return restoredAny;
}

void SendPasteShortcut() {
    std::array<INPUT, 4> inputs{};

    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;

    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = L'V';

    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = L'V';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;

    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
}

bool IsCtrlKey(UINT vk) {
    return vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL;
}

bool IsAltKey(UINT vk) {
    return vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU;
}

bool IsShiftKey(UINT vk) {
    return vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT;
}

bool IsWinKey(UINT vk) {
    return vk == VK_LWIN || vk == VK_RWIN;
}

bool IsModifierKey(UINT vk) {
    return IsCtrlKey(vk) || IsAltKey(vk) || IsShiftKey(vk) || IsWinKey(vk);
}

UINT HotKeyRegisterModifiers(const HotKeyBinding& hotKey) {
    UINT modifiers = 0;
    if (hotKey.ctrl) {
        modifiers |= MOD_CONTROL;
    }
    if (hotKey.alt) {
        modifiers |= MOD_ALT;
    }
    if (hotKey.shift) {
        modifiers |= MOD_SHIFT;
    }
    if (hotKey.win) {
        modifiers |= MOD_WIN;
    }
    return modifiers;
}

std::wstring VirtualKeyName(UINT vk) {
    if (vk >= L'A' && vk <= L'Z') {
        return std::wstring(1, static_cast<wchar_t>(vk));
    }
    if (vk >= L'0' && vk <= L'9') {
        return std::wstring(1, static_cast<wchar_t>(vk));
    }

    UINT scanCode = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    switch (vk) {
    case VK_LEFT:
    case VK_UP:
    case VK_RIGHT:
    case VK_DOWN:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_END:
    case VK_HOME:
    case VK_INSERT:
    case VK_DELETE:
    case VK_DIVIDE:
    case VK_NUMLOCK:
        scanCode |= 0x100;
        break;
    }

    wchar_t name[64]{};
    if (GetKeyNameTextW(static_cast<LONG>(scanCode << 16), name,
                        static_cast<int>(std::size(name))) > 0) {
        return name;
    }

    return L"VK " + std::to_wstring(vk);
}

void AppendHotKeyPart(std::wstring& text, std::wstring_view part) {
    if (!text.empty()) {
        text += L"+";
    }
    text += part;
}

std::wstring HotKeyText(const HotKeyBinding& hotKey) {
    std::wstring text;
    if (hotKey.ctrl) {
        AppendHotKeyPart(text, L"Ctrl");
    }
    if (hotKey.alt) {
        AppendHotKeyPart(text, L"Alt");
    }
    if (hotKey.shift) {
        AppendHotKeyPart(text, L"Shift");
    }
    if (hotKey.win) {
        AppendHotKeyPart(text, L"Win");
    }
    if (hotKey.vk != 0) {
        AppendHotKeyPart(text, VirtualKeyName(hotKey.vk));
    }
    return text.empty() ? L"не задана" : text;
}

bool IsModifierOnlyHotKey(const HotKeyBinding& hotKey) {
    return hotKey.vk == 0 && hotKey.win &&
           (hotKey.ctrl || hotKey.alt || hotKey.shift);
}

bool IsUsableHotKey(const HotKeyBinding& hotKey) {
    const bool hasModifier = hotKey.ctrl || hotKey.alt || hotKey.shift || hotKey.win;
    return (hotKey.vk != 0 && hasModifier) || IsModifierOnlyHotKey(hotKey);
}

HotKeyBinding ReadKeyboardHotKey(UINT keyDownVk) {
    HotKeyBinding hotKey;
    hotKey.ctrl = IsCtrlKey(keyDownVk) || (GetKeyState(VK_CONTROL) & 0x8000);
    hotKey.alt = IsAltKey(keyDownVk) || (GetKeyState(VK_MENU) & 0x8000);
    hotKey.shift = IsShiftKey(keyDownVk) || (GetKeyState(VK_SHIFT) & 0x8000);
    hotKey.win = IsWinKey(keyDownVk) ||
                 (GetKeyState(VK_LWIN) & 0x8000) ||
                 (GetKeyState(VK_RWIN) & 0x8000);

    if (!IsModifierKey(keyDownVk)) {
        hotKey.vk = keyDownVk;
    }
    return hotKey;
}

HotKeyBinding ReadCaptureHookHotKey(UINT keyDownVk) {
    HotKeyBinding hotKey;
    hotKey.ctrl = g_captureCtrlDown;
    hotKey.alt = g_captureAltDown;
    hotKey.shift = g_captureShiftDown;
    hotKey.win = g_captureWinDown;
    if (!IsModifierKey(keyDownVk)) {
        hotKey.vk = keyDownVk;
    }
    return hotKey;
}

void SetHotKeyControlText(const HotKeyBinding& hotKey) {
    if (g_hotKeyControl) {
        SetWindowTextW(g_hotKeyControl, HotKeyText(hotKey).c_str());
        InvalidateRect(g_hotKeyControl, nullptr, TRUE);
    }
}

bool IsHotKeyActive() {
    return g_hotKeyRegistered || g_keyboardHook != nullptr;
}

void DisableActiveHotKey() {
    if (g_hotKeyRegistered) {
        UnregisterHotKey(g_mainWindow, kHotKeyId);
        g_hotKeyRegistered = false;
    }

    if (g_keyboardHook) {
        UnhookWindowsHookEx(g_keyboardHook);
        g_keyboardHook = nullptr;
    }

    g_modifierHotKeyActive = false;
}

void RefreshHookModifierState() {
    g_hookCtrlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    g_hookAltDown = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    g_hookShiftDown = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    g_hookWinDown = ((GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) &
                     0x8000) != 0;
}

void RefreshCaptureHookModifierState() {
    g_captureCtrlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    g_captureAltDown = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    g_captureShiftDown = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    g_captureWinDown =
        ((GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000) != 0;
}

void UpdateHookModifierState(UINT vk, bool down) {
    if (IsCtrlKey(vk)) {
        g_hookCtrlDown = down;
    } else if (IsAltKey(vk)) {
        g_hookAltDown = down;
    } else if (IsShiftKey(vk)) {
        g_hookShiftDown = down;
    } else if (IsWinKey(vk)) {
        g_hookWinDown = down;
    }
}

void UpdateCaptureHookModifierState(UINT vk, bool down) {
    if (IsCtrlKey(vk)) {
        g_captureCtrlDown = down;
    } else if (IsAltKey(vk)) {
        g_captureAltDown = down;
    } else if (IsShiftKey(vk)) {
        g_captureShiftDown = down;
    } else if (IsWinKey(vk)) {
        g_captureWinDown = down;
    }
}

bool ModifierOnlyHotKeyMatches() {
    return (!g_hotKeyBinding.ctrl || g_hookCtrlDown) &&
           (!g_hotKeyBinding.alt || g_hookAltDown) &&
           (!g_hotKeyBinding.shift || g_hookShiftDown) &&
           (!g_hotKeyBinding.win || g_hookWinDown);
}

bool IsConfiguredModifierKey(UINT vk) {
    return (g_hotKeyBinding.ctrl && IsCtrlKey(vk)) ||
           (g_hotKeyBinding.alt && IsAltKey(vk)) ||
           (g_hotKeyBinding.shift && IsShiftKey(vk)) ||
           (g_hotKeyBinding.win && IsWinKey(vk));
}

LRESULT CALLBACK KeyboardHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && !g_capturingHotKey &&
        IsModifierOnlyHotKey(g_hotKeyBinding)) {
        const auto* info = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
        const UINT vk = info->vkCode;
        const bool keyDown = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
        const bool keyUp = wParam == WM_KEYUP || wParam == WM_SYSKEYUP;

        if (keyDown || keyUp) {
            const bool configuredModifier = IsConfiguredModifierKey(vk);

            UpdateHookModifierState(vk, keyDown);

            if (keyDown && ModifierOnlyHotKeyMatches() && !g_modifierHotKeyActive) {
                g_modifierHotKeyActive = true;
                PostMessageW(g_mainWindow, WM_HOTKEY, kHotKeyId, 0);
            }

            if (keyUp && !ModifierOnlyHotKeyMatches()) {
                g_modifierHotKeyActive = false;
            }

            if (keyDown && configuredModifier && g_modifierHotKeyActive &&
                IsWinKey(vk)) {
                return 1;
            }
        }
    }

    return CallNextHookEx(g_keyboardHook, code, wParam, lParam);
}

LRESULT CALLBACK HotKeyCaptureHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && g_capturingHotKey && g_hotKeyControl &&
        GetFocus() == g_hotKeyControl) {
        const auto* info = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
        const UINT vk = info->vkCode;
        const bool keyDown = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
        const bool keyUp = wParam == WM_KEYUP || wParam == WM_SYSKEYUP;

        if (keyDown) {
            UpdateCaptureHookModifierState(vk, true);
            if (vk == VK_ESCAPE) {
                g_pendingHotKeyBinding = g_hotKeyBinding;
            } else if (vk == VK_BACK || vk == VK_DELETE) {
                g_pendingHotKeyBinding = {};
            } else {
                g_pendingHotKeyBinding = ReadCaptureHookHotKey(vk);
            }
            SetHotKeyControlText(g_pendingHotKeyBinding);
            return 1;
        }

        if (keyUp) {
            UpdateCaptureHookModifierState(vk, false);
            if (IsWinKey(vk)) {
                return CallNextHookEx(g_hotKeyCaptureHook, code, wParam, lParam);
            }
            return 1;
        }
    }

    return CallNextHookEx(g_hotKeyCaptureHook, code, wParam, lParam);
}

void StartHotKeyCapture() {
    g_capturingHotKey = true;
    RefreshCaptureHookModifierState();
    if (!g_hotKeyCaptureHook) {
        g_hotKeyCaptureHook = SetWindowsHookExW(WH_KEYBOARD_LL,
                                                HotKeyCaptureHookProc,
                                                GetModuleHandleW(nullptr), 0);
    }
}

void StopHotKeyCapture() {
    g_capturingHotKey = false;
    if (g_hotKeyCaptureHook) {
        UnhookWindowsHookEx(g_hotKeyCaptureHook);
        g_hotKeyCaptureHook = nullptr;
    }
}

bool TryActivateHotKey(const HotKeyBinding& hotKey) {
    if (IsModifierOnlyHotKey(hotKey)) {
        RefreshHookModifierState();
        g_keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHookProc,
                                           GetModuleHandleW(nullptr), 0);
        return g_keyboardHook != nullptr;
    }

    const UINT modifiers = HotKeyRegisterModifiers(hotKey);
    if (RegisterHotKey(g_mainWindow, kHotKeyId, modifiers | MOD_NOREPEAT,
                       hotKey.vk)) {
        g_hotKeyRegistered = true;
        return true;
    }
    return false;
}

void UpdateTrayTip() {
    if (!g_mainWindow) {
        return;
    }

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_mainWindow;
    nid.uID = kTrayId;
    nid.uFlags = NIF_TIP;

    std::wstring tip = std::wstring(kAppName) + L" - " + HotKeyText(g_hotKeyBinding);
    wcsncpy_s(nid.szTip, tip.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

bool ApplyHotKeyBinding(const HotKeyBinding& hotKey, bool showMessage) {
    if (!IsUsableHotKey(hotKey)) {
        if (showMessage) {
            MessageBoxW(g_settingsWindow,
                        L"Выберите комбинацию с модификатором. "
                        L"Для комбинации без обычной клавиши используйте Win, "
                        L"например Ctrl+Shift+Win.",
                        kAppName, MB_ICONWARNING | MB_OK);
        }
        return false;
    }

    const HotKeyBinding previousHotKey = g_hotKeyBinding;
    const bool previousActive = IsHotKeyActive();

    DisableActiveHotKey();
    g_hotKeyBinding = hotKey;

    if (TryActivateHotKey(hotKey)) {
        g_pendingHotKeyBinding = hotKey;
        SetHotKeyControlText(g_pendingHotKeyBinding);
        UpdateTrayTip();
        UpdateSettingsWindow();
        return true;
    }

    const DWORD error = GetLastError();
    DisableActiveHotKey();
    g_hotKeyBinding = previousHotKey;
    if (previousActive) {
        TryActivateHotKey(previousHotKey);
    }

    g_pendingHotKeyBinding = g_hotKeyBinding;
    SetHotKeyControlText(g_pendingHotKeyBinding);
    UpdateTrayTip();
    UpdateSettingsWindow();

    if (showMessage) {
        std::wstring text = L"Не удалось включить " + HotKeyText(hotKey) +
                            L". Возможно, эта комбинация уже занята.\nКод ошибки: " +
                            std::to_wstring(error);
        MessageBoxW(g_settingsWindow, text.c_str(), kAppName, MB_ICONWARNING | MB_OK);
    }
    return false;
}

void HidePopup() {
    g_hoveredHistoryIndex = -1;
    if (g_popupWindow && IsWindowVisible(g_popupWindow)) {
        SavePopupPlacement();
        ShowWindow(g_popupWindow, SW_HIDE);
    }
}

void PasteHistoryIndex(int index) {
    if (index < 0 || static_cast<size_t>(index) >= g_history.size()) {
        return;
    }

    const size_t historyIndex = static_cast<size_t>(index);
    const HWND target = g_targetWindow;
    if (!RestoreClipboardItem(g_history[historyIndex])) {
        return;
    }

    MoveHistoryItemToFront(historyIndex);
    HidePopup();

    if (target && IsWindow(target) &&
        target != g_popupWindow && target != g_mainWindow &&
        target != g_settingsWindow) {
        SetForegroundWindow(target);
        Sleep(60);
        SendPasteShortcut();
    }
}

void PopulateListBox() {
    SendMessageW(g_listBox, LB_RESETCONTENT, 0, 0);
    g_hoveredHistoryIndex = -1;

    if (g_history.empty()) {
        SendMessageW(g_listBox, LB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(
                         UiText(L"History is empty", L"История пуста")));
        SendMessageW(g_listBox, LB_SETCURSEL, 0, 0);
        return;
    }

    for (size_t i = 0; i < g_history.size(); ++i) {
        std::wstring line = std::to_wstring(i + 1) + L". " + g_history[i].preview;
        SendMessageW(g_listBox, LB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(line.c_str()));
    }
    SendMessageW(g_listBox, LB_SETCURSEL, 0, 0);
}

void PositionPopup() {
    if (!g_popupRectLoaded) {
        g_popupHasSavedRect = LoadPopupPlacement(g_popupSavedRect);
        g_popupRectLoaded = true;
    }

    const RECT rect = NormalizePopupRect(g_popupHasSavedRect ? g_popupSavedRect
                                                             : DefaultPopupRect());

    SetWindowPos(g_popupWindow, HWND_TOPMOST, rect.left, rect.top,
                 RectWidth(rect), RectHeight(rect),
                 SWP_NOACTIVATE);
}

void ShowHistoryPopup() {
    if (!g_popupWindow) {
        return;
    }

    g_targetWindow = GetForegroundWindow();
    PopulateListBox();
    PositionPopup();

    ShowWindow(g_popupWindow, SW_SHOWNORMAL);
    SetForegroundWindow(g_popupWindow);
    SetFocus(g_listBox);
}

void ToggleHistoryPopup() {
    if (g_popupWindow && IsWindowVisible(g_popupWindow)) {
        HidePopup();
    } else {
        ShowHistoryPopup();
    }
}

void AddTrayIcon(HWND hwnd) {
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = kTrayId;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = kTrayMessage;
    nid.hIcon = LoadAppIcon(GetSystemMetrics(SM_CXSMICON),
                            GetSystemMetrics(SM_CYSMICON));
    std::wstring tip = std::wstring(kAppName) + L" - " + HotKeyText(g_hotKeyBinding);
    wcsncpy_s(nid.szTip, tip.c_str(), _TRUNCATE);

    Shell_NotifyIconW(NIM_ADD, &nid);
}

void RemoveTrayIcon(HWND hwnd) {
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = kTrayId;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

void UpdateSettingsWindow() {
    if (!g_settingsWindow) {
        return;
    }

    SetWindowTextW(g_settingsWindow,
                   UiText(L"MaxB Handy Clipboard Settings",
                          L"Настройки MaxB Handy Clipboard"));
    if (g_settingsTitleLabel) {
        SetWindowTextW(g_settingsTitleLabel, kAppName);
    }
    if (g_hotKeyLabel) {
        SetWindowTextW(g_hotKeyLabel,
                       UiText(L"History shortcut", L"Комбинация для истории"));
    }
    if (g_applyHotKeyButton) {
        SetWindowTextW(g_applyHotKeyButton, UiText(L"Apply", L"Применить"));
    }
    if (g_startupCheckBox) {
        SetWindowTextW(g_startupCheckBox,
                       UiText(L"Start with Windows",
                              L"Запускаться вместе с системой"));
        SendMessageW(g_startupCheckBox, BM_SETCHECK,
                     g_startWithWindows ? BST_CHECKED : BST_UNCHECKED, 0);
        InvalidateRect(g_startupCheckBox, nullptr, TRUE);
    }
    if (g_themeLabel) {
        SetWindowTextW(g_themeLabel, UiText(L"Theme", L"Тема интерфейса"));
    }
    if (g_languageLabel) {
        SetWindowTextW(g_languageLabel, UiText(L"Language", L"Язык"));
    }
    if (g_exitButton) {
        SetWindowTextW(g_exitButton, UiText(L"Exit", L"Выход"));
    }

    InvalidateRect(g_settingsWindow, nullptr, TRUE);
}

void ShowSettingsWindow() {
    if (!g_settingsWindow) {
        return;
    }

    g_pendingHotKeyBinding = g_hotKeyBinding;
    SetHotKeyControlText(g_pendingHotKeyBinding);
    UpdateSettingsWindow();

    POINT cursor{};
    GetCursorPos(&cursor);

    MONITORINFO monitorInfo{sizeof(MONITORINFO)};
    HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    GetMonitorInfoW(monitor, &monitorInfo);

    RECT rect{};
    GetWindowRect(g_settingsWindow, &rect);
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    const RECT work = monitorInfo.rcWork;

    int x = cursor.x + 12;
    int y = cursor.y + 12;
    if (x + width > work.right) {
        x = work.right - width;
    }
    if (y + height > work.bottom) {
        y = work.bottom - height;
    }
    x = std::max<int>(work.left, x);
    y = std::max<int>(work.top, y);

    SetWindowPos(g_settingsWindow, HWND_TOP, x, y, 0, 0,
                 SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(g_settingsWindow);
}

void DrawPopupChrome(HWND hwnd, HDC dc) {
    EnsureUiResources();

    RECT client{};
    GetClientRect(hwnd, &client);
    FillSolidRect(dc, client, ThemeSurfaceColor());

    RECT titleRect{kPopupContentLeft, 44, client.right - 96, 91};
    DrawTextInRect(dc, UiText(L"Clipboard history", L"История буфера"),
                   titleRect, g_popupTitleFont, ThemeTextColor(),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

    const RECT closeButton = PopupCloseButtonRect(client);
    DrawXGlyph(dc, closeButton, ThemeMutedTextColor(), 8);
    DrawPopupScrollbar(hwnd, dc);
}

RECT PopupCloseButtonRect(RECT clientRect) {
    const int top = 39;
    return RECT{clientRect.right - kPopupCloseButtonMargin -
                    kPopupCloseButtonSize,
                top,
                clientRect.right - kPopupCloseButtonMargin,
                top + kPopupCloseButtonSize};
}

RECT PopupListRect(RECT clientRect) {
    return RECT{kPopupListLeft,
                kPopupListTop,
                clientRect.right - kPopupListLeft - kPopupScrollbarLaneWidth,
                clientRect.bottom - kPopupListBottomMargin};
}

RECT PopupScrollbarTrackRect(RECT clientRect) {
    const int left = clientRect.right - kPopupListLeft - kPopupScrollbarWidth;
    return RECT{left,
                kPopupListTop + 2,
                left + kPopupScrollbarWidth,
                clientRect.bottom - kPopupListBottomMargin - 2};
}

int PopupVisibleHistoryRows() {
    if (!g_listBox) {
        return 0;
    }

    RECT listRect{};
    GetClientRect(g_listBox, &listRect);
    return std::max(1, RectHeight(listRect) / kHistoryItemHeight);
}

bool PopupScrollbarMetrics(RECT clientRect, RECT& track, RECT& thumb,
                           int& maxTopIndex) {
    if (!g_listBox || g_history.empty()) {
        return false;
    }

    const int count = static_cast<int>(SendMessageW(g_listBox, LB_GETCOUNT, 0, 0));
    const int visibleRows = PopupVisibleHistoryRows();
    maxTopIndex = std::max(0, count - visibleRows);
    if (count <= 0 || maxTopIndex <= 0) {
        return false;
    }

    track = PopupScrollbarTrackRect(clientRect);
    const int trackHeight = RectHeight(track);
    if (trackHeight <= 0) {
        return false;
    }

    const int thumbHeight =
        std::min(trackHeight,
                 std::max(kPopupScrollbarMinHeight,
                          MulDiv(trackHeight, visibleRows, count)));
    const int travel = trackHeight - thumbHeight;
    const int topIndex =
        static_cast<int>(SendMessageW(g_listBox, LB_GETTOPINDEX, 0, 0));
    const int thumbTop = track.top +
                         (travel > 0 ? MulDiv(std::clamp(topIndex, 0, maxTopIndex),
                                              travel, maxTopIndex)
                                     : 0);
    thumb = RECT{track.left, thumbTop, track.right, thumbTop + thumbHeight};
    return true;
}

void DrawPopupScrollbar(HWND hwnd, HDC dc) {
    RECT client{};
    GetClientRect(hwnd, &client);

    RECT track{};
    RECT thumb{};
    int maxTopIndex = 0;
    if (!PopupScrollbarMetrics(client, track, thumb, maxTopIndex)) {
        return;
    }

    DrawRoundedRect(dc, thumb, kElementCornerRadius, RGB(132, 139, 148),
                    RGB(132, 139, 148));
}

void InvalidatePopupScrollbar() {
    if (!g_popupWindow || !IsWindowVisible(g_popupWindow)) {
        return;
    }

    RECT client{};
    GetClientRect(g_popupWindow, &client);
    RECT invalid = PopupScrollbarTrackRect(client);
    InflateRect(&invalid, 4, 4);
    InvalidateRect(g_popupWindow, &invalid, TRUE);
}

void SetPopupScrollFromThumbTop(int thumbTop) {
    if (!g_listBox || !g_popupWindow) {
        return;
    }

    RECT client{};
    GetClientRect(g_popupWindow, &client);

    RECT track{};
    RECT thumb{};
    int maxTopIndex = 0;
    if (!PopupScrollbarMetrics(client, track, thumb, maxTopIndex)) {
        return;
    }

    const int travel = RectHeight(track) - RectHeight(thumb);
    if (travel <= 0) {
        return;
    }

    const int trackTop = static_cast<int>(track.top);
    const int trackBottom = static_cast<int>(track.bottom);
    const int clampedTop =
        std::clamp(thumbTop, trackTop, trackBottom - RectHeight(thumb));
    const int topIndex = MulDiv(clampedTop - trackTop, maxTopIndex, travel);
    SendMessageW(g_listBox, LB_SETTOPINDEX, topIndex, 0);
    InvalidateRect(g_listBox, nullptr, TRUE);
    InvalidatePopupScrollbar();
}

LRESULT PopupHitTest(HWND hwnd, LPARAM lParam) {
    RECT client{};
    GetClientRect(hwnd, &client);

    POINT point{static_cast<LONG>(static_cast<short>(LOWORD(lParam))),
                static_cast<LONG>(static_cast<short>(HIWORD(lParam)))};
    ScreenToClient(hwnd, &point);

    const bool left = point.x < kPopupResizeBorder;
    const bool right = point.x >= client.right - kPopupResizeBorder;
    const bool top = point.y < kPopupResizeBorder;
    const bool bottom = point.y >= client.bottom - kPopupResizeBorder;

    if (top && left) {
        return HTTOPLEFT;
    }
    if (top && right) {
        return HTTOPRIGHT;
    }
    if (bottom && left) {
        return HTBOTTOMLEFT;
    }
    if (bottom && right) {
        return HTBOTTOMRIGHT;
    }
    if (left) {
        return HTLEFT;
    }
    if (right) {
        return HTRIGHT;
    }
    if (top) {
        return HTTOP;
    }
    if (bottom) {
        return HTBOTTOM;
    }

    const RECT closeButton = PopupCloseButtonRect(client);
    if (PtInRect(&closeButton, point)) {
        return HTCLIENT;
    }

    if (point.y < kPopupHeaderHeight) {
        return HTCAPTION;
    }

    return HTCLIENT;
}

RECT HistoryDeleteButtonRect(RECT itemRect) {
    const int top = itemRect.top +
                    (RectHeight(itemRect) - kHistoryDeleteButtonSize) / 2;
    return RECT{itemRect.right - kHistoryDeleteButtonMargin -
                    kHistoryDeleteButtonSize,
                top,
                itemRect.right - kHistoryDeleteButtonMargin,
                top + kHistoryDeleteButtonSize};
}

void DrawHistoryDivider(HDC dc, const RECT& rect) {
    HPEN pen = CreatePen(PS_SOLID, 1, ThemeHistoryDividerColor());
    HGDIOBJ oldPen = SelectObject(dc, pen);
    const int y = rect.bottom - 1;

    MoveToEx(dc, rect.left, y, nullptr);
    LineTo(dc, rect.right, y);

    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

void DrawHistoryListOutline(HWND hwnd) {
    RECT client{};
    GetClientRect(hwnd, &client);
    if (RectWidth(client) <= 1 || RectHeight(client) <= 1) {
        return;
    }

    HDC dc = GetDC(hwnd);
    RECT outline = client;
    outline.right -= 1;
    outline.bottom -= 1;
    DrawRoundedOutline(dc, outline, kElementCornerRadius,
                       ThemeHistoryDividerColor());
    ReleaseDC(hwnd, dc);
}

void InvalidateHistoryListRow(HWND hwnd, int index) {
    if (index < 0) {
        return;
    }

    RECT row{};
    if (SendMessageW(hwnd, LB_GETITEMRECT, index,
                     reinterpret_cast<LPARAM>(&row)) != LB_ERR) {
        InvalidateRect(hwnd, &row, FALSE);
    }
}

void SetHoveredHistoryIndex(HWND hwnd, int index) {
    if (index == g_hoveredHistoryIndex) {
        return;
    }

    const int previousIndex = g_hoveredHistoryIndex;
    g_hoveredHistoryIndex = index;
    InvalidateHistoryListRow(hwnd, previousIndex);
    InvalidateHistoryListRow(hwnd, g_hoveredHistoryIndex);
}

int HistoryIndexFromListPoint(HWND hwnd, LPARAM lParam) {
    if (g_history.empty()) {
        return -1;
    }

    const DWORD result = static_cast<DWORD>(
        SendMessageW(hwnd, LB_ITEMFROMPOINT, 0, lParam));
    const int index = LOWORD(result);
    const bool outside = HIWORD(result) != 0;
    if (outside || index < 0 || static_cast<size_t>(index) >= g_history.size()) {
        return -1;
    }

    return index;
}

void DrawXGlyph(HDC dc, const RECT& rect, COLORREF color, int inset) {
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HGDIOBJ oldPen = SelectObject(dc, pen);

    MoveToEx(dc, rect.left + inset, rect.top + inset, nullptr);
    LineTo(dc, rect.right - inset, rect.bottom - inset);
    MoveToEx(dc, rect.right - inset, rect.top + inset, nullptr);
    LineTo(dc, rect.left + inset, rect.bottom - inset);

    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

void DrawDeleteButton(HDC dc, const RECT& rect) {
    DrawXGlyph(dc, rect, RGB(190, 18, 60), 7);
}

void DrawHistoryListItem(const DRAWITEMSTRUCT& item) {
    EnsureUiResources();

    HDC dc = item.hDC;
    RECT rect = item.rcItem;
    FillSolidRect(dc, rect, ThemeSurfaceColor());

    if (item.itemID == static_cast<UINT>(-1)) {
        return;
    }

    if (g_history.empty()) {
        RECT emptyRect = rect;
        InflateRect(&emptyRect, -16, 0);
        DrawTextInRect(dc, UiText(L"History is empty", L"История пуста"),
                       emptyRect, g_bodyFont,
                       ThemeMutedTextColor(), DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        return;
    }

    const size_t index = static_cast<size_t>(item.itemID);
    if (index >= g_history.size()) {
        return;
    }

    const bool hovered = static_cast<int>(index) == g_hoveredHistoryIndex;
    if (hovered) {
        RECT hoverRect = rect;
        hoverRect.right -= 1;
        hoverRect.bottom -= 1;
        DrawRoundedRect(dc, hoverRect, kElementCornerRadius,
                        ThemeSurfaceHoverColor(), ThemeSurfaceHoverColor());
    }

    const RECT deleteButton = HistoryDeleteButtonRect(rect);
    DrawDeleteButton(dc, deleteButton);

    RECT preview{rect.left + 24, rect.top + 10, deleteButton.left - 12,
                 rect.bottom - 10};
    DrawTextInRect(dc, g_history[index].preview, preview, g_bodyFont, ThemeTextColor(),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

    if (index + 1 < g_history.size()) {
        DrawHistoryDivider(dc, rect);
    }
}

LRESULT CALLBACK ListBoxProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_MOUSEMOVE) {
        TRACKMOUSEEVENT tracking{sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&tracking);
        SetHoveredHistoryIndex(hwnd, HistoryIndexFromListPoint(hwnd, lParam));
    }

    if (message == WM_MOUSELEAVE) {
        SetHoveredHistoryIndex(hwnd, -1);
        return 0;
    }

    if (message == WM_MOUSEWHEEL && !g_history.empty()) {
        const int count = static_cast<int>(SendMessageW(hwnd, LB_GETCOUNT, 0, 0));
        const int visibleRows = PopupVisibleHistoryRows();
        const int maxTopIndex = std::max(0, count - visibleRows);
        if (maxTopIndex > 0) {
            const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            const int absDelta = delta < 0 ? -delta : delta;
            const int steps = std::max(1, (absDelta / WHEEL_DELTA) * 3);
            const int currentTop =
                static_cast<int>(SendMessageW(hwnd, LB_GETTOPINDEX, 0, 0));
            const int nextTop =
                std::clamp(currentTop + (delta < 0 ? steps : -steps),
                           0, maxTopIndex);
            SendMessageW(hwnd, LB_SETTOPINDEX, nextTop, 0);
            InvalidateRect(hwnd, nullptr, TRUE);
            InvalidatePopupScrollbar();
        }
        return 0;
    }

    if (message == WM_LBUTTONUP && !g_history.empty()) {
        const DWORD result = static_cast<DWORD>(
            SendMessageW(hwnd, LB_ITEMFROMPOINT, 0, lParam));
        const int index = LOWORD(result);
        const bool outside = HIWORD(result) != 0;
        if (!outside) {
            RECT itemRect{};
            const LRESULT rectResult =
                SendMessageW(hwnd, LB_GETITEMRECT, index,
                             reinterpret_cast<LPARAM>(&itemRect));
            POINT point{static_cast<short>(LOWORD(lParam)),
                        static_cast<short>(HIWORD(lParam))};

            if (rectResult != LB_ERR) {
                const RECT deleteButton =
                    HistoryDeleteButtonRect(itemRect);
                if (PtInRect(&deleteButton, point)) {
                    DeleteHistoryItem(static_cast<size_t>(index));
                    return 0;
                }
            }

            SendMessageW(hwnd, LB_SETCURSEL, index, 0);
            PasteHistoryIndex(index);
            return 0;
        }
    }

    if (message == WM_KEYDOWN) {
        if (wParam == VK_RETURN && !g_history.empty()) {
            const int index = static_cast<int>(SendMessageW(hwnd, LB_GETCURSEL, 0, 0));
            PasteHistoryIndex(index);
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            HidePopup();
            return 0;
        }
    }

    const LRESULT result =
        CallWindowProcW(g_originalListProc, hwnd, message, wParam, lParam);

    switch (message) {
    case WM_PAINT:
        DrawHistoryListOutline(hwnd);
        break;

    case WM_KEYDOWN:
    case WM_MOUSEWHEEL:
    case WM_VSCROLL:
        InvalidatePopupScrollbar();
        break;
    }

    return result;
}

void DrawHotKeyInput(HWND hwnd, HDC dc) {
    EnsureUiResources();

    RECT client{};
    GetClientRect(hwnd, &client);
    FillSolidRect(dc, client, ThemeSurfaceColor());

    const bool focused = GetFocus() == hwnd;
    const COLORREF border = focused ? ThemeSelectedBorderColor() : ThemeBorderColor();
    DrawRoundedRect(dc, client, kElementCornerRadius, ThemeInputColor(), border);

    wchar_t text[80]{};
    GetWindowTextW(hwnd, text, static_cast<int>(std::size(text)));
    RECT textRect{client.left + 16, client.top, client.right - 12, client.bottom};
    DrawTextInRect(dc, text, textRect, g_bodyFont, ThemeTextColor(),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

LRESULT CALLBACK HotKeyControlProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS | DLGC_WANTCHARS;

    case WM_LBUTTONDOWN:
        SetFocus(hwnd);
        return 0;

    case WM_SETFOCUS:
        StartHotKeyCapture();
        SetHotKeyControlText(g_pendingHotKeyBinding);
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;

    case WM_KILLFOCUS:
        StopHotKeyCapture();
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;

    case WM_SETTEXT: {
        const LRESULT result = DefWindowProcW(hwnd, message, wParam, lParam);
        InvalidateRect(hwnd, nullptr, TRUE);
        return result;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        DrawHotKeyInput(hwnd, dc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        const UINT vk = static_cast<UINT>(wParam);
        if (vk == VK_ESCAPE) {
            g_pendingHotKeyBinding = g_hotKeyBinding;
        } else if (vk == VK_BACK || vk == VK_DELETE) {
            g_pendingHotKeyBinding = {};
        } else {
            g_pendingHotKeyBinding = ReadKeyboardHotKey(vk);
        }

        SetHotKeyControlText(g_pendingHotKeyBinding);
        return 0;
    }

    case WM_KEYUP:
    case WM_SYSKEYUP:
    case WM_CHAR:
    case WM_SYSCHAR:
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK PopupProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        EnsureUiResources();
        ApplyWindowVisualStyle(hwnd, true);
        ApplyRoundedRegion(hwnd, kWindowCornerRadius);
        g_listBox = CreateWindowExW(
            0, L"LISTBOX", nullptr,
            WS_CHILD | WS_VISIBLE | LBS_NOTIFY |
                LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
            0, 0, 0, 0, hwnd,
            reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kHistoryListId)),
            GetModuleHandleW(nullptr), nullptr);

        SendMessageW(g_listBox, WM_SETFONT,
                     reinterpret_cast<WPARAM>(g_bodyFont), TRUE);
        SendMessageW(g_listBox, LB_SETITEMHEIGHT, 0, kHistoryItemHeight);
        ApplyRoundedRegion(g_listBox, kElementCornerRadius);

        g_originalListProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(g_listBox, GWLP_WNDPROC,
                              reinterpret_cast<LONG_PTR>(ListBoxProc)));
        return 0;

    case WM_NCCALCSIZE:
        if (wParam) {
            return 0;
        }
        break;

    case WM_NCHITTEST:
        return PopupHitTest(hwnd, lParam);

    case WM_GETMINMAXINFO: {
        auto* limits = reinterpret_cast<MINMAXINFO*>(lParam);
        limits->ptMinTrackSize.x = kPopupMinWidth;
        limits->ptMinTrackSize.y = kPopupMinHeight;
        return 0;
    }

    case WM_SIZE:
        ApplyRoundedRegion(hwnd, kWindowCornerRadius);
        if (g_listBox) {
            RECT client{};
            GetClientRect(hwnd, &client);
            const RECT listRect = PopupListRect(client);
            MoveWindow(g_listBox, listRect.left, listRect.top,
                       RectWidth(listRect), RectHeight(listRect), TRUE);
            ApplyRoundedRegion(g_listBox, kElementCornerRadius);
        }
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;

    case WM_EXITSIZEMOVE:
        SavePopupPlacement();
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;

    case WM_MEASUREITEM:
        if (auto* measure = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
            wParam == kHistoryListId || measure->CtlID == kHistoryListId) {
            measure->itemHeight = kHistoryItemHeight;
            return TRUE;
        }
        break;

    case WM_DRAWITEM:
        if (auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            wParam == kHistoryListId || draw->CtlID == kHistoryListId) {
            DrawHistoryListItem(*draw);
            return TRUE;
        }
        break;

    case WM_CTLCOLORLISTBOX:
        EnsureUiResources();
        SetBkMode(reinterpret_cast<HDC>(wParam), TRANSPARENT);
        return reinterpret_cast<LRESULT>(g_surfaceBrush);

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        DrawPopupChrome(hwnd, dc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        RECT client{};
        GetClientRect(hwnd, &client);
        POINT point{static_cast<short>(LOWORD(lParam)),
                    static_cast<short>(HIWORD(lParam))};

        RECT track{};
        RECT thumb{};
        int maxTopIndex = 0;
        if (PopupScrollbarMetrics(client, track, thumb, maxTopIndex)) {
            RECT hitRect = track;
            InflateRect(&hitRect, 8, 2);
            if (PtInRect(&hitRect, point)) {
                g_popupScrollbarDragging = true;
                g_popupScrollbarDragOffsetY =
                    PtInRect(&thumb, point) ? point.y - thumb.top
                                            : RectHeight(thumb) / 2;
                SetCapture(hwnd);
                SetPopupScrollFromThumbTop(point.y - g_popupScrollbarDragOffsetY);
                return 0;
            }
        }
        break;
    }

    case WM_MOUSEMOVE:
        if (g_popupScrollbarDragging) {
            POINT point{static_cast<short>(LOWORD(lParam)),
                        static_cast<short>(HIWORD(lParam))};
            SetPopupScrollFromThumbTop(point.y - g_popupScrollbarDragOffsetY);
            return 0;
        }
        break;

    case WM_CAPTURECHANGED:
        g_popupScrollbarDragging = false;
        g_popupScrollbarDragOffsetY = 0;
        return 0;

    case WM_LBUTTONUP: {
        if (g_popupScrollbarDragging) {
            g_popupScrollbarDragging = false;
            g_popupScrollbarDragOffsetY = 0;
            ReleaseCapture();
            return 0;
        }

        RECT client{};
        GetClientRect(hwnd, &client);
        POINT point{static_cast<short>(LOWORD(lParam)),
                    static_cast<short>(HIWORD(lParam))};
        const RECT closeButton = PopupCloseButtonRect(client);
        if (PtInRect(&closeButton, point)) {
            HidePopup();
            return 0;
        }
        break;
    }

    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE && IsWindowVisible(hwnd)) {
            g_popupScrollbarDragging = false;
            g_popupScrollbarDragOffsetY = 0;
            if (GetCapture() == hwnd) {
                ReleaseCapture();
            }
            HidePopup();
        }
        return 0;

    case WM_CLOSE:
        HidePopup();
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

RECT SettingsThemeSelectorRect() {
    return RECT{216, 326, 456, 364};
}

RECT SettingsLightThemeRect() {
    return RECT{220, 330, 336, 360};
}

RECT SettingsDarkThemeRect() {
    return RECT{340, 330, 452, 360};
}

RECT SettingsLanguageSelectorRect() {
    return RECT{216, 374, 456, 412};
}

RECT SettingsEnglishLanguageRect() {
    return RECT{220, 378, 336, 408};
}

RECT SettingsRussianLanguageRect() {
    return RECT{340, 378, 452, 408};
}

RECT SettingsCloseButtonRect(RECT clientRect) {
    return RECT{clientRect.right - 70, 30, clientRect.right - 34, 66};
}

LRESULT SettingsHitTest(HWND hwnd, LPARAM lParam) {
    RECT client{};
    GetClientRect(hwnd, &client);

    POINT point{static_cast<LONG>(static_cast<short>(LOWORD(lParam))),
                static_cast<LONG>(static_cast<short>(HIWORD(lParam)))};
    ScreenToClient(hwnd, &point);

    const bool left = point.x < kSettingsResizeBorder;
    const bool right = point.x >= client.right - kSettingsResizeBorder;
    const bool top = point.y < kSettingsResizeBorder;
    const bool bottom = point.y >= client.bottom - kSettingsResizeBorder;

    if (top && left) {
        return HTTOPLEFT;
    }
    if (top && right) {
        return HTTOPRIGHT;
    }
    if (bottom && left) {
        return HTBOTTOMLEFT;
    }
    if (bottom && right) {
        return HTBOTTOMRIGHT;
    }
    if (left) {
        return HTLEFT;
    }
    if (right) {
        return HTRIGHT;
    }
    if (top) {
        return HTTOP;
    }
    if (bottom) {
        return HTBOTTOM;
    }

    const RECT closeButton = SettingsCloseButtonRect(client);
    if (PtInRect(&closeButton, point)) {
        return HTCLIENT;
    }

    if (point.y < kSettingsHeaderHeight) {
        return HTCAPTION;
    }

    return HTCLIENT;
}

void DrawCheckMark(HDC dc, const RECT& rect, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    const int x = rect.left;
    const int y = rect.top;
    MoveToEx(dc, x + 6, y + 12, nullptr);
    LineTo(dc, x + 10, y + 16);
    LineTo(dc, x + 18, y + 6);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

void DrawSunIcon(HDC dc, int cx, int cy) {
    HBRUSH brush = CreateSolidBrush(RGB(253, 230, 138));
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(dc, cx - 8, cy - 8, cx + 8, cy + 8);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(brush);
}

void DrawMoonIcon(HDC dc, int cx, int cy, COLORREF backColor) {
    HBRUSH moonBrush = CreateSolidBrush(RGB(51, 65, 85));
    HBRUSH cutBrush = CreateSolidBrush(backColor);
    HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
    HGDIOBJ oldBrush = SelectObject(dc, moonBrush);
    Ellipse(dc, cx - 8, cy - 8, cx + 8, cy + 8);
    SelectObject(dc, cutBrush);
    Ellipse(dc, cx - 3, cy - 11, cx + 13, cy + 5);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(cutBrush);
    DeleteObject(moonBrush);
}

void DrawSettingsThemeSelector(HDC dc) {
    const RECT selector = SettingsThemeSelectorRect();
    const RECT light = SettingsLightThemeRect();
    const RECT dark = SettingsDarkThemeRect();
    const COLORREF selectorFill = ThemeSelectorFillColor();
    const COLORREF selectedFill =
        g_darkThemeSelected ? ThemeAccentColor() : ThemeSurfaceColor();
    const COLORREF selectedText =
        g_darkThemeSelected ? RGB(255, 255, 255) : ThemeTextColor();
    const COLORREF mutedText = ThemeMutedTextColor();

    DrawRoundedRect(dc, selector, kElementCornerRadius, selectorFill, ThemeBorderColor());
    DrawRoundedRect(dc, g_darkThemeSelected ? dark : light, kElementCornerRadius, selectedFill,
                    ThemeBorderColor());

    const int centerY = selector.top + RectHeight(selector) / 2;
    const auto drawChoice = [&](const RECT& choice, std::wstring_view text,
                                bool moon, COLORREF textColor) {
        SIZE textSize{};
        HGDIOBJ oldFont = SelectObject(dc, g_smallFont);
        GetTextExtentPoint32W(dc, text.data(), static_cast<int>(text.size()),
                              &textSize);
        SelectObject(dc, oldFont);

        constexpr int iconSize = 16;
        constexpr int iconTextGap = 10;
        const int contentWidth = iconSize + iconTextGap + textSize.cx;
        const int startX = choice.left +
                           std::max(0, (RectWidth(choice) - contentWidth) / 2);
        const int iconCenterX = startX + iconSize / 2;
        if (moon) {
            DrawMoonIcon(dc, iconCenterX, centerY,
                         g_darkThemeSelected ? ThemeAccentColor() : selectorFill);
        } else {
            DrawSunIcon(dc, iconCenterX, centerY);
        }

        RECT textRect{startX + iconSize + iconTextGap, choice.top,
                      choice.right - 4, choice.bottom};
        DrawTextInRect(dc, text, textRect, g_smallFont, textColor,
                       DT_SINGLELINE | DT_VCENTER);
    };

    drawChoice(light, UiText(L"Light", L"Светлая"), false,
               g_darkThemeSelected ? mutedText : selectedText);
    drawChoice(dark, UiText(L"Dark", L"Темная"), true,
               g_darkThemeSelected ? selectedText : mutedText);
}

void DrawSettingsLanguageSelector(HDC dc) {
    const RECT selector = SettingsLanguageSelectorRect();
    const RECT english = SettingsEnglishLanguageRect();
    const RECT russian = SettingsRussianLanguageRect();
    const COLORREF selectorFill = ThemeSelectorFillColor();
    const COLORREF selectedFill =
        g_darkThemeSelected ? ThemeAccentColor() : ThemeSurfaceColor();
    const COLORREF selectedText =
        g_darkThemeSelected ? RGB(255, 255, 255) : ThemeTextColor();
    const COLORREF mutedText = ThemeMutedTextColor();

    DrawRoundedRect(dc, selector, kElementCornerRadius, selectorFill,
                    ThemeBorderColor());
    DrawRoundedRect(dc, g_englishLanguageSelected ? english : russian,
                    kElementCornerRadius, selectedFill, ThemeBorderColor());

    const auto drawChoice = [&](const RECT& choice, std::wstring_view text,
                                COLORREF textColor) {
        DrawTextInRect(dc, text, choice, g_smallFont, textColor,
                       DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    };

    drawChoice(english, UiText(L"English", L"Английский"),
               g_englishLanguageSelected ? selectedText : mutedText);
    drawChoice(russian, UiText(L"Russian", L"Русский"),
               g_englishLanguageSelected ? mutedText : selectedText);
}

void DrawSettingsChrome(HWND hwnd, HDC dc) {
    EnsureUiResources();

    RECT client{};
    GetClientRect(hwnd, &client);
    FillSolidRect(dc, client, ThemeWindowColor());

    DrawXGlyph(dc, SettingsCloseButtonRect(client), ThemeMutedTextColor(), 8);

    DrawRoundedRect(dc, RECT{32, 104, client.right - 32, 228}, kElementCornerRadius,
                    ThemeSurfaceColor(), ThemeBorderColor());
    DrawRoundedRect(dc, RECT{32, 250, client.right - 32, 424}, kElementCornerRadius,
                    ThemeSurfaceColor(), ThemeBorderColor());

    DrawSettingsThemeSelector(dc);
    DrawSettingsLanguageSelector(dc);
}

bool DrawSettingsControl(const DRAWITEMSTRUCT& item) {
    HDC dc = item.hDC;
    RECT rect = item.rcItem;

    switch (item.CtlID) {
    case kSettingsStartup: {
        const bool checked = g_startWithWindows;
        FillSolidRect(dc, rect, ThemeSurfaceColor());

        RECT box{0, 1, 22, 23};
        DrawRoundedRect(dc, box, kElementCornerRadius,
                        checked ? ThemeAccentColor() : ThemeSurfaceColor(),
                        checked ? ThemeAccentColor() : ThemeBorderColor());
        if (checked) {
            DrawCheckMark(dc, box, RGB(255, 255, 255));
        }

        wchar_t text[160]{};
        GetWindowTextW(item.hwndItem, text, static_cast<int>(std::size(text)));
        RECT textRect{36, 0, rect.right, rect.bottom};
        DrawTextInRect(dc, text, textRect, g_bodyFont, ThemeTextColor(),
                       DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        return true;
    }

    case kSettingsHotKey: {
        const bool focused = (item.itemState & ODS_FOCUS) != 0;
        FillSolidRect(dc, rect, ThemeSurfaceColor());
        const COLORREF border =
            focused ? ThemeSelectedBorderColor() : ThemeBorderColor();
        DrawRoundedRect(dc, rect, kElementCornerRadius, ThemeInputColor(), border);

        wchar_t text[80]{};
        GetWindowTextW(item.hwndItem, text, static_cast<int>(std::size(text)));
        RECT textRect{rect.left + 16, rect.top, rect.right - 12, rect.bottom};
        DrawTextInRect(dc, text, textRect, g_bodyFont, ThemeTextColor(),
                       DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        return true;
    }

    case kSettingsApplyHotKey:
    case kSettingsExit: {
        const bool pressed = (item.itemState & ODS_SELECTED) != 0;
        FillSolidRect(dc, rect,
                      item.CtlID == kSettingsApplyHotKey ? ThemeSurfaceColor()
                                                         : ThemeWindowColor());
        COLORREF fill = ThemeSurfaceColor();
        COLORREF border = ThemeBorderColor();
        COLORREF textColor = ThemeMutedTextColor();

        if (item.CtlID == kSettingsApplyHotKey) {
            fill = pressed ? RGB(8, 79, 156) : ThemeAccentColor();
            border = fill;
            textColor = RGB(255, 255, 255);
        } else if (item.CtlID == kSettingsExit) {
            fill = pressed ? RGB(254, 226, 226) : RGB(255, 245, 245);
            border = RGB(254, 202, 202);
            textColor = RGB(185, 28, 28);
        } else if (pressed) {
            fill = ThemeSurfaceHoverColor();
        }

        DrawRoundedRect(dc, rect, kElementCornerRadius, fill, border);

        wchar_t text[80]{};
        GetWindowTextW(item.hwndItem, text, static_cast<int>(std::size(text)));
        DrawTextInRect(dc, text, rect, g_bodyFont, textColor,
                       DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        return true;
    }
    }

    return false;
}

void ApplyThemeToOpenWindows() {
    ResetThemeBrushes();
    EnsureUiResources();

    if (g_popupWindow) {
        ApplyWindowVisualStyle(g_popupWindow, true);
        RedrawWindow(g_popupWindow, nullptr, nullptr,
                     RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    }
    if (g_listBox) {
        InvalidateRect(g_listBox, nullptr, TRUE);
    }
    if (g_settingsWindow) {
        ApplyWindowVisualStyle(g_settingsWindow, true);
        RedrawWindow(g_settingsWindow, nullptr, nullptr,
                     RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    }
    if (g_hotKeyControl) {
        InvalidateRect(g_hotKeyControl, nullptr, TRUE);
    }
    if (g_startupCheckBox) {
        InvalidateRect(g_startupCheckBox, nullptr, TRUE);
    }
}

void ApplyLanguageToOpenWindows() {
    UpdateSettingsWindow();
    if (g_popupWindow) {
        RedrawWindow(g_popupWindow, nullptr, nullptr,
                     RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    }
    if (g_listBox) {
        InvalidateRect(g_listBox, nullptr, TRUE);
    }
    if (g_settingsWindow) {
        RedrawWindow(g_settingsWindow, nullptr, nullptr,
                     RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    }
}

bool HandleSettingsThemeClick(POINT point) {
    const RECT lightTheme = SettingsLightThemeRect();
    const RECT darkTheme = SettingsDarkThemeRect();
    if (PtInRect(&lightTheme, point)) {
        if (g_darkThemeSelected) {
            g_darkThemeSelected = false;
            ApplyThemeToOpenWindows();
        }
        return true;
    }
    if (PtInRect(&darkTheme, point)) {
        if (!g_darkThemeSelected) {
            g_darkThemeSelected = true;
            ApplyThemeToOpenWindows();
        }
        return true;
    }
    return false;
}

bool HandleSettingsLanguageClick(POINT point) {
    const RECT englishLanguage = SettingsEnglishLanguageRect();
    const RECT russianLanguage = SettingsRussianLanguageRect();
    if (PtInRect(&englishLanguage, point)) {
        if (!g_englishLanguageSelected) {
            g_englishLanguageSelected = true;
            ApplyLanguageToOpenWindows();
        }
        return true;
    }
    if (PtInRect(&russianLanguage, point)) {
        if (g_englishLanguageSelected) {
            g_englishLanguageSelected = false;
            ApplyLanguageToOpenWindows();
        }
        return true;
    }
    return false;
}

LRESULT CALLBACK SettingsProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_NCCREATE:
        g_settingsWindow = hwnd;
        return TRUE;

    case WM_CREATE: {
        EnsureUiResources();
        ApplyWindowVisualStyle(hwnd, true);
        ApplyRoundedRegion(hwnd, kWindowCornerRadius);
        g_startWithWindows = IsStartupEnabled();
        const auto instance = GetModuleHandleW(nullptr);
        const auto setBodyFont = [](HWND control) {
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_bodyFont),
                         TRUE);
        };
        const auto setTitleFont = [](HWND control) {
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_titleFont),
                         TRUE);
        };

        g_settingsTitleLabel = CreateWindowExW(
            0, L"STATIC", L"MaxB Handy Clipboard",
            WS_CHILD | WS_VISIBLE,
            40, 30, 360, 48, hwnd, nullptr, instance, nullptr);
        setTitleFont(g_settingsTitleLabel);

        g_hotKeyLabel = CreateWindowExW(
            0, L"STATIC", nullptr,
            WS_CHILD | WS_VISIBLE,
            56, 126, 300, 24, hwnd, nullptr, instance, nullptr);
        setBodyFont(g_hotKeyLabel);

        g_hotKeyControl = CreateWindowExW(
            0, kHotKeyInputClass, nullptr,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            56, 160, 224, 38, hwnd,
            reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsHotKey)),
            instance, nullptr);
        setBodyFont(g_hotKeyControl);
        SetHotKeyControlText(g_pendingHotKeyBinding);

        g_applyHotKeyButton = CreateWindowExW(
            0, L"BUTTON", nullptr,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            304, 160, 132, 38, hwnd,
            reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsApplyHotKey)),
            instance, nullptr);
        setBodyFont(g_applyHotKeyButton);

        g_startupCheckBox = CreateWindowExW(
            0, L"BUTTON", nullptr,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            56, 280, 360, 26, hwnd,
            reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsStartup)),
            instance, nullptr);
        setBodyFont(g_startupCheckBox);

        g_themeLabel = CreateWindowExW(
            0, L"STATIC", nullptr,
            WS_CHILD | WS_VISIBLE,
            56, 333, 150, 24, hwnd, nullptr, instance, nullptr);
        setBodyFont(g_themeLabel);

        g_languageLabel = CreateWindowExW(
            0, L"STATIC", nullptr,
            WS_CHILD | WS_VISIBLE,
            56, 381, 150, 24, hwnd, nullptr, instance, nullptr);
        setBodyFont(g_languageLabel);

        g_exitButton = CreateWindowExW(
            0, L"BUTTON", nullptr,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            356, 448, 112, 38, hwnd,
            reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsExit)),
            instance, nullptr);
        setBodyFont(g_exitButton);

        UpdateSettingsWindow();
        return 0;
    }

    case WM_NCCALCSIZE:
        if (wParam) {
            return 0;
        }
        break;

    case WM_NCHITTEST:
        return SettingsHitTest(hwnd, lParam);

    case WM_GETMINMAXINFO: {
        auto* limits = reinterpret_cast<MINMAXINFO*>(lParam);
        limits->ptMinTrackSize.x = kSettingsClientWidth;
        limits->ptMinTrackSize.y = kSettingsClientHeight;
        return 0;
    }

    case WM_SIZE:
        ApplyRoundedRegion(hwnd, kWindowCornerRadius);
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        DrawSettingsChrome(hwnd, dc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DRAWITEM:
        if (auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            DrawSettingsControl(*draw)) {
            return TRUE;
        }
        break;

    case WM_LBUTTONDOWN: {
        POINT point{static_cast<short>(LOWORD(lParam)),
                    static_cast<short>(HIWORD(lParam))};
        if (HandleSettingsThemeClick(point)) {
            return 0;
        }
        if (HandleSettingsLanguageClick(point)) {
            return 0;
        }
        break;
    }

    case WM_LBUTTONUP: {
        POINT point{static_cast<short>(LOWORD(lParam)),
                    static_cast<short>(HIWORD(lParam))};
        RECT client{};
        GetClientRect(hwnd, &client);
        const RECT closeButton = SettingsCloseButtonRect(client);
        if (PtInRect(&closeButton, point)) {
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }

        if (HandleSettingsThemeClick(point)) {
            return 0;
        }
        if (HandleSettingsLanguageClick(point)) {
            return 0;
        }
        break;
    }

    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, ThemeTextColor());
        return reinterpret_cast<LRESULT>(GetStockObject(HOLLOW_BRUSH));
    }

    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkMode(dc, OPAQUE);
        SetBkColor(dc, ThemeInputColor());
        SetTextColor(dc, ThemeTextColor());
        EnsureUiResources();
        return reinterpret_cast<LRESULT>(g_editBrush);
    }

    case WM_CTLCOLORBTN: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, ThemeTextColor());
        return reinterpret_cast<LRESULT>(GetStockObject(HOLLOW_BRUSH));
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case kSettingsStartup:
            g_startWithWindows = !g_startWithWindows;
            if (!SetStartupEnabled(g_startWithWindows)) {
                g_startWithWindows = !g_startWithWindows;
                MessageBoxW(hwnd, L"Не удалось изменить настройку автозапуска.",
                            kAppName, MB_ICONWARNING | MB_OK);
            }
            UpdateSettingsWindow();
            return 0;

        case kSettingsApplyHotKey: {
            ApplyHotKeyBinding(g_pendingHotKeyBinding, true);
            return 0;
        }

        case kSettingsClearHistory:
            g_history.clear();
            if (g_listBox) {
                PopulateListBox();
            }
            UpdateSettingsWindow();
            return 0;

        case kSettingsClose:
            ShowWindow(hwnd, SW_HIDE);
            return 0;

        case kSettingsExit:
            DestroyWindow(g_mainWindow);
            return 0;
        }
        break;

    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK MainProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_NCCREATE:
        g_mainWindow = hwnd;
        return TRUE;

    case WM_CREATE:
        AddClipboardFormatListener(hwnd);
        if (!ApplyHotKeyBinding(g_hotKeyBinding, false)) {
            MessageBoxW(hwnd,
                        L"Не удалось зарегистрировать стандартную комбинацию "
                        L"Ctrl+Shift+V. Выберите другую в настройках.",
                        kAppName, MB_ICONWARNING | MB_OK);
        }
        AddTrayIcon(hwnd);
        CaptureClipboard();
        return 0;

    case WM_HOTKEY:
        if (wParam == kHotKeyId) {
            ToggleHistoryPopup();
            return 0;
        }
        break;

    case WM_CLIPBOARDUPDATE:
        if (g_suppressNextClipboardUpdate) {
            g_suppressNextClipboardUpdate = false;
            return 0;
        }
        CaptureClipboard();
        return 0;

    case WM_TIMER:
        if (wParam == kTimerCaptureRetry) {
            CaptureClipboard();
            return 0;
        }
        break;

    case kTrayMessage:
        if (LOWORD(lParam) == WM_RBUTTONUP) {
            ShowSettingsWindow();
            return 0;
        }
        if (LOWORD(lParam) == WM_LBUTTONUP) {
            ShowHistoryPopup();
            return 0;
        }
        break;

    case WM_DESTROY:
        HidePopup();
        if (g_settingsWindow && IsWindowVisible(g_settingsWindow)) {
            ShowWindow(g_settingsWindow, SW_HIDE);
        }
        RemoveClipboardFormatListener(hwnd);
        StopHotKeyCapture();
        DisableActiveHotKey();
        RemoveTrayIcon(hwnd);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool RegisterWindowClasses(HINSTANCE instance) {
    HICON appIcon = LoadAppIcon(GetSystemMetrics(SM_CXICON),
                                GetSystemMetrics(SM_CYICON));
    HICON smallAppIcon = LoadAppIcon(GetSystemMetrics(SM_CXSMICON),
                                     GetSystemMetrics(SM_CYSMICON));

    WNDCLASSEXW mainClass{};
    mainClass.cbSize = sizeof(mainClass);
    mainClass.lpfnWndProc = MainProc;
    mainClass.hInstance = instance;
    mainClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    mainClass.hIcon = appIcon;
    mainClass.hIconSm = smallAppIcon;
    mainClass.lpszClassName = kMainWindowClass;

    WNDCLASSEXW popupClass{};
    popupClass.cbSize = sizeof(popupClass);
    popupClass.lpfnWndProc = PopupProc;
    popupClass.hInstance = instance;
    popupClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    popupClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    popupClass.hIcon = appIcon;
    popupClass.hIconSm = smallAppIcon;
    popupClass.lpszClassName = kPopupWindowClass;

    WNDCLASSEXW settingsClass{};
    settingsClass.cbSize = sizeof(settingsClass);
    settingsClass.lpfnWndProc = SettingsProc;
    settingsClass.hInstance = instance;
    settingsClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    settingsClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    settingsClass.hIcon = appIcon;
    settingsClass.hIconSm = smallAppIcon;
    settingsClass.lpszClassName = kSettingsWindowClass;

    WNDCLASSEXW hotKeyInputClass{};
    hotKeyInputClass.cbSize = sizeof(hotKeyInputClass);
    hotKeyInputClass.lpfnWndProc = HotKeyControlProc;
    hotKeyInputClass.hInstance = instance;
    hotKeyInputClass.hCursor = LoadCursorW(nullptr, IDC_IBEAM);
    hotKeyInputClass.hbrBackground = nullptr;
    hotKeyInputClass.lpszClassName = kHotKeyInputClass;

    return RegisterClassExW(&mainClass) && RegisterClassExW(&popupClass) &&
           RegisterClassExW(&settingsClass) && RegisterClassExW(&hotKeyInputClass);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    g_instanceMutex = CreateMutexW(nullptr, TRUE, L"Local\\MaxBHandyClipboard.SingleInstance");
    if (!g_instanceMutex) {
        MessageBoxW(nullptr, L"Не удалось создать mutex приложения.", kAppName,
                    MB_ICONERROR | MB_OK);
        return 1;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"MaxB Handy Clipboard уже запущен.", kAppName,
                    MB_ICONINFORMATION | MB_OK);
        CloseHandle(g_instanceMutex);
        return 0;
    }

    if (!RegisterWindowClasses(instance)) {
        MessageBoxW(nullptr, L"Не удалось зарегистрировать окна приложения.", kAppName,
                    MB_ICONERROR | MB_OK);
        CloseHandle(g_instanceMutex);
        return 1;
    }

    g_mainWindow = CreateWindowExW(
        0, kMainWindowClass, kAppName, WS_OVERLAPPED,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        nullptr, nullptr, instance, nullptr);

    g_popupWindow = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW, kPopupWindowClass, kAppName,
        WS_POPUP | WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, kPopupWidth, 360,
        nullptr, nullptr, instance, nullptr);

    constexpr DWORD settingsStyle = WS_POPUP | WS_THICKFRAME;
    constexpr DWORD settingsExStyle = WS_EX_TOOLWINDOW;
    RECT settingsRect{0, 0, kSettingsClientWidth, kSettingsClientHeight};

    g_settingsWindow = CreateWindowExW(
        settingsExStyle, kSettingsWindowClass, L"MaxB Handy Clipboard Settings",
        settingsStyle,
        CW_USEDEFAULT, CW_USEDEFAULT, RectWidth(settingsRect),
        RectHeight(settingsRect),
        nullptr, nullptr, instance, nullptr);

    if (!g_mainWindow || !g_popupWindow || !g_settingsWindow) {
        MessageBoxW(nullptr, L"Не удалось создать окна приложения.", kAppName,
                    MB_ICONERROR | MB_OK);
        CloseHandle(g_instanceMutex);
        return 1;
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    const int exitCode = static_cast<int>(message.wParam);
    CleanupUiResources();
    CloseHandle(g_instanceMutex);
    return exitCode;
}

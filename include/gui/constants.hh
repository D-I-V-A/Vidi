#pragma once
#ifndef GUI_CONSTANTS_HH
#define GUI_CONSTANTS_HH

#include <windows.h>

namespace guiVidi {
// inline warna
inline constexpr COLORREF COLOR_MODERN_BG = RGB(255, 255, 255);
inline constexpr COLORREF COLOR_MODERN_PRIMARY = RGB(0, 120, 212);
inline constexpr COLORREF COLOR_MODERN_TEXT = RGB(50, 50, 50);
inline constexpr COLORREF COLOR_SEEK_TRACK = RGB(224, 224, 224);
inline constexpr COLORREF COLOR_SEEK_FILL = RGB(255, 140, 0);
inline constexpr COLORREF COLOR_SEEK_FILL_HOT = RGB(255, 170, 51);
inline constexpr COLORREF COLOR_TIP_BG = RGB(30, 30, 30);

// inline dimensi
inline constexpr int VOL_MAX = 150;
inline constexpr int MAX_SUB_OVERLAYS = 2;

// timer
inline constexpr UINT TIMER_INTERVAL_MS = 33;
inline constexpr int FULLSCREEN_HIDE_MS = 2500;

inline double GetDpiScale(HWND hwnd) {
    UINT dpi = GetDpiForWindow(hwnd);
    return (double)dpi / 96.0;
}

inline int MeasureStringWidth(HWND hwndRef, HFONT hFont, const wchar_t* text) {
    if (!hwndRef || !text || !*text)
        return 60;

    HDC hdc = GetDC(hwndRef);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont ? hFont : (HFONT)GetStockObject(DEFAULT_GUI_FONT));

    SIZE sz = {0};
    GetTextExtentPoint32W(hdc, text, (int)wcslen(text), &sz);

    SelectObject(hdc, hOldFont);
    ReleaseDC(hwndRef, hdc);

    int w = sz.cx + 8;
    return (w < 60) ? 60 : w;
}

inline int CurrentTimeLabelWidth(HWND hLabel, HFONT hFont) {
    wchar_t buf[64] = {};
    GetWindowTextW(hLabel, buf, 64);
    return MeasureStringWidth(hLabel, hFont, buf);
}

inline COLORREF LerpColor(COLORREF a, COLORREF b, double t) {
    if (t < 0)
        t = 0;
    if (t > 1)
        t = 1;
    return RGB((int)(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t),
               (int)(GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t),
               (int)(GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t));
}

} // namespace guiVidi

#endif
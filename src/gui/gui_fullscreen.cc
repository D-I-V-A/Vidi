#include "../../include/gui/gui.hh"
#include "../../include/kernels/ids.hh"

namespace guiVidi {

// ==========================================
// ENTER FULLSCREEN
// ==========================================
void VideoPlayerGUI::EnterFullscreen() {
    OutputDebugStringW(L"[VIDI] EnterFullscreen START\n");

    if (m_isFullscreen) {
        OutputDebugStringW(L"[VIDI] EnterFullscreen ALREADY fullscreen, abort\n");
        return;
    }
    MONITORINFO mi = {sizeof(mi)};
    HMONITOR mon = MonitorFromWindow(g_hMainWnd, MONITOR_DEFAULTTONEAREST);
    if (!GetWindowPlacement(g_hMainWnd, &m_prevPlacement) || !GetMonitorInfo(mon, &mi)) {
        OutputDebugStringW(L"[VIDI] EnterFullscreen FAILED to get monitor info\n");
        return;
    }
    int monW = mi.rcMonitor.right - mi.rcMonitor.left;
    int monH = mi.rcMonitor.bottom - mi.rcMonitor.top;
    {
        wchar_t dbg[256];
        swprintf_s(dbg, L"[VIDI] EnterFullscreen monitor=%dx%d pos=(%d,%d)\n", monW, monH, mi.rcMonitor.left,
                   mi.rcMonitor.top);
        OutputDebugStringW(dbg);
    }

    m_isFullscreen = true;
    SendMessage(g_hMainWnd, WM_SETREDRAW, FALSE, 0);

    DWORD style = GetWindowLong(g_hMainWnd, GWL_STYLE);
    {
        wchar_t dbg[256];
        swprintf_s(dbg, L"[VIDI] EnterFullscreen oldStyle=0x%08X changing to WS_POPUP\n", style);
        OutputDebugStringW(dbg);
    }
    SetWindowLong(g_hMainWnd, GWL_STYLE, (style & ~WS_OVERLAPPEDWINDOW) | WS_POPUP);
    if (m_hMenuBar)
        SetMenu(g_hMainWnd, nullptr);

    SetWindowPos(g_hMainWnd, HWND_TOPMOST, mi.rcMonitor.left, mi.rcMonitor.top, monW, monH,
                 SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

    SendMessage(g_hMainWnd, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(g_hMainWnd, nullptr, nullptr,
                 RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);
    RECT rc;
    GetClientRect(g_hMainWnd, &rc);
    {
        wchar_t dbg[256];
        swprintf_s(dbg, L"[VIDI] EnterFullscreen AFTER resize client=%dx%d (monitor=%dx%d)\n", rc.right, rc.bottom,
                   monW, monH);
        OutputDebugStringW(dbg);
    }
    LayoutFullscreen(monW, monH);
    m_player.UpdateVideoSize();
    RecoverVideo();
    PokeOSControls();
    OutputDebugStringW(L"[VIDI] EnterFullscreen END\n");
}

// ==========================================
// EXIT FULLSCREEN
// ==========================================
void VideoPlayerGUI::ExitFullscreen() {
    OutputDebugStringW(L"[VIDI] === ExitFullscreen START ===\n");
    if (!m_isFullscreen)
        return;

    m_isFullscreen = false;
    SendMessage(g_hMainWnd, WM_SETREDRAW, FALSE, 0);

    DWORD style = GetWindowLong(g_hMainWnd, GWL_STYLE);
    SetWindowLong(g_hMainWnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
    if (m_hMenuBar)
        SetMenu(g_hMainWnd, m_hMenuBar);
    SetWindowPlacement(g_hMainWnd, &m_prevPlacement);

    SetWindowPos(g_hMainWnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

    KillTimer(g_hMainWnd, ID_TIMER_OSI_HIDE);
    ShowOSControls(true);
    if (m_cursorHidden) {
        ShowCursor(TRUE);
        m_cursorHidden = false;
    }

    SendMessage(g_hMainWnd, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(g_hMainWnd, nullptr, nullptr,
                 RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);

    RECT rc;
    GetClientRect(g_hMainWnd, &rc);
    {
        wchar_t dbg[256];
        swprintf_s(dbg, L"[VIDI] ExitFullscreen client=%dx%d\n", rc.right, rc.bottom);
        OutputDebugStringW(dbg);
    }
    LayoutControls(rc.right, rc.bottom);
    m_player.UpdateVideoSize();
    RecoverVideo();
    m_lastVideoClickTick = 0;
    OutputDebugStringW(L"[VIDI] === ExitFullscreen END ===\n");
}

// ==========================================
// FIT WINDOW TO VIDEO
// ==========================================
void VideoPlayerGUI::FitWindowToVideo() {
    if (m_isFullscreen || !g_hMainWnd)
        return;

    int vw = 0, vh = 0;
    m_player.GetNativeVideoSize(vw, vh);
    if (vw <= 0 || vh <= 0)
        return;

    HMONITOR mon = MonitorFromWindow(g_hMainWnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {sizeof(mi)};
    if (!GetMonitorInfo(mon, &mi))
        return;
    int availW = mi.rcWork.right - mi.rcWork.left;
    int availH = mi.rcWork.bottom - mi.rcWork.top;

    double scaleX = ((double)availW * 0.9) / vw;
    double scaleY = ((double)availH * 0.9) / vh;
    double scale = (scaleX < scaleY) ? scaleX : scaleY;
    if (scale > 1.0)
        scale = 1.0;
    int cw = (int)(vw * scale + 0.5);
    int ch = (int)(vh * scale + 0.5);

    RECT rcC, rcW;
    GetClientRect(g_hMainWnd, &rcC);
    GetWindowRect(g_hMainWnd, &rcW);
    int extraW = (rcW.right - rcW.left) - rcC.right;
    int extraH = (rcW.bottom - rcW.top) - rcC.bottom;

    int newX = mi.rcWork.left + (availW - (cw + extraW)) / 2;
    int newY = mi.rcWork.top + (availH - (ch + extraH)) / 2;
    SetWindowPos(g_hMainWnd, nullptr, newX, newY, cw + extraW, ch + extraH, SWP_NOZORDER);
}

// ==========================================
// SHOW/HIDE OS CONTROLS (fullscreen overlay)
// ==========================================
void VideoPlayerGUI::ShowOSControls(bool visible) {
    int cmd = visible ? SW_SHOW : SW_HIDE;
    HWND ctrls[] = {g_hProgress,      g_hSkipBack,    g_hPlayBtn,  g_hStopBtn,    g_hSkipForward,
                    g_hFullscreenBtn, g_hPlaylistBtn, g_hLoopBtn,  g_hShuffleBtn, g_hVolIcon,
                    g_hVolume,        g_hVolPercent,  g_hTimeLabel};
    for (HWND h : ctrls)
        if (h)
            ShowWindow(h, cmd);
}

void VideoPlayerGUI::PokeOSControls() {
    if (!m_isFullscreen || !g_hMainWnd)
        return;

    if (m_cursorHidden) {
        ShowCursor(TRUE);
        m_cursorHidden = false;
    }
    ShowOSControls(true);
    SetTimer(g_hMainWnd, ID_TIMER_OSI_HIDE, FULLSCREEN_HIDE_MS, nullptr);
}

bool VideoPlayerGUI::CursorOverControls() {
    POINT pt;
    if (!GetCursorPos(&pt))
        return false;
    const HWND ctrls[] = {g_hProgress,      g_hSkipBack,    g_hPlayBtn,  g_hStopBtn,    g_hSkipForward,
                          g_hFullscreenBtn, g_hPlaylistBtn, g_hLoopBtn,  g_hShuffleBtn, g_hVolIcon,
                          g_hVolume,        g_hVolPercent,  g_hTimeLabel};
    for (HWND h : ctrls) {
        if (!h || !IsWindowVisible(h))
            continue;
        RECT rc;
        if (!GetWindowRect(h, &rc))
            continue;
        if (PtInRect(&rc, pt))
            return true;
    }
    return false;
}

// ==========================================
// RECOVER VIDEO — pulihkan tampilan setelah session switch/minimize/ganti resolusi
// ==========================================
void VideoPlayerGUI::RecoverVideo() {
    if (m_isPlaying) {
        m_player.Play();
    } else if (m_cachedDuration > 0.0) {
        m_player.ForceFrameRefresh();
    }
}

} // namespace guiVidi

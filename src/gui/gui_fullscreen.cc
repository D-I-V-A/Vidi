#include "../../include/gui/gui.hh"
#include "../../include/kernels/ids.hh"

namespace guiVidi {

void VideoPlayerGUI::EnterFullscreen() {
    // fungsi ini merupakan logic dari bagaimana video player
    // melakukan fullscreen
    OutputDebugStringW(L"[VIDI] EnterFullscreen START\n");

    if (m_isFullscreen) {
        OutputDebugStringW(L"[VIDI] EnterFullscreen ALREADY fullscreen, abort\n");

        return;
    }

    MONITORINFO mi = {sizeof(mi)};

    HMONITOR mon = MonitorFromWindow(g_hMainWnd, MONITOR_DEFAULTTONEAREST);

    if (!GetWindowPlacement(g_hMainWnd, &m_prevPlacement) || !GetMonitorInfo(mon, &mi)) {
        OutputDebugStringW(L"[VIDI] EnterFullscreen FAILED "
                           L"to get monitor info\n");

        return;
    }

    const int monW = mi.rcMonitor.right - mi.rcMonitor.left;

    const int monH = mi.rcMonitor.bottom - mi.rcMonitor.top;

    m_isFullscreen = true;

    SendMessage(g_hMainWnd, WM_SETREDRAW, FALSE, 0);

    DWORD style = GetWindowLong(g_hMainWnd, GWL_STYLE);

    SetWindowLong(g_hMainWnd, GWL_STYLE, (style & ~WS_OVERLAPPEDWINDOW) | WS_POPUP);

    DWORD exStyle = GetWindowLong(g_hMainWnd, GWL_EXSTYLE);

    SetWindowLong(g_hMainWnd, GWL_EXSTYLE, exStyle | WS_EX_APPWINDOW);

    if (m_hMenuBar)
        SetMenu(g_hMainWnd, nullptr);

    SetWindowPos(g_hMainWnd, HWND_TOPMOST, mi.rcMonitor.left, mi.rcMonitor.top, monW, monH,
                 SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

    for (int i = 0; i < MAX_SUB_OVERLAYS; ++i) {
        if (m_hSubOverlay[i]) {

            SetWindowPos(m_hSubOverlay[i], HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }

    SendMessage(g_hMainWnd, WM_SETREDRAW, TRUE, 0);

    RedrawWindow(g_hMainWnd, nullptr, nullptr,
                 RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);

    // ========================================================
    // Layout fullscreen
    // ========================================================

    LayoutFullscreen(monW, monH);

    // ========================================================
    // mengupdate ukuran video.
    // ========================================================

    m_player.UpdateVideoSize();

    // ========================================================
    // FORCE subtitle geometry refresh
    // ========================================================

    m_lastSubFrameW = 0;
    m_lastSubFrameH = 0;

    m_lastSubOverlayX = 0;
    m_lastSubOverlayY = 0;

    m_lastSubContentHash = 0;

    m_lastSubRenderTick = 0;

    // ========================================================
    // IMPORTANT
    //
    // Subtitle dipaksa update SEKARANG.
    //
    // Tidak menunggu video frame baru.
    // ========================================================

    if (!m_subsHidden) {

        UpdateSubtitleDisplays(m_lastSubPosition, true);
    }

    // ========================================================
    // Baru recover video
    // ========================================================

    RecoverVideo();
    UpdateSubtitleDisplays(m_lastSubPosition, true);
    PokeOSControls();

    OutputDebugStringW(L"[VIDI] EnterFullscreen END\n");
}

void VideoPlayerGUI::ExitFullscreen() {
    OutputDebugStringW(L"[VIDI] === ExitFullscreen START ===\n");

    if (!m_isFullscreen)
        return;

    m_isFullscreen = false;

    SendMessage(g_hMainWnd, WM_SETREDRAW, FALSE, 0);

    DWORD style = GetWindowLong(g_hMainWnd, GWL_STYLE);

    SetWindowLong(g_hMainWnd, GWL_STYLE, (style & ~WS_POPUP) | WS_OVERLAPPEDWINDOW);

    DWORD exStyle = GetWindowLong(g_hMainWnd, GWL_EXSTYLE);

    SetWindowLong(g_hMainWnd, GWL_EXSTYLE, exStyle & ~WS_EX_APPWINDOW);

    if (m_hMenuBar)
        SetMenu(g_hMainWnd, m_hMenuBar);

    if (!SetWindowPlacement(g_hMainWnd, &m_prevPlacement)) {
        HMONITOR mon = MonitorFromWindow(g_hMainWnd, MONITOR_DEFAULTTONEAREST);

        MONITORINFO mi = {sizeof(mi)};

        if (GetMonitorInfo(mon, &mi)) {
            RECT r = mi.rcWork;

            SetWindowPos(g_hMainWnd, nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top,
                         SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        }
    }

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

    RECT rc = {};

    GetClientRect(g_hMainWnd, &rc);

    LayoutControls(rc.right, rc.bottom);

    m_player.UpdateVideoSize();

    // ========================================================
    // FORCE subtitle geometry refresh
    // ========================================================

    m_lastSubFrameW = 0;
    m_lastSubFrameH = 0;

    m_lastSubOverlayX = 0;
    m_lastSubOverlayY = 0;

    m_lastSubContentHash = 0;

    m_lastSubRenderTick = 0;

    // ========================================================
    // FORCE immediate subtitle update
    // ========================================================

    if (!m_subsHidden) {

        UpdateSubtitleDisplays(m_lastSubPosition, true);
    }

    // ========================================================
    // Recover video AFTER subtitle geometry is ready
    // ========================================================

    RecoverVideo();

    m_lastVideoClickTick = 0;

    OutputDebugStringW(L"[VIDI] === ExitFullscreen END ===\n");
}
// ==========================================
// FIT WINDOW TO VIDEO
// ==========================================
void VideoPlayerGUI::FitWindowToVideo() {
    // fungsi untuk kondisi ukuran window pada video player sesuai dengan ukuran video player
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

void VideoPlayerGUI::RecoverVideo() {
    // fungsi dimana berlogic pulihkan tampilan setelah session
    // seperti switch/minimize maupun ganti resolusi
    if (m_isPlaying) {
        m_player.Play();
    } else if (m_cachedDuration > 0.0) {
        m_player.ForceFrameRefresh();
    }
}

} // namespace guiVidi

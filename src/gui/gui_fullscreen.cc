#include "../../include/gui/gui.hh"
#include "../../include/gui/constants.hh"
#include "../../include/kernels/ids.hh"

namespace guiVidi {

// ============================================================
// HELPERS (internal linkage — hanya file ini)
// ============================================================
namespace {

// Kumpulkan semua child control yang perlu hide/show saat toggle FS.
constexpr int kFsControlCount = 13;

// Batch hide/show N window dalam SATU DeferWindowPos round-trip.
// Jauh lebih cepat dari loop ShowWindow() karena OS commit sekali.
void BatchShowWindows(HWND* list, int count, bool show) {
    if (count <= 0)
        return;

    HDWP hdwp = BeginDeferWindowPos(count);
    if (!hdwp) {
        for (int i = 0; i < count; ++i)
            if (list[i])
                ShowWindow(list[i], show ? SW_SHOW : SW_HIDE);
        return;
    }

    for (int i = 0; i < count; ++i) {
        if (!list[i])
            continue;
        hdwp = DeferWindowPos(hdwp, list[i], nullptr, 0, 0, 0, 0,
                              SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                                  (show ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
    }
    if (hdwp)
        EndDeferWindowPos(hdwp);
}

} // namespace

// ============================================================
// COLLECT FULLSCREEN CONTROLS
// Isi `out` dengan daftar HWND kontrol yang perlu di-hide saat FS.
// Satu sumber kebenaran, dipakai Enter & Exit.
// ============================================================
void VideoPlayerGUI::CollectFullscreenControls(HWND* out, int& count) const {
    HWND list[kFsControlCount] = {
        g_hProgress, g_hSkipBack,   g_hPlayBtn, g_hStopBtn, g_hSkipForward, g_hFullscreenBtn, g_hPlaylistBtn,
        g_hLoopBtn,  g_hShuffleBtn, g_hVolIcon, g_hVolume,  g_hVolPercent,  g_hTimeLabel,
    };
    count = kFsControlCount;
    for (int i = 0; i < count; ++i)
        out[i] = list[i];
}

// ============================================================
// ENTER FULLSCREEN
// ============================================================
void VideoPlayerGUI::EnterFullscreen() {
    if (m_isFullscreen)
        return;

    MONITORINFO mi = {sizeof(mi)};
    HMONITOR mon = MonitorFromWindow(g_hMainWnd, MONITOR_DEFAULTTONEAREST);
    if (!GetWindowPlacement(g_hMainWnd, &m_prevPlacement) || !GetMonitorInfo(mon, &mi))
        return;

    const int monW = mi.rcMonitor.right - mi.rcMonitor.left;
    const int monH = mi.rcMonitor.bottom - mi.rcMonitor.top;

    // 1) Freeze paint utama. JANGAN UpdateWindow / Invalidate —
    //    biarkan DWM composite frame terakhir (no flash putih).
    SetWindowRedraw(g_hMainWnd, FALSE);
    if (g_hVideoArea)
        SetWindowRedraw(g_hVideoArea, FALSE);

    m_isFullscreen = true;

    // 2) Hide semua kontrol pakai SATU batch DeferWindowPos
    HWND ctrls[kFsControlCount] = {};
    int nCtrls = 0;
    CollectFullscreenControls(ctrls, nCtrls);
    BatchShowWindows(ctrls, nCtrls, false);

    // 3) Buat overlay DULU, supaya bisa langsung di-layout dalam frame yang sama
    if (!m_hFsOverlay)
        CreateFsOverlay();

    // 4) Style change — satu commit.
    LONG_PTR style = GetWindowLongPtrW(g_hMainWnd, GWL_STYLE);
    LONG_PTR exStyle = GetWindowLongPtrW(g_hMainWnd, GWL_EXSTYLE);
    SetWindowLongPtrW(g_hMainWnd, GWL_STYLE, (style & ~WS_OVERLAPPEDWINDOW) | WS_POPUP);
    SetWindowLongPtrW(g_hMainWnd, GWL_EXSTYLE, exStyle | WS_EX_APPWINDOW);
    if (m_hMenuBar)
        SetMenu(g_hMainWnd, nullptr);

    SetWindowPos(g_hMainWnd, HWND_TOPMOST, mi.rcMonitor.left, mi.rcMonitor.top, monW, monH,
                 SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_NOREDRAW);

    // 5) Resize video area + overlay dalam SATU batch
    {
        HDWP hdwp = BeginDeferWindowPos(2);
        if (g_hVideoArea)
            hdwp = DeferWindowPos(hdwp, g_hVideoArea, HWND_BOTTOM, 0, 0, monW, monH,
                                  SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_NOREDRAW);
        if (m_hFsOverlay)
            hdwp = DeferWindowPos(hdwp, m_hFsOverlay, HWND_TOPMOST, 0, 0, 0, 0,
                                  SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_NOREDRAW);
        if (hdwp)
            EndDeferWindowPos(hdwp);
    }
    LayoutFsOverlay(monW, monH);

    // 6) Video renderer resize ke area baru
    m_player.UpdateVideoSize();

    // 7) Reset cache subtitle geometry — refresh-nya nanti via PostMessage
    m_lastSubFrameW = m_lastSubFrameH = 0;
    m_lastSubOverlayX = m_lastSubOverlayY = 0;
    m_lastSubContentHash = 0;
    m_lastSubRenderTick = 0;

    // 8) Unfreeze SEKALI. Invalidate async (TANPA RDW_UPDATENOW).
    SetWindowRedraw(g_hMainWnd, TRUE);
    if (g_hVideoArea)
        SetWindowRedraw(g_hVideoArea, TRUE);

    InvalidateRect(g_hMainWnd, nullptr, FALSE);
    if (g_hVideoArea)
        InvalidateRect(g_hVideoArea, nullptr, TRUE);

    // 9) Kerja berat (subtitle rebuild + force frame refresh) di-defer.
    //    PostMessage antri di message queue → transisi terasa instan.
    PostMessage(g_hMainWnd, WM_APP_FS_REFRESH, TRUE, 0);

    // 10) Cursor + overlay auto-hide
    PokeOSControls();
}

// ============================================================
// EXIT FULLSCREEN
// ============================================================
void VideoPlayerGUI::ExitFullscreen() {
    if (!m_isFullscreen)
        return;

    m_isFullscreen = false;

    // 1) Freeze — jangan paint dulu
    SetWindowRedraw(g_hMainWnd, FALSE);
    if (g_hVideoArea)
        SetWindowRedraw(g_hVideoArea, FALSE);

    // 2) Sembunyikan overlay & subtitle (batch)
    if (m_hFsOverlay)
        ShowWindow(m_hFsOverlay, SW_HIDE);

    HWND subOverlays[MAX_SUB_OVERLAYS] = {};
    for (int i = 0; i < MAX_SUB_OVERLAYS; ++i)
        subOverlays[i] = m_hSubOverlay[i];
    BatchShowWindows(subOverlays, MAX_SUB_OVERLAYS, false);

    // 3) Kill auto-hide timer + restore cursor
    KillTimer(g_hMainWnd, ID_TIMER_OSI_HIDE);
    if (m_cursorHidden) {
        ShowCursor(TRUE);
        m_cursorHidden = false;
    }

    // 4) Restore style — satu commit
    LONG_PTR style = GetWindowLongPtrW(g_hMainWnd, GWL_STYLE);
    LONG_PTR exStyle = GetWindowLongPtrW(g_hMainWnd, GWL_EXSTYLE);
    SetWindowLongPtrW(g_hMainWnd, GWL_STYLE, (style & ~WS_POPUP) | WS_OVERLAPPEDWINDOW);
    SetWindowLongPtrW(g_hMainWnd, GWL_EXSTYLE, exStyle & ~WS_EX_APPWINDOW);
    if (m_hMenuBar)
        SetMenu(g_hMainWnd, m_hMenuBar);

    // 5) Restore placement
    if (!SetWindowPlacement(g_hMainWnd, &m_prevPlacement)) {
        HMONITOR mon = MonitorFromWindow(g_hMainWnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = {sizeof(mi)};
        if (GetMonitorInfo(mon, &mi)) {
            RECT r = mi.rcWork;
            SetWindowPos(g_hMainWnd, nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top,
                         SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOREDRAW);
        }
    }
    SetWindowPos(g_hMainWnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_NOREDRAW);

    // 6) Re-apply theme (kadang trackbar lupa theme setelah style change)
    if (g_hProgress)
        SetWindowTheme(g_hProgress, L" ", L" ");
    if (g_hVolume)
        SetWindowTheme(g_hVolume, L" ", L" ");

    // 7) Layout + show controls (batch)
    RECT rc = {};
    GetClientRect(g_hMainWnd, &rc);
    LayoutControls(rc.right, rc.bottom);

    HWND ctrls[kFsControlCount] = {};
    int nCtrls = 0;
    CollectFullscreenControls(ctrls, nCtrls);
    BatchShowWindows(ctrls, nCtrls, true);

    // 8) Reset cache subtitle geometry
    m_lastSubFrameW = m_lastSubFrameH = 0;
    m_lastSubOverlayX = m_lastSubOverlayY = 0;
    m_lastSubContentHash = 0;
    m_lastSubRenderTick = 0;
    m_lastVideoClickTick = 0;

    // 9) Video renderer resize
    m_player.UpdateVideoSize();

    // 10) Unfreeze + async invalidate
    SetWindowRedraw(g_hMainWnd, TRUE);
    if (g_hVideoArea)
        SetWindowRedraw(g_hVideoArea, TRUE);

    InvalidateRect(g_hMainWnd, nullptr, FALSE);

    // 11) Defer subtitle + video refresh
    PostMessage(g_hMainWnd, WM_APP_FS_REFRESH, FALSE, 0);
}

// ============================================================
// FIT WINDOW TO VIDEO
// ============================================================
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

// ============================================================
// SHOW/HIDE OS CONTROLS
// ============================================================
void VideoPlayerGUI::ShowOSControls(bool visible) {
    ShowFsOverlay(visible);
}

void VideoPlayerGUI::PokeOSControls() {
    if (!m_isFullscreen || !g_hMainWnd)
        return;

    if (m_cursorHidden) {
        ShowCursor(TRUE);
        m_cursorHidden = false;
    }

    if (!m_hFsOverlay) {
        MONITORINFO mi = {sizeof(mi)};
        HMONITOR mon = MonitorFromWindow(g_hMainWnd, MONITOR_DEFAULTTONEAREST);
        if (GetMonitorInfo(mon, &mi)) {
            int monW = mi.rcMonitor.right - mi.rcMonitor.left;
            int monH = mi.rcMonitor.bottom - mi.rcMonitor.top;
            CreateFsOverlay();
            LayoutFsOverlay(monW, monH);
        }
    } else {
        ShowWindow(m_hFsOverlay, SW_SHOW);
    }

    SetTimer(g_hMainWnd, ID_TIMER_OSI_HIDE, FULLSCREEN_HIDE_MS, nullptr);
}

bool VideoPlayerGUI::CursorOverControls() {
    return CursorOverFsOverlay();
}

// ============================================================
// RECOVER VIDEO
// ============================================================
void VideoPlayerGUI::RecoverVideo() {
    if (m_isPlaying) {
        m_player.Play();
    } else if (m_cachedDuration > 0.0) {
        m_player.ForceFrameRefresh();
    }
}

// ============================================================
// FULLSCREEN OVERLAY
// ============================================================
static const wchar_t FS_OVERLAY_CLASS[] = L"VidiFsOverlay";

void VideoPlayerGUI::CreateFsOverlay() {
    if (m_hFsOverlay)
        return;

    HINSTANCE hInst = GetModuleHandle(nullptr);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = VideoPlayerGUI::FsOverlayWndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = FS_OVERLAY_CLASS;
    wc.hbrBackground = CreateSolidBrush(RGB(20, 20, 20));
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    static bool s_classRegistered = false;
    if (!s_classRegistered) {
        RegisterClassExW(&wc);
        s_classRegistered = true;
    }

    m_hFsOverlay = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, FS_OVERLAY_CLASS, L"", WS_POPUP,
                                   0, 0, 0, 0, g_hMainWnd, nullptr, hInst, this);

    if (m_hFsOverlay) {
        SetWindowLongPtr(m_hFsOverlay, GWLP_USERDATA, (LONG_PTR)this);
    }
}

void VideoPlayerGUI::DestroyFsOverlay() {
    if (m_hFsOverlay) {
        DestroyWindow(m_hFsOverlay);
        m_hFsOverlay = nullptr;
    }
}

void VideoPlayerGUI::ShowFsOverlay(bool visible) {
    if (!m_hFsOverlay)
        return;
    ShowWindow(m_hFsOverlay, visible ? SW_SHOW : SW_HIDE);
}

void VideoPlayerGUI::LayoutFsOverlay(int screenW, int screenH) {
    if (!m_hFsOverlay)
        return;

    double dpi = GetDpiScale(m_hFsOverlay);

    const int PROGRESS_ROW_H = (int)(FS_PROGRESS_ROW_H * dpi);
    const int CONTROLS_ROW_H = (int)(FS_CONTROLS_ROW_H * dpi);
    const int BAR_H = PROGRESS_ROW_H + CONTROLS_ROW_H;

    int barW = (int)(screenW * 0.55);
    if (barW < (int)(360 * dpi))
        barW = (int)(360 * dpi);

    int barX = (screenW - barW) / 2;
    int barY = screenH - BAR_H;

    SetWindowPos(m_hFsOverlay, HWND_TOPMOST, barX, barY, barW, BAR_H, SWP_NOACTIVATE | SWP_SHOWWINDOW);

    InvalidateRect(m_hFsOverlay, nullptr, FALSE);
}

bool VideoPlayerGUI::CursorOverFsOverlay() {
    if (!m_hFsOverlay || !IsWindowVisible(m_hFsOverlay))
        return false;

    POINT pt;
    if (!GetCursorPos(&pt))
        return false;

    RECT rc;
    GetWindowRect(m_hFsOverlay, &rc);
    return PtInRect(&rc, pt);
}

// ============================================================
// OVERLAY WINDOW PROC
// ============================================================
LRESULT CALLBACK VideoPlayerGUI::FsOverlayWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    VideoPlayerGUI* self = nullptr;

    if (uMsg == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        self = reinterpret_cast<VideoPlayerGUI*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)self);
        return 0;
    }

    self = reinterpret_cast<VideoPlayerGUI*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (!self)
        return DefWindowProc(hwnd, uMsg, wParam, lParam);

    switch (uMsg) {

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rcWnd;
        GetClientRect(hwnd, &rcWnd);
        int wndW = rcWnd.right - rcWnd.left;
        int wndH = rcWnd.bottom - rcWnd.top;

        HDC dcMem = CreateCompatibleDC(hdc);
        HBITMAP hBmp = CreateCompatibleBitmap(hdc, wndW, wndH);
        HGDIOBJ hOldBmp = SelectObject(dcMem, hBmp);

        double dpi = GetDpiScale(hwnd);

        HBRUSH hBg = CreateSolidBrush(guiVidi::COLOR_MODERN_BG);
        FillRect(dcMem, &rcWnd, hBg);
        DeleteObject(hBg);

        const int PROGRESS_H = (int)(5 * dpi);
        const int EDGE = (int)(12 * dpi);
        const int BTN_SIZE = (int)(26 * dpi);
        const int SP = (int)(8 * dpi);
        const int PROGRESS_ROW_H = PROGRESS_H + (int)(2 * dpi);
        const int CONTROLS_ROW_H = wndH - PROGRESS_ROW_H;

        // ---- ROW 1: Progress bar ----
        int pos = (int)SendMessage(self->g_hProgress, TBM_GETPOS, 0, 0);
        double ratio = (self->m_progressRangeMax > 0) ? (double)pos / self->m_progressRangeMax : 0.0;
        if (ratio < 0)
            ratio = 0;
        if (ratio > 1)
            ratio = 1;

        int TOP_PAD = (int)(8 * dpi);
        int progCy = TOP_PAD + PROGRESS_H / 2;
        RECT pTrack = {EDGE, progCy - PROGRESS_H / 2, wndW - EDGE, progCy + PROGRESS_H / 2};

        HPEN hNullPen = CreatePen(PS_NULL, 0, 0);
        HBRUSH hPTrack = CreateSolidBrush(COLOR_SEEK_TRACK);
        HGDIOBJ hOldPenP = SelectObject(dcMem, hNullPen);
        HGDIOBJ hOldBrP = SelectObject(dcMem, hPTrack);
        RoundRect(dcMem, pTrack.left, pTrack.top, pTrack.right, pTrack.bottom, PROGRESS_H, PROGRESS_H);

        int progTrackW = pTrack.right - pTrack.left;
        int pFillX = pTrack.left + (int)(ratio * progTrackW);
        if (pFillX > pTrack.left + PROGRESS_H) {
            HBRUSH hPFill = CreateSolidBrush(COLOR_SEEK_FILL);
            HGDIOBJ hPrevPF = SelectObject(dcMem, hPFill);
            RoundRect(dcMem, pTrack.left, pTrack.top + 1, pFillX, pTrack.bottom - 1, PROGRESS_H - 2, PROGRESS_H - 2);
            SelectObject(dcMem, hPrevPF);
            DeleteObject(hPFill);
        }

        int thumbSize = (int)(12 * dpi);
        int thumbR = thumbSize / 2;
        int thumbCx = pFillX;
        int thumbCy = progCy;
        if (thumbCx - thumbR < pTrack.left)
            thumbCx = pTrack.left + thumbR;
        if (thumbCx + thumbR > pTrack.right)
            thumbCx = pTrack.right - thumbR;

        HBRUSH hPThumb = CreateSolidBrush(COLOR_MODERN_BG);
        HPEN hPThumbBorder = CreatePen(PS_SOLID, 1, RGB(80, 80, 80));
        HGDIOBJ hOldBrThumb = SelectObject(dcMem, hPThumb);
        HGDIOBJ hOldPenThumb = SelectObject(dcMem, hPThumbBorder);
        Rectangle(dcMem, thumbCx - thumbR, thumbCy - thumbR, thumbCx + thumbR, thumbCy + thumbR);
        SelectObject(dcMem, hOldBrThumb);
        SelectObject(dcMem, hOldPenThumb);
        DeleteObject(hPThumb);
        DeleteObject(hPThumbBorder);

        SelectObject(dcMem, hOldBrP);
        SelectObject(dcMem, hOldPenP);
        DeleteObject(hPTrack);
        DeleteObject(hNullPen);

        // ---- ROW 2: Controls ----
        int ctrlY = PROGRESS_ROW_H;
        int volIconSize = (int)(16 * dpi);
        int volW = (int)(70 * dpi);
        int volGap = (int)(3 * dpi);
        int volTotalW = volIconSize + volGap + volW;

        wchar_t timeBuf[64] = {};
        if (self->g_hTimeLabel)
            GetWindowTextW(self->g_hTimeLabel, timeBuf, 64);
        if (timeBuf[0] == L'\0')
            wcscpy_s(timeBuf, L"--:-- / --:--");

        HFONT hOldFont = (HFONT)SelectObject(dcMem, self->m_hTimeFont);
        SetBkMode(dcMem, TRANSPARENT);
        SetTextColor(dcMem, guiVidi::COLOR_MODERN_TEXT);

        SIZE timeSz = {0};
        GetTextExtentPoint32W(dcMem, timeBuf, (int)wcslen(timeBuf), &timeSz);
        int timeW = timeSz.cx + (int)(6 * dpi);
        if (timeW < (int)(80 * dpi))
            timeW = (int)(80 * dpi);

        int groupW = BTN_SIZE + SP + volTotalW + SP + timeW + SP + BTN_SIZE;
        int groupX = (wndW - groupW) / 2;

        // Play/Pause
        int playBtnX = groupX;
        int playBtnY = ctrlY + (CONTROLS_ROW_H - BTN_SIZE) / 2;
        HICON hPlayIcon = self->m_isPlaying ? self->m_hIconPause : self->m_hIconPlay;
        if (hPlayIcon) {
            DrawIconEx(dcMem, playBtnX + (BTN_SIZE - 22) / 2, playBtnY + (BTN_SIZE - 22) / 2, hPlayIcon, 22, 22, 0,
                       nullptr, DI_NORMAL);
        }

        // Volume
        int volStartX = playBtnX + BTN_SIZE + SP;
        int volBarY = ctrlY + (CONTROLS_ROW_H - (int)(5 * dpi)) / 2;

        int volPos = (int)SendMessage(self->g_hVolume, TBM_GETPOS, 0, 0);
        double volRatio = (double)volPos / VOL_MAX;
        if (volRatio < 0)
            volRatio = 0;
        if (volRatio > 1)
            volRatio = 1;

        if (self->m_hIconSpeaker) {
            DrawIconEx(dcMem, volStartX, volBarY - (int)(1 * dpi), self->m_hIconSpeaker, volIconSize, volIconSize, 0,
                       nullptr, DI_NORMAL);
        }

        int volTrackX = volStartX + volIconSize + volGap;
        RECT volTrack = {volTrackX, volBarY, volTrackX + volW, volBarY + (int)(5 * dpi)};

        HPEN hNullPen2 = CreatePen(PS_NULL, 0, 0);
        HBRUSH hVolTrack = CreateSolidBrush(COLOR_SEEK_TRACK);
        HGDIOBJ hOldPen2 = SelectObject(dcMem, hNullPen2);
        HGDIOBJ hOldBr2 = SelectObject(dcMem, hVolTrack);
        RoundRect(dcMem, volTrack.left, volTrack.top, volTrack.right, volTrack.bottom, 5, 5);

        int volFillX = volTrack.left + (int)(volRatio * (volTrack.right - volTrack.left));
        if (volFillX > volTrack.left + 5) {
            COLORREF cFill = (volRatio < 0.6667)
                                 ? LerpColor(RGB(60, 170, 70), RGB(255, 200, 40), volRatio / 0.6667)
                                 : LerpColor(RGB(255, 200, 40), RGB(225, 55, 55), (volRatio - 0.6667) / 0.3333);
            HBRUSH hVolFill = CreateSolidBrush(cFill);
            HGDIOBJ hPrevVol = SelectObject(dcMem, hVolFill);
            RoundRect(dcMem, volTrack.left, volTrack.top + 1, volFillX, volTrack.bottom - 1, 4, 4);
            SelectObject(dcMem, hPrevVol);
            DeleteObject(hVolFill);
        }

        int volThumbR = 5;
        int volThumbCx = volFillX;
        if (volThumbCx - volThumbR < volTrack.left)
            volThumbCx = volTrack.left + volThumbR;
        if (volThumbCx + volThumbR > volTrack.right)
            volThumbCx = volTrack.right - volThumbR;
        int volThumbCy = volBarY + (int)(5 * dpi) / 2;

        HBRUSH hVolThumb = CreateSolidBrush(COLOR_SEEK_FILL);
        HPEN hVolThumbBorder = CreatePen(PS_SOLID, 1, RGB(200, 100, 0));
        HGDIOBJ hOldBrVT = SelectObject(dcMem, hVolThumb);
        HGDIOBJ hOldPenVT = SelectObject(dcMem, hVolThumbBorder);
        Ellipse(dcMem, volThumbCx - volThumbR, volThumbCy - volThumbR, volThumbCx + volThumbR, volThumbCy + volThumbR);
        SelectObject(dcMem, hOldBrVT);
        SelectObject(dcMem, hOldPenVT);
        DeleteObject(hVolThumb);
        DeleteObject(hVolThumbBorder);

        SelectObject(dcMem, hOldBr2);
        SelectObject(dcMem, hOldPen2);
        DeleteObject(hVolTrack);
        DeleteObject(hNullPen2);

        // Time label
        int timeX = volStartX + volTotalW + SP;
        RECT rcTime = {timeX, ctrlY, timeX + timeW, ctrlY + CONTROLS_ROW_H};
        DrawTextW(dcMem, timeBuf, -1, &rcTime, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        SelectObject(dcMem, hOldFont);

        // Fullscreen button
        int fsBtnX = timeX + timeW + SP;
        int fsBtnY = ctrlY + (CONTROLS_ROW_H - BTN_SIZE) / 2;
        if (self->m_hIconFullscreen) {
            DrawIconEx(dcMem, fsBtnX + (BTN_SIZE - 22) / 2, fsBtnY + (BTN_SIZE - 22) / 2, self->m_hIconFullscreen, 22,
                       22, 0, nullptr, DI_NORMAL);
        }

        // Blit
        BitBlt(hdc, 0, 0, wndW, wndH, dcMem, 0, 0, SRCCOPY);

        SelectObject(dcMem, hOldBmp);
        DeleteObject(hBmp);
        DeleteDC(dcMem);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_MOUSEMOVE:
        self->PokeOSControls();
        return 0;

    case WM_LBUTTONDOWN: {
        int x = (short)LOWORD(lParam);
        int y = (short)HIWORD(lParam);

        RECT rcWnd;
        GetClientRect(hwnd, &rcWnd);
        int wndW = rcWnd.right - rcWnd.left;
        int wndH = rcWnd.bottom - rcWnd.top;

        double dpi = GetDpiScale(hwnd);
        const int PROGRESS_H = (int)(5 * dpi);
        const int EDGE = (int)(12 * dpi);
        const int BTN_SIZE = (int)(26 * dpi);
        const int SP = (int)(8 * dpi);
        const int PROGRESS_ROW_H = PROGRESS_H + (int)(2 * dpi);
        const int CONTROLS_ROW_H = wndH - PROGRESS_ROW_H;

        POINT pt = {x, y};

        // Progress bar click
        int progCy = PROGRESS_ROW_H / 2;
        RECT rcProg = {EDGE, progCy - (int)(8 * dpi), wndW - EDGE, progCy + (int)(8 * dpi)};
        if (PtInRect(&rcProg, pt) && self->m_cachedDuration > 0.0) {
            int clickX = x - EDGE;
            int progW = wndW - EDGE * 2;
            if (progW <= 0)
                return 0;
            double clickRatio = (double)clickX / progW;
            if (clickRatio < 0)
                clickRatio = 0;
            if (clickRatio > 1)
                clickRatio = 1;
            int newPos = (int)(clickRatio * self->m_progressRangeMax);
            self->SetProgressPos(newPos);
            double t = clickRatio * self->m_cachedDuration;
            self->m_player.Seek(t);
            self->UpdateTimeLabel(t, self->m_cachedDuration);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        // Controls row click
        int ctrlY = PROGRESS_ROW_H;

        int volIconSize = (int)(16 * dpi);
        int volW = (int)(70 * dpi);
        int volGap = (int)(3 * dpi);
        int volTotalW = volIconSize + volGap + volW;

        wchar_t timeBufHit[64] = {};
        if (self->g_hTimeLabel)
            GetWindowTextW(self->g_hTimeLabel, timeBufHit, 64);
        HDC hdcTemp = GetDC(hwnd);
        HFONT hOldFont2 = (HFONT)SelectObject(hdcTemp, self->m_hTimeFont);
        SIZE timeSzHit = {0};
        GetTextExtentPoint32W(hdcTemp, timeBufHit, (int)wcslen(timeBufHit), &timeSzHit);
        SelectObject(hdcTemp, hOldFont2);
        ReleaseDC(hwnd, hdcTemp);
        int timeWHit = timeSzHit.cx + (int)(6 * dpi);

        int groupW = BTN_SIZE + SP + volTotalW + SP + timeWHit + SP + BTN_SIZE;
        int groupX = (wndW - groupW) / 2;

        // Play/Pause click
        int playBtnXHit = groupX;
        int playBtnYHit = ctrlY + (CONTROLS_ROW_H - BTN_SIZE) / 2;
        RECT rcPlay = {playBtnXHit, playBtnYHit, playBtnXHit + BTN_SIZE, playBtnYHit + BTN_SIZE};
        if (PtInRect(&rcPlay, pt)) {
            if (self->m_isPlaying) {
                self->m_player.Pause();
                self->SetPlayPauseUI(false);
            } else {
                self->m_player.Play();
                self->SetPlayPauseUI(true);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        // Volume click
        int volStartXHit = playBtnXHit + BTN_SIZE + SP;
        int volTrackXHit = volStartXHit + volIconSize + volGap;
        int volBarYHit = ctrlY + (CONTROLS_ROW_H - (int)(5 * dpi)) / 2;
        RECT rcVolTrack = {volTrackXHit, volBarYHit - (int)(10 * dpi), volTrackXHit + volW,
                           volBarYHit + (int)(14 * dpi)};
        if (PtInRect(&rcVolTrack, pt)) {
            double volRatio = (double)(x - volTrackXHit) / volW;
            if (volRatio < 0)
                volRatio = 0;
            if (volRatio > 1)
                volRatio = 1;
            int volPos = (int)(volRatio * VOL_MAX);
            SendMessage(self->g_hVolume, TBM_SETPOS, TRUE, volPos);
            self->ApplyVolumeFromSlider(volPos);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        // Fullscreen click
        int timeXHit = volStartXHit + volTotalW + SP;
        int fsBtnXHit = timeXHit + timeWHit + SP;
        int fsBtnYHit = ctrlY + (CONTROLS_ROW_H - BTN_SIZE) / 2;
        RECT rcFs = {fsBtnXHit, fsBtnYHit, fsBtnXHit + BTN_SIZE, fsBtnYHit + BTN_SIZE};
        if (PtInRect(&rcFs, pt)) {
            self->ExitFullscreen();
            return 0;
        }

        return 0;
    }

    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;

    default:
        break;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

} // namespace guiVidi
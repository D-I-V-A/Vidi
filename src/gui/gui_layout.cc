#include "../../include/gui/gui.hh"
#include <cmath>

namespace guiVidi {

// ==========================================
// LAYOUT CONTROLS
// ==========================================
void VideoPlayerGUI::LayoutControls(int width, int height) {
    {
        wchar_t dbg[256];
        swprintf_s(dbg, L"[VIDI] LayoutControls(%d, %d) fullscreen=%d\n", width, height, m_isFullscreen);
        OutputDebugStringW(dbg);
    }
    if (m_isFullscreen) {
        LayoutFullscreen(width, height);
        return;
    }

    double dpi = GetDpiScale(g_hMainWnd);

    const int EDGE = (int)(8 * dpi);
    const int PROGRESS_H = (int)(18 * dpi);
    const int BTN_SIZE = (int)(36 * dpi);
    const int BTN_SPACING = (int)(4 * dpi);
    const int CTRL_H = (int)(24 * dpi);
    const int VOL_ICON_W = (int)(20 * dpi);
    const int VOL_GAP = (int)(4 * dpi);
    const int VOL_PERC_W = (int)(40 * dpi);
    const int PAD_TOP = (int)(4 * dpi);
    const int GAP_BAR_BTN = (int)(16 * dpi);
    const int PAD_BOTTOM = (int)(4 * dpi);

    int BOTTOM_BAR_H = PAD_TOP + PROGRESS_H + GAP_BAR_BTN + BTN_SIZE + PAD_BOTTOM;
    if (BOTTOM_BAR_H < (int)(72 * dpi))
        BOTTOM_BAR_H = (int)(72 * dpi);

    int videoHeight = height - BOTTOM_BAR_H;
    if (videoHeight < 100)
        videoHeight = 100;

    int progressY = videoHeight + PAD_TOP;
    int btnRowY = progressY + PROGRESS_H + GAP_BAR_BTN;

    struct BtnInfo {
        HWND h;
        bool valid;
    };
    BtnInfo allBtns[] = {{g_hSkipBack, m_hIconSkipBack != nullptr},
                         {g_hPlayBtn, m_hIconPlay != nullptr},
                         {g_hStopBtn, m_hIconStop != nullptr},
                         {g_hSkipForward, m_hIconSkipForward != nullptr},
                         {g_hFullscreenBtn, m_hIconFullscreen != nullptr},
                         {g_hPlaylistBtn, m_hIconPlaylist != nullptr},
                         {g_hLoopBtn, m_hIconLoop != nullptr},
                         {g_hShuffleBtn, m_hIconShuffle != nullptr}};
    const int btnCount = sizeof(allBtns) / sizeof(allBtns[0]);

    for (int i = 0; i < btnCount; ++i) {
        if (allBtns[i].h) {
            ShowWindow(allBtns[i].h, allBtns[i].valid ? SW_SHOW : SW_HIDE);
        }
    }

    int validCount = 0;
    int leftTotal = 0;
    for (int i = 0; i < btnCount; ++i) {
        if (allBtns[i].valid && allBtns[i].h) {
            leftTotal += BTN_SIZE;
            validCount++;
        }
    }
    if (validCount > 1)
        leftTotal += (validCount - 1) * BTN_SPACING;

    int leftMargin = EDGE;
    int rightAfterBtns = (int)(12 * dpi);

    int timeW = CurrentTimeLabelWidth(g_hTimeLabel, m_hTimeFont);
    if (timeW < (int)(80 * dpi))
        timeW = (int)(80 * dpi);
    int maxTimeW = (int)(200 * dpi);
    if (timeW > maxTimeW)
        timeW = maxTimeW;

    int volW = (int)(120 * dpi);

    int available = width - leftMargin - leftTotal - rightAfterBtns;
    int totalRightW = timeW + VOL_GAP + VOL_ICON_W + VOL_GAP + volW + VOL_GAP + VOL_PERC_W;

    if (totalRightW > available) {
        int shrink = totalRightW - available;

        int minVolW = (int)(30 * dpi);
        int volShrink = (volW - minVolW < shrink) ? (volW - minVolW) : shrink;
        if (volShrink > 0) {
            volW -= volShrink;
            shrink -= volShrink;
        }

        if (shrink > 0) {
            int minTimeW = (int)(40 * dpi);
            int timeShrink = (timeW - minTimeW < shrink) ? (timeW - minTimeW) : shrink;
            if (timeShrink > 0)
                timeW -= timeShrink;
        }

        totalRightW = timeW + VOL_GAP + VOL_ICON_W + VOL_GAP + volW + VOL_GAP + VOL_PERC_W;
    }

    int timeX = width - EDGE - totalRightW;
    int volX = timeX + timeW + VOL_GAP;
    int volY = btnRowY + (BTN_SIZE - CTRL_H) / 2;
    int timeY = volY;

    if (g_hVideoArea) {
        SetWindowPos(g_hVideoArea, HWND_BOTTOM, 0, 0, width, videoHeight, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    if (g_hProgress) {
        SetWindowPos(g_hProgress, HWND_TOP, EDGE, progressY, width - EDGE * 2, PROGRESS_H,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    int bx = leftMargin;
    for (int i = 0; i < btnCount; ++i) {
        if (allBtns[i].valid && allBtns[i].h) {
            SetWindowPos(allBtns[i].h, HWND_TOP, bx, btnRowY, BTN_SIZE, BTN_SIZE, SWP_NOACTIVATE | SWP_SHOWWINDOW);
            bx += BTN_SIZE + BTN_SPACING;
        }
    }

    if (g_hTimeLabel) {
        SetWindowPos(g_hTimeLabel, HWND_TOP, timeX, timeY, timeW, CTRL_H, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    if (g_hVolIcon) {
        SetWindowPos(g_hVolIcon, HWND_TOP, volX, volY, VOL_ICON_W, CTRL_H, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    if (g_hVolume) {
        SetWindowPos(g_hVolume, HWND_TOP, volX + VOL_ICON_W + VOL_GAP, volY, volW, CTRL_H,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    if (g_hVolPercent) {
        SetWindowPos(g_hVolPercent, HWND_TOP, volX + VOL_ICON_W + VOL_GAP + volW + VOL_GAP, volY, VOL_PERC_W, CTRL_H,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    m_player.UpdateVideoSize();
    InvalidateRect(g_hProgress, nullptr, TRUE);
}

// ==========================================
// LAYOUT FULLSCREEN
// ==========================================
void VideoPlayerGUI::LayoutFullscreen(int width, int height) {
    wchar_t dbg[256];
    swprintf_s(dbg, L"[VIDI] LayoutFullscreen called with %dx%d\n", width, height);
    OutputDebugStringW(dbg);
    double dpi = GetDpiScale(g_hMainWnd);
    const int EDGE = (int)(12 * dpi);
    const int BTN_SIZE = (int)(36 * dpi);
    const int SP = (int)(4 * dpi);
    const int CTRL_H = (int)(24 * dpi);
    const int BOTTOM_PAD = (int)(16 * dpi);
    const int PROGRESS_GAP = (int)(22 * dpi);
    const int PROGRESS_H = (int)(18 * dpi);

    int progressY = height - PROGRESS_H - BOTTOM_PAD;
    int btnRowY = progressY - PROGRESS_GAP - BTN_SIZE;

    const int VOL_ICON_W = (int)(20 * dpi);
    const int VOL_W = (int)(96 * dpi);
    const int VOL_PERC_W = (int)(40 * dpi);
    const int VOL_GAP = (int)(4 * dpi);

    int volTotalW = VOL_ICON_W + VOL_GAP + VOL_W + VOL_GAP + VOL_PERC_W;
    int volX = width - EDGE - volTotalW;
    int volY = btnRowY + (BTN_SIZE - CTRL_H) / 2;

    if (g_hVideoArea)
        SetWindowPos(g_hVideoArea, HWND_BOTTOM, 0, 0, width, height, SWP_SHOWWINDOW);

    if (g_hProgress)
        SetWindowPos(g_hProgress, HWND_TOP, EDGE, progressY, width - EDGE * 2, PROGRESS_H, SWP_SHOWWINDOW);

    HWND btns[8] = {g_hPlayBtn,       g_hSkipBack,    g_hStopBtn, g_hSkipForward,
                    g_hFullscreenBtn, g_hPlaylistBtn, g_hLoopBtn, g_hShuffleBtn};
    int stripW = BTN_SIZE * 8 + SP * 7;
    int bx = (width - stripW) / 2;
    for (HWND h : btns) {
        if (h)
            SetWindowPos(h, HWND_TOP, bx, btnRowY, BTN_SIZE, BTN_SIZE, SWP_SHOWWINDOW);
        bx += BTN_SIZE + SP;
    }

    if (g_hVolIcon)
        SetWindowPos(g_hVolIcon, HWND_TOP, volX, volY, VOL_ICON_W, CTRL_H, SWP_SHOWWINDOW);

    if (g_hVolume)
        SetWindowPos(g_hVolume, HWND_TOP, volX + VOL_ICON_W + VOL_GAP, volY, VOL_W, CTRL_H, SWP_SHOWWINDOW);

    if (g_hVolPercent)
        SetWindowPos(g_hVolPercent, HWND_TOP, volX + VOL_ICON_W + VOL_GAP + VOL_W + VOL_GAP, volY, VOL_PERC_W, CTRL_H,
                     SWP_SHOWWINDOW);

    if (g_hTimeLabel) {
        int timeW = CurrentTimeLabelWidth(g_hTimeLabel, m_hTimeFont);
        int timeX = width - EDGE - timeW;
        SetWindowPos(g_hTimeLabel, HWND_TOP, timeX, volY + CTRL_H + (int)(2 * dpi), timeW, CTRL_H, SWP_SHOWWINDOW);
    }

    m_player.UpdateVideoSize();
    InvalidateRect(g_hProgress, nullptr, FALSE);
}

} // namespace guiVidi
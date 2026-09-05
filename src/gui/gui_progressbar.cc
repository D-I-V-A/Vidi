#include "../../include/gui/gui.hh"
#include "../../include/kernels/ids.hh"
#include <cmath>

namespace guiVidi {

// ==========================================
// PROGRESS SUBCLASS — custom draw seekbar
// ==========================================
LRESULT CALLBACK VideoPlayerGUI::ProgressSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                                      UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    VideoPlayerGUI* self = reinterpret_cast<VideoPlayerGUI*>(dwRefData);
    switch (uMsg) {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        if (!self)
            break;
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc;
        GetClientRect(hwnd, &rc);
        HDC dcMem = CreateCompatibleDC(hdc);
        HBITMAP hBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HGDIOBJ hOldBmp = SelectObject(dcMem, hBmp);

        COLORREF bg = self->m_isFullscreen ? COLOR_TIP_BG : COLOR_MODERN_BG;
        HBRUSH hBgBrush = CreateSolidBrush(bg);
        FillRect(dcMem, &rc, hBgBrush);
        DeleteObject(hBgBrush);

        SendMessage(hwnd, WM_PRINTCLIENT, (WPARAM)dcMem, PRF_CLIENT);

        BitBlt(hdc, 0, 0, rc.right, rc.bottom, dcMem, 0, 0, SRCCOPY);

        SelectObject(dcMem, hOldBmp);
        DeleteObject(hBmp);
        DeleteDC(dcMem);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        if (self) {
            SetCapture(hwnd);
            self->m_isDraggingProgress = true;
            int x = (short)LOWORD(lParam);
            self->SeekFromTrackbarClick(x);
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (!self)
            break;
        int x = (short)LOWORD(lParam);
        if (self->m_isDraggingProgress && GetCapture() == hwnd) {
            self->DragSeekTo(x);
        } else {
            TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&tme);
            self->m_hotX = x;
            if (!self->m_seekHot) {
                self->m_seekHot = true;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }
        return 0;
    }

    case WM_MOUSELEAVE:
        if (self && self->m_seekHot) {
            self->m_seekHot = false;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_LBUTTONUP:
        if (self && GetCapture() == hwnd) {
            self->EndSeekDrag();
            ReleaseCapture();
        }
        return 0;

    case WM_CAPTURECHANGED:
        if (self) {
            self->m_isDraggingProgress = false;
            InvalidateRect(hwnd, nullptr, FALSE);
            self->HideTimeTip();
        }
        return 0;
    }
    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

// ==========================================
// VIDEO AREA SUBCLASS — double-click untuk toggle fullscreen
// ==========================================
LRESULT CALLBACK VideoPlayerGUI::VideoAreaSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                                       UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    VideoPlayerGUI* self = reinterpret_cast<VideoPlayerGUI*>(dwRefData);
    switch (uMsg) {
    case WM_LBUTTONDOWN:
        if (self) {
            DWORD now = GetTickCount();
            short x = (short)LOWORD(lParam), y = (short)HIWORD(lParam);
            if (now - self->m_lastVideoClickTick < GetDoubleClickTime() && abs(x - self->m_lastVideoClickX) < 4 &&
                abs(y - self->m_lastVideoClickY) < 4) {
                self->m_lastVideoClickTick = 0;
                if (self->m_isFullscreen)
                    self->ExitFullscreen();
                else
                    self->EnterFullscreen();
            } else {
                self->m_lastVideoClickTick = now;
                self->m_lastVideoClickX = x;
                self->m_lastVideoClickY = y;
            }
        }
        break;
    }
    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

// ==========================================
// DRAW VLC SEEKBAR — custom draw seekbar
// ==========================================
void VideoPlayerGUI::DrawVlcSeekbar(HDC hdc) {
    RECT rc;
    GetClientRect(g_hProgress, &rc);

    int pos = (int)SendMessage(g_hProgress, TBM_GETPOS, 0, 0);
    const int BAR_H = 5;
    const int THUMB_SIZE = m_seekHot ? 14 : 12;
    int cy = (rc.top + rc.bottom) / 2;
    RECT track = {rc.left + 2, cy - BAR_H / 2, rc.right - 3, cy + BAR_H / 2};
    int w = track.right - track.left;

    HPEN hNullPen = CreatePen(PS_NULL, 0, 0);
    HBRUSH hTrack = CreateSolidBrush(COLOR_SEEK_TRACK);
    HGDIOBJ hOldPen = SelectObject(hdc, hNullPen);
    HGDIOBJ hOldBr = SelectObject(hdc, hTrack);

    RoundRect(hdc, track.left, track.top, track.right, track.bottom, BAR_H, BAR_H);

    double ratio = (m_progressRangeMax > 0) ? (double)pos / m_progressRangeMax : 0.0;
    if (ratio < 0)
        ratio = 0;
    if (ratio > 1)
        ratio = 1;

    int fx = track.left + (int)(ratio * w);
    if (fx > track.left + BAR_H) {
        HBRUSH hFill = CreateSolidBrush(COLOR_SEEK_FILL);
        HGDIOBJ hPrev = SelectObject(hdc, hFill);
        RoundRect(hdc, track.left, track.top + 1, fx, track.bottom - 1, BAR_H - 2, BAR_H - 2);
        SelectObject(hdc, hPrev);
        DeleteObject(hFill);
    }

    RECT thumb = {fx - THUMB_SIZE / 2, cy - THUMB_SIZE / 2, fx + THUMB_SIZE / 2, cy + THUMB_SIZE / 2};
    HBRUSH hThumb = CreateSolidBrush(RGB(255, 255, 255));
    HGDIOBJ hPrevThumb = SelectObject(hdc, hThumb);
    FillRect(hdc, &thumb, hThumb);
    SelectObject(hdc, hPrevThumb);
    DeleteObject(hThumb);

    HPEN hBorderPen = CreatePen(PS_SOLID, 1, RGB(180, 180, 180));
    HGDIOBJ hPrevBorder = SelectObject(hdc, hBorderPen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, thumb.left, thumb.top, thumb.right, thumb.bottom);
    SelectObject(hdc, hPrevBorder);
    DeleteObject(hBorderPen);

    SelectObject(hdc, hOldBr);
    SelectObject(hdc, hOldPen);
    DeleteObject(hTrack);
    DeleteObject(hNullPen);
}

// ==========================================
// SCRUBBING — klik mulai drag, drag real-time, lepas = seek final
// ==========================================
void VideoPlayerGUI::SeekFromTrackbarClick(int mouseX) {
    RECT rc;
    GetClientRect(g_hProgress, &rc);
    if (rc.right <= 0)
        return;
    if (m_cachedDuration <= 0.0)
        return;

    double ratio = static_cast<double>(mouseX) / static_cast<double>(rc.right);
    if (ratio < 0.0)
        ratio = 0.0;
    if (ratio > 1.0)
        ratio = 1.0;

    m_hotX = mouseX;
    m_isDraggingProgress = true;

    int newPos = static_cast<int>(ratio * m_progressRangeMax);
    SetProgressPos(newPos);
    UpdateSeekFromPos(newPos);
}

void VideoPlayerGUI::DragSeekTo(int x) {
    RECT rc;
    GetClientRect(g_hProgress, &rc);
    if (rc.right <= 0 || m_cachedDuration <= 0.0)
        return;

    double ratio = static_cast<double>(x) / static_cast<double>(rc.right);
    if (ratio < 0.0)
        ratio = 0.0;
    if (ratio > 1.0)
        ratio = 1.0;

    m_hotX = x;
    int newPos = static_cast<int>(ratio * m_progressRangeMax);
    SetProgressPos(newPos);
    UpdateSeekFromPos(newPos);
}

void VideoPlayerGUI::UpdateSeekFromPos(int pos) {
    if (m_cachedDuration <= 0.0)
        return;

    double t = (static_cast<double>(pos) / m_progressRangeMax) * m_cachedDuration;
    UpdateTimeLabel(t, m_cachedDuration);
    ShowTimeTip(t);

    DWORD now = GetTickCount();
    if (now - m_lastSeekTick > 120) {
        m_player.Seek(t);
        m_lastSeekTick = now;
    }
}

void VideoPlayerGUI::EndSeekDrag() {
    if (!m_isDraggingProgress)
        return;
    m_isDraggingProgress = false;
    HideTimeTip();

    if (m_cachedDuration > 0.0) {
        int pos = (int)SendMessage(g_hProgress, TBM_GETPOS, 0, 0);
        double t = (static_cast<double>(pos) / m_progressRangeMax) * m_cachedDuration;
        m_player.Seek(t);

        m_hasPendingSeek = true;
        m_pendingSeekTarget = t;
        m_pendingSeekStartTick = GetTickCount();
    }
}

void VideoPlayerGUI::ShowTimeTip(double seconds) {
    if (!m_hTimeTip)
        return;

    wchar_t buf[24];
    int s = (int)seconds;
    if (s >= 3600)
        swprintf_s(buf, L"%d:%02d:%02d", s / 3600, (s % 3600) / 60, s % 60);
    else
        swprintf_s(buf, L"%02d:%02d", s / 60, s % 60);
    SetWindowText(m_hTimeTip, buf);

    POINT pt = {m_hotX, -30};
    ClientToScreen(g_hProgress, &pt);
    SetWindowPos(m_hTimeTip, HWND_TOPMOST, pt.x - 40, pt.y, 80, 22, SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void VideoPlayerGUI::HideTimeTip() {
    if (m_hTimeTip)
        ShowWindow(m_hTimeTip, SW_HIDE);
}

// ==========================================
// HELPER — set posisi progress + paksa repaint penuh channel
// ==========================================
void VideoPlayerGUI::SetProgressPos(int pos) {
    int cur = (int)SendMessage(g_hProgress, TBM_GETPOS, 0, 0);
    if (cur == pos)
        return;

    SendMessage(g_hProgress, TBM_SETPOS, TRUE, pos);
    InvalidateRect(g_hProgress, nullptr, FALSE);
}

// ==========================================
// HSCROLL — keyboard arrow di trackbar
// ==========================================
void VideoPlayerGUI::OnHScroll(WPARAM wParam, LPARAM lParam) {
    HWND hCtrl = (HWND)lParam;
    int code = LOWORD(wParam);

    if (hCtrl == g_hProgress) {
        if (code == TB_THUMBTRACK || code == TB_THUMBPOSITION || code == TB_ENDTRACK) {
            if (m_isDraggingProgress)
                return;
            int pos = (int)SendMessage(g_hProgress, TBM_GETPOS, 0, 0);
            SetProgressPos(pos);
            UpdateSeekFromPos(pos);
        }
    } else if (hCtrl == g_hVolume) {
        int vol = (int)SendMessage(g_hVolume, TBM_GETPOS, 0, 0);
        ApplyVolumeFromSlider(vol);
        InvalidateRect(g_hVolume, nullptr, FALSE);
    }
}

} // namespace guiVidi

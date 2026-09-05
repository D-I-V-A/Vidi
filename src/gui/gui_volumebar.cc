#include "../../include/gui/gui.hh"
#include "../../include/kernels/ids.hh"

namespace guiVidi {

// ==========================================
// VOLUME SUBCLASS
// ==========================================
LRESULT CALLBACK VideoPlayerGUI::VolumeSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
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

    case WM_LBUTTONDOWN:
        if (self) {
            SetCapture(hwnd);
            self->m_volDrag = true;
            RECT rc;
            GetClientRect(hwnd, &rc);
            int x = (short)LOWORD(lParam);
            int pos = (rc.right > 0) ? (int)(((double)x / rc.right) * VOL_MAX) : 0;
            if (pos < 0) pos = 0;
            if (pos > VOL_MAX) pos = VOL_MAX;
            SendMessage(hwnd, TBM_SETPOS, TRUE, pos);
            self->m_volHotX = x;
            self->ApplyVolumeFromSlider(pos);
            InvalidateRect(hwnd, nullptr, FALSE);
            self->ShowVolTip(pos);
        }
        return 0;

    case WM_MOUSEMOVE: {
        if (!self)
            break;
        int x = (short)LOWORD(lParam);
        if (self->m_volDrag && GetCapture() == hwnd) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            int pos = (rc.right > 0) ? (int)(((double)x / rc.right) * VOL_MAX) : 0;
            if (pos < 0) pos = 0;
            if (pos > VOL_MAX) pos = VOL_MAX;
            SendMessage(hwnd, TBM_SETPOS, TRUE, pos);
            self->m_volHotX = x;
            self->ApplyVolumeFromSlider(pos);
            InvalidateRect(hwnd, nullptr, FALSE);
            self->ShowVolTip(pos);
        } else {
            TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&tme);
            if (!self->m_volHot) {
                self->m_volHot = true;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }
        return 0;
    }

    case WM_MOUSELEAVE:
        if (self && self->m_volHot) {
            self->m_volHot = false;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_LBUTTONUP:
        if (self && GetCapture() == hwnd)
            ReleaseCapture();
        return 0;

    case WM_CAPTURECHANGED:
        if (self) {
            self->m_volDrag = false;
            InvalidateRect(hwnd, nullptr, FALSE);
            self->HideTimeTip();
        }
        return 0;
    }
    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

// ==========================================
// DRAW VLC VOLUME BAR — gradasi hijau -> kuning -> merah
// ==========================================
void VideoPlayerGUI::DrawVlcVolumeBar(HDC hdc) {
    RECT rc;
    GetClientRect(g_hVolume, &rc);

    int pos = (int)SendMessage(g_hVolume, TBM_GETPOS, 0, 0);
    const int BAR_H = 5;
    const int THUMB_SIZE = (m_volHot || m_volDrag) ? 14 : 12;
    int cy = (rc.top + rc.bottom) / 2;
    RECT track = {rc.left + 2, cy - BAR_H / 2, rc.right - 3, cy + BAR_H / 2};
    int w = track.right - track.left;

    HPEN hNullPen = CreatePen(PS_NULL, 0, 0);
    HBRUSH hTrack = CreateSolidBrush(COLOR_SEEK_TRACK);
    HGDIOBJ hOldPen = SelectObject(hdc, hNullPen);
    HGDIOBJ hOldBr = SelectObject(hdc, hTrack);

    RoundRect(hdc, track.left, track.top, track.right, track.bottom, BAR_H, BAR_H);

    double ratio = (double)pos / VOL_MAX;
    if (ratio < 0) ratio = 0;
    if (ratio > 1) ratio = 1;
    COLORREF cFill = (ratio < 0.6667) ? LerpColor(RGB(60, 170, 70), RGB(255, 200, 40), ratio / 0.6667)
                                      : LerpColor(RGB(255, 200, 40), RGB(225, 55, 55), (ratio - 0.6667) / 0.3333);

    int fx = track.left + (int)(ratio * w);
    if (fx > track.left + BAR_H) {
        HBRUSH hFill = CreateSolidBrush(cFill);
        HGDIOBJ hPrev = SelectObject(hdc, hFill);
        RoundRect(hdc, track.left, track.top + 1, fx, track.bottom - 1, BAR_H - 2, BAR_H - 2);
        SelectObject(hdc, hPrev);
        DeleteObject(hFill);
    }

    int notch = track.left + (int)(w * (100.0 / VOL_MAX));
    HPEN hNotch = CreatePen(PS_SOLID, 1, RGB(150, 150, 150));
    HGDIOBJ hPrevNotch = SelectObject(hdc, hNotch);
    MoveToEx(hdc, notch, track.top - 1, nullptr);
    LineTo(hdc, notch, track.bottom + 1);
    SelectObject(hdc, hPrevNotch);
    DeleteObject(hNotch);

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
// APPLY VOLUME
// ==========================================
void VideoPlayerGUI::ApplyVolumeFromSlider(int pos) {
    if (pos < 0) pos = 0;
    if (pos > VOL_MAX) pos = VOL_MAX;
    float v = pos / 100.0f;
    float native = (v > 1.0f) ? 1.0f : v;
    float boost = (v > 1.0f) ? v : 1.0f;
    m_player.SetVolume(native);
    m_player.SetDspGain(boost);
    if (pos > 0 && m_isMuted)
        m_isMuted = false;
    UpdateVolumePercent(pos);
}

void VideoPlayerGUI::ShowVolTip(int pos) {
    if (!m_hTimeTip)
        return;
    wchar_t buf[16];
    swprintf_s(buf, L"%d%%", pos);
    SetWindowText(m_hTimeTip, buf);
    POINT pt = {m_volHotX, -30};
    ClientToScreen(g_hVolume, &pt);
    SetWindowPos(m_hTimeTip, HWND_TOPMOST, pt.x - 30, pt.y, 60, 22, SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void VideoPlayerGUI::UpdateVolumePercent(int pos) {
    if (!g_hVolPercent)
        return;
    wchar_t buf[8];
    swprintf_s(buf, L"%d%%", pos);
    SetWindowTextW(g_hVolPercent, buf);
}

} // namespace guiVidi

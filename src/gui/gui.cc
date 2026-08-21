#include "../../include/gui/gui.hh"
#include "../../include/kernels/ids.hh"
#include <wtsapi32.h>
#include <string>
#include <cmath>

namespace guiVidi {

// ==========================================
// LAYOUT CONTROLS
// ==========================================
int MeasureStringWidth(HWND hwndRef, HFONT hFont, const wchar_t* text) {
        if (!hwndRef || !text) return 90;
        HDC hdc = GetDC(hwndRef);
        HGDIOBJ old = SelectObject(hdc, hFont ? (HGDIOBJ)hFont
                                              : GetStockObject(DEFAULT_GUI_FONT));
        SIZE sz = {};
        GetTextExtentPoint32W(hdc, text, (int)wcslen(text), &sz);
        SelectObject(hdc, old);
        ReleaseDC(hwndRef, hdc);
        int w = sz.cx + 8;          // padding kecil
        return (w < 90) ? 90 : w;   // batas bawah agar tidak loncat-loncat
    }

    // Ambil lebar yang dibutuhkan oleh teks SAAT INI di label waktu
int CurrentTimeLabelWidth(HWND hLabel, HFONT hFont) {
        wchar_t buf[64] = {};
        GetWindowTextW(hLabel, buf, 64);
        return MeasureStringWidth(hLabel, hFont, buf);
}

// Interpolasi warna utk gradasi volume hijau -> kuning -> merah
COLORREF LerpColor(COLORREF a, COLORREF b, double t) {
        if (t < 0) t = 0;
        if (t > 1) t = 1;
        return RGB(
            (int)(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t),
            (int)(GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t),
            (int)(GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t));
}
void VideoPlayerGUI::LayoutControls(int width, int height) {
    if (m_isFullscreen) { LayoutFullscreen(width, height); return; }

    const int EDGE           = 8;   // inset kiri/kanan
    const int BOTTOM_BAR_H   = 64;  // tinggi total control bar
    const int PROGRESS_H     = 18;  // hit-area trackbar (visual digambar manual)
    const int PROGRESS_PAD_Y = 2;
    const int BTN_SIZE       = 36;
    const int BTN_SPACING    = 4;
    const int GAP            = 6;   // jarak progress -> baris tombol

    int videoHeight = height - BOTTOM_BAR_H;
    if (videoHeight < 100) videoHeight = 100;

    int barY      = videoHeight;
    int progressY = barY + PROGRESS_PAD_Y;
    int btnRowY   = progressY + PROGRESS_H + GAP;

    // Lebar label waktu DIUKUR dari teks aktual -> tidak pernah terpotong
    const int VOL_W        = 96;
    const int VOL_TIME_GAP = 8;
    int timeW  = CurrentTimeLabelWidth(g_hTimeLabel, m_hTimeFont);
    int timeX  = width - EDGE - timeW;                    // waktu paling kanan
    int volX   = timeX - VOL_TIME_GAP - VOL_W;            // volume di kirinya
    int volY   = btnRowY + (BTN_SIZE - 24) / 2;           // center vertikal vs tombol

    // ===== Batch atomik: posisi + ukuran + z-order dalam 1 operasi =====
    HDWP dwp = BeginDeferWindowPos(9);
    HWND hAfter = nullptr;

    // 0) Video area: paling bawah, dock fill sisa ruang di atas control bar
    if (g_hVideoArea) {
        dwp = DeferWindowPos(dwp, g_hVideoArea, HWND_BOTTOM,
                             0, 0, width, videoHeight,
                             SWP_NOACTIVATE | SWP_SHOWWINDOW);
        hAfter = g_hVideoArea;
    }

    // 1) Progress bar: STRETCH penuh mengikuti lebar window
    if (g_hProgress) {
        dwp = DeferWindowPos(dwp, g_hProgress, hAfter ? hAfter : HWND_TOP,
                             EDGE, progressY, width - EDGE * 2, PROGRESS_H,
                             SWP_NOACTIVATE | SWP_SHOWWINDOW);
        hAfter = g_hProgress;
    }

    // 2) Tombol playback rata kiri, spacing seragam
    HWND btns[5] = { g_hSkipBack, g_hPlayBtn, g_hPauseBtn, g_hStopBtn, g_hSkipForward };
    int bx = EDGE;
    for (HWND h : btns) {
        if (!h) continue;
        dwp = DeferWindowPos(dwp, h, hAfter ? hAfter : HWND_TOP,
                             bx, btnRowY, BTN_SIZE, BTN_SIZE,
                             SWP_NOACTIVATE | SWP_SHOWWINDOW);
        hAfter = h;
        bx += BTN_SIZE + BTN_SPACING;
    }

    // 3) Volume & waktu rata kanan (anchor ke edge kanan)
    if (g_hVolume) {
        dwp = DeferWindowPos(dwp, g_hVolume, hAfter ? hAfter : HWND_TOP,
                             volX, volY, VOL_W, 24, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        hAfter = g_hVolume;
    }
    if (g_hTimeLabel) {
        dwp = DeferWindowPos(dwp, g_hTimeLabel, hAfter ? hAfter : HWND_TOP,
                             timeX, volY, timeW, 24, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
    if (dwp) EndDeferWindowPos(dwp);

    m_player.UpdateVideoSize();

    // KUNCI FIX #1/#4: custom-draw seekbar hanya repaint area yang trackbar anggap
    // dirty. Setelah melebar, paksa repaint penuh supaya bar mengikuti lebar baru.
    InvalidateRect(g_hProgress, nullptr, TRUE);
}

// ==========================================
// WINDOW PROC
// ==========================================
LRESULT CALLBACK VideoPlayerGUI::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    VideoPlayerGUI* self = reinterpret_cast<VideoPlayerGUI*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (uMsg) {
        case WM_CREATE:
        {
            CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
            VideoPlayerGUI* pThis = reinterpret_cast<VideoPlayerGUI*>(pCreate->lpCreateParams);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
            
            SetWindowTheme(hwnd, L"Explorer", nullptr);
            pThis->CreateMenuBar(hwnd);
            pThis->CreateControls(hwnd);
            
            // [PENTING] Panggil LayoutControls setelah create
            RECT rc;
            GetClientRect(hwnd, &rc);
            pThis->LayoutControls(rc.right, rc.bottom);
            
            return 0;
        }

        case WM_ERASEBKGND:
        {
            if (!self) return 0;
            HDC hdc = (HDC)wParam;
            RECT rc;
            GetClientRect(hwnd, &rc);
            HBRUSH hBrush = CreateSolidBrush(self->COLOR_MODERN_BG);
            FillRect(hdc, &rc, hBrush);
            DeleteObject(hBrush);
            return 1;
        }

        case WM_CTLCOLORSTATIC:
        {
            HDC hdc = (HDC)wParam;

            // Tooltip waktu seekbar: bg gelap + teks putih
            if ((HWND)lParam == self->m_hTimeTip) {
                SetBkColor(hdc, self->COLOR_TIP_BG);
                SetTextColor(hdc, RGB(255, 255, 255));
                static HBRUSH hTipBrush = CreateSolidBrush(self->COLOR_TIP_BG);
                return (INT_PTR)hTipBrush;
            }

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, self->COLOR_MODERN_TEXT);
            static HBRUSH hBrush = CreateSolidBrush(self->COLOR_MODERN_BG);
            return (INT_PTR)hBrush;
        }

        // [VLC-STYLE] Custom draw seekbar + volume bar
        case WM_NOTIFY:
        {
            LPNMHDR pnmh = (LPNMHDR)lParam;
            if (!self) break;
            if (pnmh->code != NM_CUSTOMDRAW) break;

            if (pnmh->hwndFrom == self->g_hProgress) {
                LPNMCUSTOMDRAW pcd = (LPNMCUSTOMDRAW)lParam;
                if (pcd->dwDrawStage == CDDS_PREPAINT)
                    return CDRF_NOTIFYITEMDRAW;
                if (pcd->dwDrawStage == CDDS_ITEMPREPAINT) {
                    if (pcd->dwItemSpec == TBCD_CHANNEL) {
                        self->DrawVlcSeekbar(pcd->hdc);
                        return CDRF_SKIPDEFAULT;
                    }
                    if (pcd->dwItemSpec == TBCD_THUMB)
                        return CDRF_SKIPDEFAULT;   // sembunyikan thumb sistem
                }
                return CDRF_DODEFAULT;
            }
            if (pnmh->hwndFrom == self->g_hVolume) {
                LPNMCUSTOMDRAW pcd = (LPNMCUSTOMDRAW)lParam;
                if (pcd->dwDrawStage == CDDS_PREPAINT)
                    return CDRF_NOTIFYITEMDRAW;
                if (pcd->dwDrawStage == CDDS_ITEMPREPAINT) {
                    if (pcd->dwItemSpec == TBCD_CHANNEL) {
                        self->DrawVlcVolumeBar(pcd->hdc);
                        return CDRF_SKIPDEFAULT;
                    }
                    if (pcd->dwItemSpec == TBCD_THUMB)
                        return CDRF_SKIPDEFAULT;   // sembunyikan thumb sistem
                }
                return CDRF_DODEFAULT;
            }
            break;
        }
        case WM_GETMINMAXINFO:
        {
            // Batas min window: cluster kanan (volume+waktu ~250px dari kanan)
            // tidak pernah menimpa cluster tombol kiri (~210px dari kiri)
            MINMAXINFO* pmmi = reinterpret_cast<MINMAXINFO*>(lParam);
            pmmi->ptMinTrackSize.x = 500;
            pmmi->ptMinTrackSize.y = 320;
            return 0;
        }
        case WM_SIZE:
            if (!self) break;
            if (wParam == SIZE_MINIMIZED) { self->m_wasMinimized = true; return 0; }
            if (self->m_wasMinimized) {
                self->m_wasMinimized = false;
                self->m_player.UpdateVideoSize();
                self->RecoverVideo();
            }
            self->LayoutControls(LOWORD(lParam), HIWORD(lParam));
            // [FIX MAXIMIZE] Setelah maximize/restore, surface renderer dibuat ulang.
            // Saat paused tidak ada frame baru -> paksa render 1 frame agar tidak
            // menampilkan frame idle renderer (gradient hijau).
            if (wParam == SIZE_MAXIMIZED || wParam == SIZE_RESTORED)
                self->RecoverVideo();
            return 0;

        case WM_EXITSIZEMOVE:
            // [FIX] Selesai drag-resize: finalisasi ukuran video + repaint frame
            if (self) { self->m_player.UpdateVideoSize(); self->RecoverVideo(); }
            return 0;
        case WM_COMMAND:
            if(self) self->OnCommand(wParam,lParam);
            return 0;
        case WM_HSCROLL:
            if(self) self->OnHScroll(wParam,lParam);
            return 0;
        case WM_TIMER:
            if (!self) break;
            if (wParam == ID_TIMER_UPDATE) {
                self->OnTimerTick();
            }
            else if (wParam == ID_TIMER_OSI_HIDE) {
                // Auto-hide overlay + cursor saat idle di fullscreen
                if (self->m_isFullscreen && !self->m_isDraggingProgress) {
                    self->ShowOSControls(false);
                    if (!self->m_cursorHidden) {
                        ShowCursor(FALSE);
                        self->m_cursorHidden = true;
                    }
                }
            }
            return 0;
        case WM_APP_MEDIA_READY:
            if (self) self->OnMediaReady();   // durasi trackbar + fit window ke video
            return 0;

        case WM_ACTIVATEAPP:
            // Kehilangan fokus: tetap play (gaya VLC). Fokus kembali: pulihkan video.
            if (wParam && self) self->RecoverVideo();
            return 0;

        case WM_WTSSESSION_CHANGE:
            if (self && (wParam == WTS_SESSION_UNLOCK || wParam == WTS_REMOTE_CONNECT ||
                         wParam == WTS_CONSOLE_CONNECT))
                self->RecoverVideo();
            return 0;

        case WM_DISPLAYCHANGE:
            if (self) { self->m_player.UpdateVideoSize(); self->RecoverVideo(); }
            return 0;

        case WM_QUERYENDSESSION:
            return TRUE;

        case WM_ENDSESSION:
            if (self && wParam) {   // shutdown/logoff: stop rapi
                self->m_player.Stop();
                self->SetPlayPauseUI(false);
            }
            return 0;

        case WM_APP_GRAPH_EVENT:
            if (self) self->m_player.HandleGraphEvent();   // EC_COMPLETE dll.
            return 0;

        case WM_APP_PLAYBACK_ENDED:
            if (self) {
                self->SetPlayPauseUI(false);
                self->SetProgressPos(self->m_progressRangeMax);
            }
            return 0;
        case WM_APP_AUDIO_MISSING:
            MessageBox(hwnd, L"File ini punya track audio tapi codec-nya tidak ditemukan.\nVideo tetap diputar tanpa suara.",
               L"Vidi Player", MB_OK | MB_ICONWARNING);
        break;
        case WM_APP_MEDIA_ERROR:
            if (self) {
                MessageBox(self->g_hMainWnd, L"Gagal memutar file (graph error).",
                           L"Vidi", MB_OK | MB_ICONERROR);
                self->SetPlayPauseUI(false);
                self->SetProgressPos(0);
            }
            return 0;

        case WM_DESTROY:
            if (self->m_hModernFont) DeleteObject(self->m_hModernFont);
            if (self->m_hTimeFont) DeleteObject(self->m_hTimeFont);
            if (self->m_hTipFont) DeleteObject(self->m_hTipFont);
            WTSUnRegisterSessionNotification(hwnd);
            KillTimer(hwnd, ID_TIMER_UPDATE);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// ==========================================
// TRACKBAR SUBCLASS — biar klik langsung loncat ke titik klik
// ==========================================
LRESULT CALLBACK VideoPlayerGUI::ProgressSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                                       UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    VideoPlayerGUI* self = reinterpret_cast<VideoPlayerGUI*>(dwRefData);

    switch (uMsg) {
        case WM_ERASEBKGND:
            // [FIX] Jangan cuma return 1 (dulu: tidak pernah menghapus -> jejak
            // frame lama/gosong). Kita isi bg putih di sini; bar digambar lagi
            // oleh NM_CUSTOMDRAW sehingga anti-flicker tetap terjaga.
            if (self) {
                HDC hdc = (HDC)wParam;
                RECT rcE;
                GetClientRect(hwnd, &rcE);
                HBRUSH hBg = CreateSolidBrush(self->COLOR_MODERN_BG);
                FillRect(hdc, &rcE, hBg);
                DeleteObject(hBg);
            }
            return 1;

        case WM_LBUTTONDOWN:
            if (self) {
                SetCapture(hwnd);
                self->SeekFromTrackbarClick((short)LOWORD(lParam));
            }
            return 0;

        case WM_MOUSEMOVE: {
            if (!self) break;
            int x = (short)LOWORD(lParam);
            if (GetCapture() == hwnd && self->m_isDraggingProgress) {
                self->DragSeekTo(x);
            } else {
                TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
                TrackMouseEvent(&tme);
                self->m_seekHot = true;
                self->m_hotX = x;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_MOUSELEAVE:
            if (self) {
                self->m_seekHot = false;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;

        case WM_LBUTTONUP:
            if (self && GetCapture() == hwnd) {
                ReleaseCapture();
                self->EndSeekDrag();
            }
            return 0;

        case WM_CAPTURECHANGED:
            if (self) self->EndSeekDrag();
            return 0;
    }

    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

// ==========================================
// VLC-STYLE SEEKBAR — track abu tipis, fill oranye, thumb kotak putih
// ==========================================
void VideoPlayerGUI::DrawVlcSeekbar(HDC hdc) {
    RECT rc;
    GetClientRect(g_hProgress, &rc);

    // BG penuh kontrol (pengganti erasebkgnd, menghapus jejak frame lama)
    HBRUSH hBg = CreateSolidBrush(COLOR_MODERN_BG);
    FillRect(hdc, &rc, hBg);
    DeleteObject(hBg);

    int pos = (int)SendMessage(g_hProgress, TBM_GETPOS, 0, 0);
    int max = (int)SendMessage(g_hProgress, TBM_GETRANGEMAX, 0, 0);

    const int BAR_H = 5;                    // bar visual tipis
    const int THUMB_SIZE = (m_seekHot || m_isDraggingProgress) ? 14 : 12;
    int cy = (rc.top + rc.bottom) / 2;
    RECT track = { rc.left + 2, cy - BAR_H / 2, rc.right - 3, cy + BAR_H / 2 };
    int width = track.right - track.left;

    HPEN hNullPen = CreatePen(PS_NULL, 0, 0);
    HGDIOBJ hOldPen = SelectObject(hdc, hNullPen);

    // Track abu muda rounded
    HBRUSH hTrack = CreateSolidBrush(COLOR_SEEK_TRACK);
    HGDIOBJ hOldBr = SelectObject(hdc, hTrack);
    RoundRect(hdc, track.left, track.top, track.right, track.bottom, BAR_H, BAR_H);

    // Fill oranye — inset 1px agar sedikit lebih tipis dari track
    bool hot = m_seekHot || m_isDraggingProgress;
    int fx = track.left + (max > 0 ? (int)(((double)pos / max) * width) : 0);
    if (fx > track.left + BAR_H) {
        HBRUSH hFill = CreateSolidBrush(hot ? COLOR_SEEK_FILL_HOT : COLOR_SEEK_FILL);
        SelectObject(hdc, hFill);
        RoundRect(hdc, track.left, track.top + 1, fx, track.bottom - 1, BAR_H - 2, BAR_H - 2);
        DeleteObject(hFill);
    }

    // Thumb kotak putih dengan border abu halus
    RECT thumb = { fx - THUMB_SIZE / 2, cy - THUMB_SIZE / 2,
                   fx + THUMB_SIZE / 2, cy + THUMB_SIZE / 2 };
    HBRUSH hThumb = CreateSolidBrush(RGB(255, 255, 255));
    SelectObject(hdc, hThumb);
    FillRect(hdc, &thumb, hThumb);
    DeleteObject(hThumb);

    HPEN hBorderPen = CreatePen(PS_SOLID, 1, RGB(180, 180, 180));
    SelectObject(hdc, hBorderPen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, thumb.left, thumb.top, thumb.right, thumb.bottom);
    DeleteObject(hBorderPen);

    SelectObject(hdc, hOldBr);
    SelectObject(hdc, hOldPen);
    DeleteObject(hNullPen);
}

// ==========================================
// [VOL 0-150] posisi slider -> volume native + DSP gain
// ==========================================
void VideoPlayerGUI::ApplyVolumeFromSlider(int pos) {
    if (pos < 0) pos = 0;
    if (pos > VOL_MAX) pos = VOL_MAX;
    float v      = pos / 100.0f;                  // 0 .. 1.5
    float native = (v > 1.0f) ? 1.0f : v;         // <=100% via IBasicAudio
    float boost  = (v > 1.0f) ? v     : 1.0f;     // >100% via DSP
    m_player.SetVolume(native);
    m_player.SetDspGain(boost);
    if (pos > 0 && m_isMuted) m_isMuted = false;  // geser manual -> mute lepas
}

void VideoPlayerGUI::ShowVolTip(int pos) {
    if (!m_hTimeTip) return;
    wchar_t buf[16];
    swprintf_s(buf, L"%d%%", pos);
    SetWindowText(m_hTimeTip, buf);
    POINT pt = { m_volHotX, -30 };
    ClientToScreen(g_hVolume, &pt);
    SetWindowPos(m_hTimeTip, HWND_TOPMOST, pt.x - 30, pt.y, 60, 22,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

LRESULT CALLBACK VideoPlayerGUI::VolumeSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                                    LPARAM lParam, UINT_PTR uIdSubclass,
                                                    DWORD_PTR dwRefData) {
    VideoPlayerGUI* self = reinterpret_cast<VideoPlayerGUI*>(dwRefData);
    switch (uMsg) {
        case WM_ERASEBKGND:
            if (self) {
                HDC hdc = (HDC)wParam;
                RECT rcE;
                GetClientRect(hwnd, &rcE);
                HBRUSH hBg = CreateSolidBrush(self->COLOR_MODERN_BG);
                FillRect(hdc, &rcE, hBg);
                DeleteObject(hBg);
            }
            return 1;

        case WM_LBUTTONDOWN:
            if (self) {
                SetCapture(hwnd);
                self->m_volDrag = true;
                RECT rc;
                GetClientRect(hwnd, &rc);
                int x   = (short)LOWORD(lParam);
                int pos = (rc.right > 0) ? (int)(((double)x / rc.right) * self->VOL_MAX) : 0;
                if (pos < 0) pos = 0;                     // [FIX] capture keluar area
                if (pos > self->VOL_MAX) pos = self->VOL_MAX;
                SendMessage(hwnd, TBM_SETPOS, TRUE, pos);
                self->m_volHotX = x;
                self->ApplyVolumeFromSlider(pos);
                InvalidateRect(hwnd, nullptr, FALSE);
                self->ShowVolTip(pos);
            }
            return 0;

        case WM_MOUSEMOVE: {
            if (!self) break;
            int x = (short)LOWORD(lParam);
            if (self->m_volDrag && GetCapture() == hwnd) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                int pos = (rc.right > 0) ? (int)(((double)x / rc.right) * self->VOL_MAX) : 0;
                if (pos < 0) pos = 0;                     // [FIX] drag melewati tepi
                if (pos > self->VOL_MAX) pos = self->VOL_MAX;
                SendMessage(hwnd, TBM_SETPOS, TRUE, pos);
                self->m_volHotX = x;
                self->ApplyVolumeFromSlider(pos);
                InvalidateRect(hwnd, nullptr, FALSE);
                self->ShowVolTip(pos);
            } else {
                TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
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
// VOLUME BAR — gradasi hijau(0%) -> kuning -> merah(150%), notch di 100%
// ==========================================
void VideoPlayerGUI::DrawVlcVolumeBar(HDC hdc) {
    RECT rc;
    GetClientRect(g_hVolume, &rc);

    HBRUSH hBg = CreateSolidBrush(COLOR_MODERN_BG);
    FillRect(hdc, &rc, hBg);
    DeleteObject(hBg);

    int pos = (int)SendMessage(g_hVolume, TBM_GETPOS, 0, 0);
    const int BAR_H = 5;
    const int THUMB_SIZE = (m_volHot || m_volDrag) ? 14 : 12;
    int cy = (rc.top + rc.bottom) / 2;
    RECT track = { rc.left + 2, cy - BAR_H / 2, rc.right - 3, cy + BAR_H / 2 };
    int w = track.right - track.left;

    HPEN hNullPen = CreatePen(PS_NULL, 0, 0);
    HGDIOBJ hOldPen = SelectObject(hdc, hNullPen);

    HBRUSH hTrack = CreateSolidBrush(COLOR_SEEK_TRACK);
    HGDIOBJ hOldBr = SelectObject(hdc, hTrack);
    RoundRect(hdc, track.left, track.top, track.right, track.bottom, BAR_H, BAR_H);

    double ratio = (double)pos / VOL_MAX;
    if (ratio < 0) ratio = 0;
    if (ratio > 1) ratio = 1;
    COLORREF cFill = (ratio < 0.6667)
        ? LerpColor(RGB(60, 170, 70),  RGB(255, 200, 40), ratio / 0.6667)
        : LerpColor(RGB(255, 200, 40), RGB(225, 55, 55), (ratio - 0.6667) / 0.3333);

    int fx = track.left + (int)(ratio * w);
    if (fx > track.left + BAR_H) {
        HBRUSH hFill = CreateSolidBrush(cFill);
        SelectObject(hdc, hFill);
        RoundRect(hdc, track.left, track.top + 1, fx, track.bottom - 1,
                  BAR_H - 2, BAR_H - 2);
        DeleteObject(hFill);
    }

    // Notch penanda 100% = batas masuk zona boost
    int notch = track.left + (int)(w * (100.0 / VOL_MAX));
    HPEN hNotch = CreatePen(PS_SOLID, 1, RGB(150, 150, 150));
    SelectObject(hdc, hNotch);
    MoveToEx(hdc, notch, track.top - 1, nullptr);
    LineTo(hdc, notch, track.bottom + 1);
    DeleteObject(hNotch);

    RECT thumb = { fx - THUMB_SIZE / 2, cy - THUMB_SIZE / 2,
                   fx + THUMB_SIZE / 2, cy + THUMB_SIZE / 2 };
    HBRUSH hThumb = CreateSolidBrush(RGB(255, 255, 255));
    SelectObject(hdc, hThumb);
    FillRect(hdc, &thumb, hThumb);
    DeleteObject(hThumb);

    HPEN hBorderPen = CreatePen(PS_SOLID, 1, RGB(180, 180, 180));
    SelectObject(hdc, hBorderPen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, thumb.left, thumb.top, thumb.right, thumb.bottom);
    DeleteObject(hBorderPen);

    SelectObject(hdc, hOldBr);
    SelectObject(hdc, hOldPen);
    DeleteObject(hNullPen);
}

// ==========================================
// SCRUBBING — klik mulai drag, drag real-time, lepas = seek final
// ==========================================
void VideoPlayerGUI::SeekFromTrackbarClick(int mouseX) {
    RECT rc;
    GetClientRect(g_hProgress, &rc);
    if (rc.right <= 0) return;
    // untuk mencegah seek ke 0 apabila durasinya belum diketahui
    if (m_cachedDuration <= 0.0) return;

    double ratio = static_cast<double>(mouseX) / static_cast<double>(rc.right);
    if (ratio < 0.0) ratio = 0.0;
    if (ratio > 1.0) ratio = 1.0;

    m_hotX = mouseX;
    m_isDraggingProgress = true;

    int newPos = static_cast<int>(ratio * m_progressRangeMax);
    SetProgressPos(newPos);
    UpdateSeekFromPos(newPos);
}

void VideoPlayerGUI::DragSeekTo(int x) {
    RECT rc;
    GetClientRect(g_hProgress, &rc);
    if (rc.right <= 0 || m_cachedDuration <= 0.0) return;

    double ratio = static_cast<double>(x) / static_cast<double>(rc.right);
    if (ratio < 0.0) ratio = 0.0;
    if (ratio > 1.0) ratio = 1.0;

    m_hotX = x;
    int newPos = static_cast<int>(ratio * m_progressRangeMax);
    SetProgressPos(newPos);
    UpdateSeekFromPos(newPos);
}

void VideoPlayerGUI::UpdateSeekFromPos(int pos) {
    if (m_cachedDuration <= 0.0) return;

    double t = (static_cast<double>(pos) / m_progressRangeMax) * m_cachedDuration;
    UpdateTimeLabel(t, m_cachedDuration);
    ShowTimeTip(t);

    DWORD now = GetTickCount();
    if (now - m_lastSeekTick > 120) {   // throttled seek = smooth scrubbing
        m_player.Seek(t);
        m_lastSeekTick = now;
    }
}

void VideoPlayerGUI::EndSeekDrag() {
    if (!m_isDraggingProgress) return;
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
    if (!m_hTimeTip) return;

    wchar_t buf[24];
    int s = (int)seconds;
    if (s >= 3600)
        swprintf_s(buf, L"%d:%02d:%02d", s / 3600, (s % 3600) / 60, s % 60);
    else
        swprintf_s(buf, L"%02d:%02d", s / 60, s % 60);
    SetWindowText(m_hTimeTip, buf);

    POINT pt = { m_hotX, -30 };
    ClientToScreen(g_hProgress, &pt);
    SetWindowPos(m_hTimeTip, HWND_TOPMOST, pt.x - 40, pt.y, 80, 22,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void VideoPlayerGUI::HideTimeTip() {
    if (m_hTimeTip) ShowWindow(m_hTimeTip, SW_HIDE);
}

// ==========================================
// VIDEO AREA SUBCLASS — double-click untuk toggle fullscreen
// (STATIC tidak punya CS_DBLCLKS, jadi dideteksi manual via GetTickCount)
// ==========================================
LRESULT CALLBACK VideoPlayerGUI::VideoAreaSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                                       UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    VideoPlayerGUI* self = reinterpret_cast<VideoPlayerGUI*>(dwRefData);
    switch (uMsg) {
        case WM_LBUTTONDOWN:
            if (self) {
                DWORD now = GetTickCount();
                short x = (short)LOWORD(lParam), y = (short)HIWORD(lParam);
                if (now - self->m_lastVideoClickTick < GetDoubleClickTime() &&
                    abs(x - self->m_lastVideoClickX) < 4 &&
                    abs(y - self->m_lastVideoClickY) < 4) {
                    self->m_lastVideoClickTick = 0;
                    if (self->m_isFullscreen) self->ExitFullscreen();
                    else                      self->EnterFullscreen();
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
// MEDIA READY — set range trackbar sesuai durasi asli video
// ==========================================
void VideoPlayerGUI::OnMediaReady() {
    m_cachedDuration = m_player.GetDuration();
    double dur = m_cachedDuration;
    int range = static_cast<int>(dur * 10.0);
    if (range < 100) range = 100;

    m_progressRangeMax = range;
    SendMessage(g_hProgress, TBM_SETRANGEMIN, TRUE, 0);
    SendMessage(g_hProgress, TBM_SETRANGEMAX, TRUE, m_progressRangeMax);
    SetProgressPos(0);
    FitWindowToVideo();

    // Frame pertama langsung dipaksa render, baru video ditampilkan
    m_player.ForceFrameRefresh();
    m_player.ShowVideoWindow();
}

// ==========================================
// COMMAND HANDLER
// ==========================================
void VideoPlayerGUI::OnCommand(WPARAM wParam, LPARAM lParam) {
    switch (LOWORD(wParam)) {
        case IDC_BTN_PLAY:
        case IDM_PLAYBACK_PLAY:
            if (m_isPlaying) {
                m_player.Pause();
                SetPlayPauseUI(false);
            } else {
                m_player.Play();
                SetPlayPauseUI(true);
            }
            break;

        case IDC_BTN_PAUSE:
            m_player.Pause();
            SetPlayPauseUI(false);
            break;

        case IDC_BTN_STOP:
        case IDM_PLAYBACK_STOP:
            m_player.Stop();
            SetPlayPauseUI(false);
            SetProgressPos(0);
            m_cachedDuration = 0.0;
            break;

        case IDC_BTN_SKIPBACK:
        case IDM_PLAYBACK_SKIPBACK: {
            double pos = m_player.GetPosition();
            m_player.Seek(pos > 10.0 ? pos - 10.0 : 0.0);
            break;
        }
        case IDC_BTN_SKIPFORWARD:
        case IDM_PLAYBACK_SKIPFWD: {
            double pos = m_player.GetPosition();
            m_player.Seek((pos + 10.0 < m_cachedDuration) ? pos + 10.0 : m_cachedDuration);
            break;
        }

        case IDM_FILE_OPEN:
            OpenFileDialog();
            break;

        case IDM_FILE_EXIT:
            PostMessage(g_hMainWnd, WM_CLOSE, 0, 0);
            break;

        case IDM_AUDIO_VOLUP: {
            int vol = (int)SendMessage(g_hVolume, TBM_GETPOS, 0, 0);
            vol = (vol + 10 > VOL_MAX) ? VOL_MAX : vol + 10;
            SendMessage(g_hVolume, TBM_SETPOS, TRUE, vol);
            ApplyVolumeFromSlider(vol);
            break;
        }
        case IDM_AUDIO_VOLDOWN: {
            int vol = (int)SendMessage(g_hVolume, TBM_GETPOS, 0, 0);
            vol = (vol - 10 < 0) ? 0 : vol - 10;
            SendMessage(g_hVolume, TBM_SETPOS, TRUE, vol);
            ApplyVolumeFromSlider(vol);
            break;
        }
        case IDM_AUDIO_MUTE:
            ToggleMute();
            break;

        case IDM_VIEW_FULLSCREEN:
            if(m_isFullscreen) ExitFullscreen();
            else EnterFullscreen();
            break;

        case IDM_APP_ESCAPE:
            if (m_isFullscreen) ExitFullscreen();
            break;

        case IDM_HELP_ABOUT:
            MessageBox(g_hMainWnd,
                L"Vidi Video Player\nDibangun dengan Win32 + Media Foundation",
                L"About Vidi", MB_OK | MB_ICONINFORMATION);
            break;
    }
}

// ==========================================
// TOGGLE MUTE — bisukan / aktifkan suara
// ==========================================
void VideoPlayerGUI::ToggleMute() {
    if (!m_isMuted) {
        m_lastVolume = SendMessage(g_hVolume, TBM_GETPOS, 0, 0) / 100.0f;   // 0..1.5
        m_player.SetVolume(0.0f);
        m_player.SetDspGain(1.0f);          // matikan juga boost saat mute
        SendMessage(g_hVolume, TBM_SETPOS, TRUE, 0);
        InvalidateRect(g_hVolume, nullptr, FALSE);
        m_isMuted = true;
    } else {
        int back = (int)(m_lastVolume * 100.0f + 0.5f);
        SendMessage(g_hVolume, TBM_SETPOS, TRUE, back);
        ApplyVolumeFromSlider(back);
        InvalidateRect(g_hVolume, nullptr, FALSE);
        m_isMuted = false;
    }
}

void VideoPlayerGUI::EnterFullscreen() {
    if (m_isFullscreen) return;

    MONITORINFO mi = { sizeof(mi) };
    HMONITOR mon = MonitorFromWindow(g_hMainWnd, MONITOR_DEFAULTTONEAREST);
    if (!GetWindowPlacement(g_hMainWnd, &m_prevPlacement) ||
        !GetMonitorInfo(mon, &mi))
        return;

    DWORD style = GetWindowLong(g_hMainWnd, GWL_STYLE);
    SetWindowLong(g_hMainWnd, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
    if (m_hMenuBar) SetMenu(g_hMainWnd, nullptr);   // menubar ikut hilang di fullscreen
    SetWindowPos(g_hMainWnd, HWND_TOP,
        mi.rcMonitor.left, mi.rcMonitor.top,
        mi.rcMonitor.right - mi.rcMonitor.left,
        mi.rcMonitor.bottom - mi.rcMonitor.top,
        SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

    m_isFullscreen = true;
    PokeOSControls();   // kontrol langsung terlihat + mulai hitung idle
}
void VideoPlayerGUI::ExitFullscreen() {
    if (!m_isFullscreen) return;

    SetWindowLong(g_hMainWnd, GWL_STYLE,
                  GetWindowLong(g_hMainWnd, GWL_STYLE) | WS_OVERLAPPEDWINDOW);
    SetWindowPlacement(g_hMainWnd, &m_prevPlacement);
    if (m_hMenuBar) SetMenu(g_hMainWnd, m_hMenuBar);   // kembalikan menubar
    SetWindowPos(g_hMainWnd, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

    m_isFullscreen = false;
    KillTimer(g_hMainWnd, ID_TIMER_OSI_HIDE);
    ShowOSControls(true);
    if (m_cursorHidden) {
        ShowCursor(TRUE);
        m_cursorHidden = false;
    }
}

void VideoPlayerGUI::FitWindowToVideo() {
    if (m_isFullscreen || !g_hMainWnd) return;

    int vw = 0, vh = 0;
    m_player.GetNativeVideoSize(vw, vh);
    if (vw <= 0 || vh <= 0) return;

    HMONITOR mon = MonitorFromWindow(g_hMainWnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    if (!GetMonitorInfo(mon, &mi)) return;
    int availW = mi.rcWork.right - mi.rcWork.left;
    int availH = mi.rcWork.bottom - mi.rcWork.top;

    // Ukuran native video; hanya mengecil bila tak muat 90% work area (tidak upscale)
    double scaleX = ((double)availW * 0.9) / vw;
    double scaleY = ((double)availH * 0.9) / vh;
    double scale = (scaleX < scaleY) ? scaleX : scaleY;
    if (scale > 1.0) scale = 1.0;
    int cw = (int)(vw * scale + 0.5);
    int ch = (int)(vh * scale + 0.5);

    // Delta chrome (menu bar + border) dari ukuran window saat ini → aman utk DPI
    RECT rcC, rcW;
    GetClientRect(g_hMainWnd, &rcC);
    GetWindowRect(g_hMainWnd, &rcW);
    int extraW = (rcW.right - rcW.left) - rcC.right;
    int extraH = (rcW.bottom - rcW.top) - rcC.bottom;

    int newX = mi.rcWork.left + (availW - (cw + extraW)) / 2;
    int newY = mi.rcWork.top  + (availH - (ch + extraH)) / 2;
    SetWindowPos(g_hMainWnd, nullptr, newX, newY, cw + extraW, ch + extraH,
                 SWP_NOZORDER);
}

void VideoPlayerGUI::LayoutFullscreen(int width, int height) {
    const int MARGINX  = 12;
    const int EDGE     = 12;
    const int BTN_SIZE = 36;
    const int SP       = 4;

    int btnRowY = height - BTN_SIZE - 8;
    int progressY = btnRowY - 26;
    const int VOL_W        = 96;
    const int VOL_TIME_GAP = 8;
    int timeW = CurrentTimeLabelWidth(g_hTimeLabel, m_hTimeFont);
    int timeX = width - EDGE - timeW;
    int volX  = timeX - VOL_TIME_GAP - VOL_W;
    int volY  = btnRowY + (BTN_SIZE - 24) / 2;
    HDWP dwp = BeginDeferWindowPos(9);
    HWND hAfter = nullptr;

    // Video area memenuhi layar
    if (g_hVideoArea)
        SetWindowPos(g_hVideoArea, NULL, 0, 0, width, height,
                     SWP_NOZORDER | SWP_SHOWWINDOW);
    m_player.UpdateVideoSize();

    // Progress bar full-width
    if (g_hProgress)
        SetWindowPos(g_hProgress, NULL, MARGINX, progressY,
                     width - MARGINX * 2, 18, SWP_NOZORDER | SWP_SHOWWINDOW);

    // Tombol playback di-center horizontal
    int stripW = BTN_SIZE * 5 + SP * 4;
    int bx = (width - stripW) / 2;
    HWND btns[5] = { g_hSkipBack, g_hPlayBtn, g_hPauseBtn, g_hStopBtn, g_hSkipForward };
    for (HWND h : btns) {
        if (h)
            SetWindowPos(h, NULL, bx, btnRowY, BTN_SIZE, BTN_SIZE,
                         SWP_NOZORDER | SWP_SHOWWINDOW);
        bx += BTN_SIZE + SP;
    }

    if (g_hVolume) {
        dwp = DeferWindowPos(dwp, g_hVolume, hAfter ? hAfter : HWND_TOP,
                             volX, volY, VOL_W, 24, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        hAfter = g_hVolume;
    }
    if (g_hTimeLabel) {
        dwp = DeferWindowPos(dwp, g_hTimeLabel, hAfter ? hAfter : HWND_TOP,
                             timeX, volY, timeW, 24, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
    if (dwp) EndDeferWindowPos(dwp);

    m_player.UpdateVideoSize();
    InvalidateRect(g_hProgress, nullptr, TRUE);

}

void VideoPlayerGUI::ShowOSControls(bool visible) {
    int cmd = visible ? SW_SHOW : SW_HIDE;
    HWND ctrls[] = { g_hProgress, g_hSkipBack, g_hPlayBtn, g_hPauseBtn,
                     g_hStopBtn, g_hSkipForward, g_hVolume, g_hTimeLabel };
    for (HWND h : ctrls)
        if (h) ShowWindow(h, cmd);
}

void VideoPlayerGUI::PokeOSControls() {
    if (!m_isFullscreen || !g_hMainWnd) return;

    if (m_cursorHidden) {
        ShowCursor(TRUE);
        m_cursorHidden = false;
    }
    ShowOSControls(true);
    // One-shot: kalau 2.5 detik tak ada gerakan, WM_TIMER_OSI_HIDE menyembunyikan
    SetTimer(g_hMainWnd, ID_TIMER_OSI_HIDE, 2500, nullptr);
}

// ==========================================
// RECOVER VIDEO — pulihkan tampilan setelah session switch/minimize/ganti resolusi
// ==========================================
void VideoPlayerGUI::RecoverVideo() {
    if (m_isPlaying) {
        m_player.Play();                              // pastikan graph Running lagi
    } else if (m_cachedDuration > 0.0) {
        m_player.ForceFrameRefresh();                 // paused: paksa render 1 frame
    }
}
// ==========================================
// HSCROLL — keyboard arrow di trackbar (drag ditangani subclass)
// ==========================================
void VideoPlayerGUI::OnHScroll(WPARAM wParam, LPARAM lParam) {
    HWND hCtrl = (HWND)lParam;
    int code = LOWORD(wParam);

    if (hCtrl == g_hProgress) {
        if (code == TB_THUMBTRACK || code == TB_THUMBPOSITION || code == TB_ENDTRACK) {
            if (m_isDraggingProgress) return;  // drag mouse sudah ditangani subclass
            int pos = (int)SendMessage(g_hProgress, TBM_GETPOS, 0, 0);
            SetProgressPos(pos);
            UpdateSeekFromPos(pos);
        }
    }
    else if (hCtrl == g_hVolume) {
        int vol = (int)SendMessage(g_hVolume, TBM_GETPOS, 0, 0);
        ApplyVolumeFromSlider(vol);
        InvalidateRect(g_hVolume, nullptr, FALSE);
    }
}

// ==========================================
// HELPER — set posisi progress + paksa repaint penuh channel
// ==========================================
void VideoPlayerGUI::SetProgressPos(int pos) {
    SendMessage(g_hProgress, TBM_SETPOS, TRUE, pos);
    InvalidateRect(g_hProgress, nullptr, TRUE);  // kunci fix: repaint SELURUH channel
}

// ==========================================
// TIMER TICK — auto update posisi & label
// ==========================================
void VideoPlayerGUI::OnTimerTick() {
    if (m_isFullscreen) {
        POINT pt;
        GetCursorPos(&pt);
        if (pt.x != m_lastCursor.x || pt.y != m_lastCursor.y) {
            m_lastCursor = pt;
            PokeOSControls();
        }
    }
    if (m_isDraggingProgress) return;
    DWORD now = GetTickCount();
    if (now - m_lastDurCheckTick > 500) {
        m_lastDurCheckTick = now;
        double fresh = m_player.GetDuration();
        if (fresh > 0.0 && fabs(fresh - m_cachedDuration) > 0.5) {
            m_cachedDuration = fresh;
            int range = static_cast<int>(fresh * 10.0);
            if (range < 100) range = 100;
            m_progressRangeMax = range;
            SendMessage(g_hProgress, TBM_SETRANGEMIN, TRUE, 0);
            SendMessage(g_hProgress, TBM_SETRANGEMAX, TRUE, m_progressRangeMax);
        }
    }
    double dur = m_cachedDuration;
    if (dur <= 0.0) return;
    if (m_hasPendingSeek) {
        double actualPos = m_player.GetPosition();
        DWORD elapsed = GetTickCount() - m_pendingSeekStartTick;
        bool settled = (fabs(actualPos - m_pendingSeekTarget) < 1.0) || (elapsed > 1500);

        if (settled) {
            m_hasPendingSeek = false;
        } else {
            if (dur > 0.0) {
                int sliderPos = static_cast<int>((m_pendingSeekTarget / dur) * m_progressRangeMax);
                SetProgressPos(sliderPos);
            }
            UpdateTimeLabel(m_pendingSeekTarget, dur);
            return;
        }
    }

    double pos = m_player.GetPosition();
    int sliderPos = static_cast<int>((pos / dur) * m_progressRangeMax);
    if (sliderPos < 0) sliderPos = 0;
    if (sliderPos > m_progressRangeMax) sliderPos = m_progressRangeMax;
    SetProgressPos(sliderPos);
    UpdateTimeLabel(pos, dur);
}

void VideoPlayerGUI::UpdateTimeLabel(double posSeconds, double durSeconds) {
    wchar_t buf[64];
    int p = (int)posSeconds, d = (int)durSeconds;
    if (d >= 3600)
        swprintf_s(buf, L"%d:%02d:%02d / %d:%02d:%02d",
                   p / 3600, (p % 3600) / 60, p % 60,
                   d / 3600, (d % 3600) / 60, d % 60);
    else
        swprintf_s(buf, L"%02d:%02d / %02d:%02d",
                   p / 60, p % 60, d / 60, d % 60);

    // Skip bila teks identik (hemat repaint tiap tick timer)
    wchar_t prev[64] = {};
    GetWindowTextW(g_hTimeLabel, prev, 64);
    if (wcscmp(prev, buf) == 0) return;

    SetWindowTextW(g_hTimeLabel, buf);
    InvalidateRect(g_hTimeLabel, nullptr, TRUE);

    // Format berubah panjang (menit -> jam): label harus melebar SEGERA,
    // tanpa menunggu user resize window
    int needed = MeasureStringWidth(g_hTimeLabel, m_hTimeFont, buf);
    RECT rc;
    GetWindowRect(g_hTimeLabel, &rc);
    if (needed > (rc.right - rc.left) && g_hMainWnd) {
        RECT rcC;
        GetClientRect(g_hMainWnd, &rcC);
        LayoutControls(rcC.right, rcC.bottom);
    }
}

void VideoPlayerGUI::SetPlayPauseUI(bool playing) {
    m_isPlaying = playing;
    EnableWindow(g_hPlayBtn, !playing);
    EnableWindow(g_hPauseBtn, playing);
    // Cegah screensaver/layar mati selama video berjalan
    SetThreadExecutionState(playing ? (ES_CONTINUOUS | ES_DISPLAY_REQUIRED)
                                    : ES_CONTINUOUS);
}

void VideoPlayerGUI::OpenFileDialog() {
    wchar_t filePath[MAX_PATH] = {0};
    OPENFILENAME ofn = {};

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hMainWnd;
    ofn.lpstrFilter =
        L"Video Files\0*.mp4;*.mkv;*.avi;*.mov;*.wmv;*.webm;*.m4v;*.ts;*.flv\0"
        L"Audio Files\0*.mp3;*.aac;*.flac;*.wav;*.ogg\0"
        L"All Files\0*.*\0\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

    if (!GetOpenFileName(&ofn)) return;

    // Reset state pemutar lama sebelum buka file baru
    m_player.Stop();
    SetPlayPauseUI(false);
    SetProgressPos(0);
    UpdateTimeLabel(0.0, 0.0);
    m_cachedDuration = 0.0;
    m_hasPendingSeek = false;
    m_isDraggingProgress = false;

    if (m_player.OpenFile(filePath)) {
        m_player.Play();
        SetPlayPauseUI(true);
    } else {
        MessageBox(g_hMainWnd,
            L"Gagal membuka file. Format mungkin tidak didukung.",
            L"Vidi", MB_OK | MB_ICONERROR);
    }
}


// ==========================================
// CREATE MENU BAR
// ==========================================
void VideoPlayerGUI::CreateMenuBar(HWND hwnd) {
    HMENU hMenuBar = CreateMenu();

    // --- Media ---
    HMENU hMedia = CreatePopupMenu();
    AppendMenu(hMedia, MF_STRING, IDM_FILE_OPEN, L"Open...\tCtrl+O");
    AppendMenu(hMedia, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hMedia, MF_STRING, IDM_FILE_EXIT, L"Exit\tAlt+F4");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hMedia, L"Media");

    // --- Playback ---
    HMENU hPlayback = CreatePopupMenu();
    AppendMenu(hPlayback, MF_STRING, IDM_PLAYBACK_PLAY, L"Play/Pause\tSpace");
    AppendMenu(hPlayback, MF_STRING, IDM_PLAYBACK_STOP, L"Stop\tS");
    AppendMenu(hPlayback, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hPlayback, MF_STRING, IDM_PLAYBACK_SKIPBACK, L"Skip Back 10s\tLeft");
    AppendMenu(hPlayback, MF_STRING, IDM_PLAYBACK_SKIPFWD, L"Skip Forward 10s\tRight");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hPlayback, L"Playback");

    // --- Audio ---
    HMENU hAudio = CreatePopupMenu();
    AppendMenu(hAudio, MF_STRING, IDM_AUDIO_VOLUP, L"Volume Up\tUp");
    AppendMenu(hAudio, MF_STRING, IDM_AUDIO_VOLDOWN, L"Volume Down\tDown");
    AppendMenu(hAudio, MF_STRING, IDM_AUDIO_MUTE, L"Mute\tM");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hAudio, L"Audio");

    // --- View ---
    HMENU hView = CreatePopupMenu();
    AppendMenu(hView, MF_STRING, IDM_VIEW_FULLSCREEN, L"Toggle Fullscreen\tF");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hView, L"View");

    // --- Help ---
    HMENU hHelp = CreatePopupMenu();
    AppendMenu(hHelp, MF_STRING, IDM_HELP_ABOUT, L"About");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hHelp, L"Help");

    m_hMenuBar = hMenuBar;
    SetMenu(hwnd, hMenuBar);
}

// ==========================================
// CREATE CONTROLS
// ==========================================
void VideoPlayerGUI::CreateControls(HWND hwnd) {
    g_hMainWnd = hwnd;
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);

    // Font Modern
    m_hModernFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, 
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
    m_hTimeFont = CreateFontW(-15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, 
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
    // Font khusus tooltip (lebih kecil agar pas vertikal di kotak 22px)
    m_hTipFont = CreateFontW(-12, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");

    // Load Icons
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring exeDir = exePath;
    exeDir = exeDir.substr(0, exeDir.find_last_of(L'\\') + 1);
    std::wstring assetsDir = exeDir + L"assets\\";

    m_hIconPlay = (HICON)LoadImageW(nullptr, (assetsDir + L"play-button-arrowhead.ico").c_str(), IMAGE_ICON, 24, 24, LR_LOADFROMFILE);
    m_hIconPause = (HICON)LoadImageW(nullptr, (assetsDir + L"pause.ico").c_str(), IMAGE_ICON, 24, 24, LR_LOADFROMFILE);
    m_hIconStop = (HICON)LoadImageW(nullptr, (assetsDir + L"stop-button.ico").c_str(), IMAGE_ICON, 24, 24, LR_LOADFROMFILE);
    m_hIconSkipBack = (HICON)LoadImageW(nullptr, (assetsDir + L"left-arrow.ico").c_str(), IMAGE_ICON, 24, 24, LR_LOADFROMFILE);
    m_hIconSkipForward = (HICON)LoadImageW(nullptr, (assetsDir + L"fast-forward.ico").c_str(), IMAGE_ICON, 24, 24, LR_LOADFROMFILE);

    // [PENTING] Semua kontrol dibuat dengan posisi (0,0) dan ukuran 0,0
    // LayoutControls akan mengatur posisi yang benar nanti
    
    // Video Area
    g_hVideoArea = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"", 
        WS_CHILD | WS_VISIBLE | SS_BLACKRECT, 
        0, 0, 100, 100, hwnd, nullptr, nullptr, nullptr);
    SetWindowSubclass(g_hVideoArea, VideoAreaSubclassProc, 2, (DWORD_PTR)this);

    // Buttons - dibuat dulu dengan posisi placeholder
    DWORD btnStyle = WS_CHILD | BS_ICON | BS_FLAT;
    
    g_hSkipBack = CreateWindowW(L"BUTTON", L"", btnStyle, 
        0, 0, 44, 44, hwnd, (HMENU)IDC_BTN_SKIPBACK, nullptr, nullptr);
    if (m_hIconSkipBack) SendMessage(g_hSkipBack, BM_SETIMAGE, IMAGE_ICON, (LPARAM)m_hIconSkipBack);
    
    g_hPlayBtn = CreateWindowW(L"BUTTON", L"", btnStyle, 
        0, 0, 48, 48, hwnd, (HMENU)IDC_BTN_PLAY, nullptr, nullptr);
    if (m_hIconPlay) SendMessage(g_hPlayBtn, BM_SETIMAGE, IMAGE_ICON, (LPARAM)m_hIconPlay);
    
    g_hPauseBtn = CreateWindowW(L"BUTTON", L"", btnStyle, 
        0, 0, 48, 48, hwnd, (HMENU)IDC_BTN_PAUSE, nullptr, nullptr);
    if (m_hIconPause) SendMessage(g_hPauseBtn, BM_SETIMAGE, IMAGE_ICON, (LPARAM)m_hIconPause);
    
    g_hStopBtn = CreateWindowW(L"BUTTON", L"", btnStyle, 
        0, 0, 44, 44, hwnd, (HMENU)IDC_BTN_STOP, nullptr, nullptr);
    if (m_hIconStop) SendMessage(g_hStopBtn, BM_SETIMAGE, IMAGE_ICON, (LPARAM)m_hIconStop);
    
    g_hSkipForward = CreateWindowW(L"BUTTON", L"", btnStyle, 
        0, 0, 44, 44, hwnd, (HMENU)IDC_BTN_SKIPFORWARD, nullptr, nullptr);
    
    if (m_hIconSkipForward) SendMessage(g_hSkipForward, BM_SETIMAGE, IMAGE_ICON, (LPARAM)m_hIconSkipForward);

    // Progress Bar (VLC-style)
    g_hProgress = CreateWindowExW(0, TRACKBAR_CLASS, L"", 
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS, 
        0, 0, 100, 24, hwnd, (HMENU)IDC_PROGRESS, nullptr, nullptr);
    SetWindowTheme(g_hProgress, L" ", L" ");
    SendMessage(g_hProgress, TBM_SETRANGEMIN, TRUE, 0);
    SendMessage(g_hProgress, TBM_SETRANGEMAX, TRUE, m_progressRangeMax);
    // [FIX] Sebelumnya: TBM_SETTHUMBLENGTH dengan wParam=0 -> thumb length 0,
    // trackbar tidak tergambar sama sekali. Thumb sistem disembunyikan lewat
    // custom draw (CDRF_SKIPDEFAULT), jadi cukup set panjang wajar.
    SendMessage(g_hProgress, TBM_SETTHUMBLENGTH, 12, 0);
    SetWindowSubclass(g_hProgress, ProgressSubclassProc, 1, (DWORD_PTR)this);

    // Tooltip waktu seekbar (popup gelap di atas cursor)
    m_hTimeTip = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        L"STATIC", L"", WS_POPUP | SS_CENTER | SS_CENTERIMAGE,
        0, 0, 80, 22, hwnd, nullptr, nullptr, nullptr);
    SetWindowLongPtr(m_hTimeTip, -8 /* GWL_HWNDPARENT */, (LONG_PTR)hwnd);  // route WM_CTLCOLORSTATIC
    SendMessage(m_hTimeTip, WM_SETFONT, (WPARAM)m_hTipFont, TRUE);

    // Volume Slider [0-150%, custom draw gradasi hijau->merah]
    g_hVolume = CreateWindowExW(0, TRACKBAR_CLASS, L"",
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
        0, 0, 100, 24, hwnd, (HMENU)IDC_VOLUME, nullptr, nullptr);
    SetWindowTheme(g_hVolume, L" ", L" ");
    SendMessage(g_hVolume, TBM_SETRANGEMIN, TRUE, 0);
    SendMessage(g_hVolume, TBM_SETRANGEMAX, TRUE, VOL_MAX);
    SendMessage(g_hVolume, TBM_SETPOS, TRUE, 100);   // default 100% (unity)
    SetWindowSubclass(g_hVolume, VolumeSubclassProc, 3, (DWORD_PTR)this);

    // Time Label
    g_hTimeLabel = CreateWindowW(L"STATIC", L"00:00 / 00:00", 
        WS_CHILD | SS_RIGHT, 
        0, 0, 150, 30, hwnd, (HMENU)IDC_TIME_LABEL, nullptr, nullptr);

    // Apply Fonts
    HWND hCtrl = GetWindow(hwnd, GW_CHILD);
    while (hCtrl) {
        SendMessage(hCtrl, WM_SETFONT, (hCtrl == g_hTimeLabel) ? (WPARAM)m_hTimeFont : (WPARAM)m_hModernFont, TRUE);
        hCtrl = GetNextWindow(hCtrl, GW_HWNDNEXT);
    }

    m_player.Initialize(g_hVideoArea, hwnd);
    SetTimer(hwnd, ID_TIMER_UPDATE, TIMER_INTERVAL_MS, nullptr);
}


// ==========================================
// INITIALIZE & RUN
// ==========================================
bool VideoPlayerGUI::Initialize(HINSTANCE hInstance, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"VideoPlayerWindow";

    WNDCLASS wc = {};
    wc.lpfnWndProc = VideoPlayerGUI::WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClass(&wc)) return false;

    g_hMainWnd = CreateWindowEx(
        0, CLASS_NAME, L"Video Player UI", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 620, 470,
        NULL, NULL, hInstance, this
    );

    if (!g_hMainWnd) return false;

    m_hAccel = CreatePlayerAccelTable();

    // Terima notifikasi lock/unlock/switch sesi (Win+L, RDP, fast user switching)
    WTSRegisterSessionNotification(g_hMainWnd, NOTIFY_FOR_THIS_SESSION);

    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);
    return true;
}

HACCEL VideoPlayerGUI::CreatePlayerAccelTable() {
    ACCEL acc[] = {
        { FVIRTKEY | FCONTROL | FNOINVERT, 'O',       IDM_FILE_OPEN         }, // Ctrl+O
        { FVIRTKEY | FNOINVERT,            VK_SPACE,  IDM_PLAYBACK_PLAY     }, // play/pause
        { FVIRTKEY | FNOINVERT,            'S',       IDM_PLAYBACK_STOP     },
        { FVIRTKEY | FNOINVERT,            VK_LEFT,   IDM_PLAYBACK_SKIPBACK },
        { FVIRTKEY | FNOINVERT,            VK_RIGHT,  IDM_PLAYBACK_SKIPFWD  },
        { FVIRTKEY | FNOINVERT,            VK_UP,     IDM_AUDIO_VOLUP       },
        { FVIRTKEY | FNOINVERT,            VK_DOWN,   IDM_AUDIO_VOLDOWN     },
        { FVIRTKEY | FNOINVERT,            'M',       IDM_AUDIO_MUTE        },
        { FVIRTKEY | FNOINVERT,            'F',       IDM_VIEW_FULLSCREEN   },
        { FVIRTKEY | FNOINVERT,            VK_ESCAPE, IDM_APP_ESCAPE        },
    };
    return CreateAcceleratorTable(acc, ARRAYSIZE(acc));
}

int VideoPlayerGUI::Run() {
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (!TranslateAccelerator(g_hMainWnd, m_hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return (int)msg.wParam;
}

} // namespace guiVidi
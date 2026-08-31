#include "../../include/gui/gui.hh"
#include "../../include/kernels/ids.hh"
#include <wtsapi32.h>
#include <dwmapi.h>
#include <string>
#include <cmath>

namespace guiVidi {

// ==========================================
// LAYOUT CONTROLS
// ==========================================
int MeasureStringWidth(HWND hwndRef, HFONT hFont, const wchar_t* text) {
    if (!hwndRef || !text || !*text)
        return 60; // fallback lebar minimal

    HDC hdc = GetDC(hwndRef);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont ? hFont : (HFONT)GetStockObject(DEFAULT_GUI_FONT));

    SIZE sz = {0};
    GetTextExtentPoint32W(hdc, text, (int)wcslen(text), &sz);

    SelectObject(hdc, hOldFont);
    ReleaseDC(hwndRef, hdc);

    // Tambahkan padding 8 pixel, minimal 60
    int w = sz.cx + 8;
    return (w < 60) ? 60 : w;
}

// Ambil lebar yang dibutuhkan oleh teks SAAT INI di label waktu
int CurrentTimeLabelWidth(HWND hLabel, HFONT hFont) {
    wchar_t buf[64] = {};
    GetWindowTextW(hLabel, buf, 64);
    return MeasureStringWidth(hLabel, hFont, buf);
}

// Interpolasi warna utk gradasi volume hijau -> kuning -> merah
COLORREF LerpColor(COLORREF a, COLORREF b, double t) {
    if (t < 0)
        t = 0;
    if (t > 1)
        t = 1;
    return RGB((int)(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t),
               (int)(GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t),
               (int)(GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t));
}
double GetDpiScale(HWND hwnd) {
    UINT dpi = GetDpiForWindow(hwnd); // physical DPI monitor tempat window berada
    return (double)dpi / 96.0;        // 96 = baseline 100%
}

void VideoPlayerGUI::LayoutControls(int width, int height) {
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

    // --- Kumpulkan tombol beserta status valid (ikon ada) ---
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

    // Sembunyikan tombol yang tidak valid, tampilkan yang valid
    for (int i = 0; i < btnCount; ++i) {
        if (allBtns[i].h) {
            ShowWindow(allBtns[i].h, allBtns[i].valid ? SW_SHOW : SW_HIDE);
        }
    }

    // Hitung total lebar tombol valid (tanpa spacing tambahan di akhir)
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

    // Tambahkan margin kiri (EDGE) dan sedikit jarak setelah tombol
    int leftMargin = EDGE;
    int rightAfterBtns = (int)(12 * dpi); // jarak ekstra sebelum time label

    // --- Elemen kanan: Time Label + Volume (sejajar vertikal) ---
    // Ukur lebar label waktu saat ini
    // --- Elemen kanan: Time Label + Volume (sejajar vertikal) ---
    int timeW = CurrentTimeLabelWidth(g_hTimeLabel, m_hTimeFont);
    if (timeW < (int)(80 * dpi))
        timeW = (int)(80 * dpi);
    int maxTimeW = (int)(200 * dpi);
    if (timeW > maxTimeW)
        timeW = maxTimeW;

    // [FIX] Lebar slider volume TETAP (bukan mengisi semua sisa ruang)
    int volW = (int)(120 * dpi);

    int available = width - leftMargin - leftTotal - rightAfterBtns;
    int totalRightW = timeW + VOL_GAP + VOL_ICON_W + VOL_GAP + volW + VOL_GAP + VOL_PERC_W;

    // Kalau window terlalu sempit untuk muat semua elemen kanan, baru dikompres
    if (totalRightW > available) {
        int shrink = totalRightW - available;

        // Kompres slider dulu (sampai minimum 30dpi)
        int minVolW = (int)(30 * dpi);
        int volShrink = (volW - minVolW < shrink) ? (volW - minVolW) : shrink;
        if (volShrink > 0) {
            volW -= volShrink;
            shrink -= volShrink;
        }

        // Kalau masih kurang, baru kompres time label
        if (shrink > 0) {
            int minTimeW = (int)(40 * dpi);
            int timeShrink = (timeW - minTimeW < shrink) ? (timeW - minTimeW) : shrink;
            if (timeShrink > 0)
                timeW -= timeShrink;
        }

        totalRightW = timeW + VOL_GAP + VOL_ICON_W + VOL_GAP + volW + VOL_GAP + VOL_PERC_W;
    }

    // Posisi elemen kanan — right-aligned sebagai satu blok
    int timeX = width - EDGE - totalRightW;
    int volX = timeX + timeW + VOL_GAP;
    int volY = btnRowY + (BTN_SIZE - CTRL_H) / 2;
    int timeY = volY;

    // --- Terapkan posisi ---

    // 1. Video Area
    if (g_hVideoArea) {
        SetWindowPos(g_hVideoArea, HWND_BOTTOM, 0, 0, width, videoHeight, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    // 2. Progress bar
    if (g_hProgress) {
        SetWindowPos(g_hProgress, HWND_TOP, EDGE, progressY, width - EDGE * 2, PROGRESS_H,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    // 3. Tombol (kiri) – layout hanya yang valid secara berurutan
    int bx = leftMargin;
    for (int i = 0; i < btnCount; ++i) {
        if (allBtns[i].valid && allBtns[i].h) {
            SetWindowPos(allBtns[i].h, HWND_TOP, bx, btnRowY, BTN_SIZE, BTN_SIZE, SWP_NOACTIVATE | SWP_SHOWWINDOW);
            bx += BTN_SIZE + BTN_SPACING;
        }
    }

    // 4. Time label
    if (g_hTimeLabel) {
        SetWindowPos(g_hTimeLabel, HWND_TOP, timeX, timeY, timeW, CTRL_H, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    // 5. Volume icon
    if (g_hVolIcon) {
        SetWindowPos(g_hVolIcon, HWND_TOP, volX, volY, VOL_ICON_W, CTRL_H, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    // 6. Volume slider
    if (g_hVolume) {
        SetWindowPos(g_hVolume, HWND_TOP, volX + VOL_ICON_W + VOL_GAP, volY, volW, CTRL_H,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    // 7. Volume percent
    if (g_hVolPercent) {
        SetWindowPos(g_hVolPercent, HWND_TOP, volX + VOL_ICON_W + VOL_GAP + volW + VOL_GAP, volY, VOL_PERC_W, CTRL_H,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    m_player.UpdateVideoSize();
    InvalidateRect(g_hProgress, nullptr, TRUE);

    // Subtitle overlay di-update via UpdateSubtitleDisplay()
}
// ==========================================
// WINDOW PROC
// ==========================================
LRESULT CALLBACK VideoPlayerGUI::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    VideoPlayerGUI* self = reinterpret_cast<VideoPlayerGUI*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (uMsg) {
    case WM_CREATE: {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        VideoPlayerGUI* pThis = reinterpret_cast<VideoPlayerGUI*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);

        SetWindowTheme(hwnd, L"Explorer", nullptr);
        pThis->CreateMenuBar(hwnd);
        pThis->CreateControls(hwnd);

        if (pThis->g_hTimeLabel) {
            SetWindowTextW(pThis->g_hTimeLabel, L"--:-- / --:--");
        }

        // [PENTING] Panggil LayoutControls setelah create
        RECT rc;
        GetClientRect(hwnd, &rc);
        pThis->LayoutControls(rc.right, rc.bottom);

        return 0;
    }

    case WM_ERASEBKGND: {
        if (!self)
            return 0;
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        // [FIX FULLSCREEN] Di fullscreen, bg hitam agar edge/border tidak bocor putih.
        COLORREF bg = self->m_isFullscreen ? RGB(0, 0, 0) : self->COLOR_MODERN_BG;
        HBRUSH hBrush = CreateSolidBrush(bg);
        FillRect(hdc, &rc, hBrush);
        DeleteObject(hBrush);
        return 1;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;

        // Tooltip waktu seekbar: bg gelap + teks putih
        if ((HWND)lParam == self->m_hTimeTip) {
            SetBkColor(hdc, self->COLOR_TIP_BG);
            SetTextColor(hdc, RGB(255, 255, 255));
            static HBRUSH hTipBrush = CreateSolidBrush(self->COLOR_TIP_BG);
            return (INT_PTR)hTipBrush;
        }

        // [FIX FULLSCREEN] Label waktu di atas video pakai bg gelap
        if ((HWND)lParam == self->g_hTimeLabel && self->m_isFullscreen) {
            SetBkColor(hdc, self->COLOR_TIP_BG);
            SetTextColor(hdc, RGB(255, 255, 255));
            static HBRUSH hTimeFsBrush = CreateSolidBrush(self->COLOR_TIP_BG);
            return (INT_PTR)hTimeFsBrush;
        }

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, self->COLOR_MODERN_TEXT);
        static HBRUSH hBrush = CreateSolidBrush(self->COLOR_MODERN_BG);
        return (INT_PTR)hBrush;
    }

    // [VLC-STYLE] Custom draw seekbar + volume bar
    case WM_NOTIFY: {
        LPNMHDR pnmh = (LPNMHDR)lParam;
        if (!self)
            break;
        if (pnmh->code != NM_CUSTOMDRAW)
            break;

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
                    return CDRF_SKIPDEFAULT;
            }
            return CDRF_DODEFAULT;
        }
        if (pnmh->hwndFrom == self->g_hVolume) {
            LPNMCUSTOMDRAW pcd = (LPNMCUSTOMDRAW)lParam;
            if (pcd->dwDrawStage == CDDS_PREPAINT) {
                // [FIX] Jangan fill background di sini lagi — sudah di-fill
                // oleh VolumeSubclassProc::WM_PAINT (double buffer). Fill kedua
                // di titik ini menimpa hasil gambar & menyebabkan glitch/kedip.
                return CDRF_NOTIFYITEMDRAW;
            }
            if (pcd->dwDrawStage == CDDS_ITEMPREPAINT) {
                if (pcd->dwItemSpec == TBCD_CHANNEL) {
                    self->DrawVlcVolumeBar(pcd->hdc);
                    return CDRF_SKIPDEFAULT;
                }
                if (pcd->dwItemSpec == TBCD_THUMB)
                    return CDRF_SKIPDEFAULT;
            }
            return CDRF_DODEFAULT;
        }
        break;
    }
    case WM_GETMINMAXINFO: {
        MINMAXINFO* pmmi = reinterpret_cast<MINMAXINFO*>(lParam);
        double dpi = self ? GetDpiScale(hwnd) : 1.0;
        pmmi->ptMinTrackSize.x = (int)(500 * dpi);
        pmmi->ptMinTrackSize.y = (int)(320 * dpi);
        return 0;
    }
    case WM_DPICHANGED: {
        if (self) {
            RECT rcTarget;
            if (self->m_isFullscreen) {
                // [FIX FULLSCREEN] Rect saran sistem berukuran windowed;
                // saat fullscreen tetap isi penuh monitor tujuan.
                MONITORINFO mi = {sizeof(mi)};
                HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                if (!GetMonitorInfo(mon, &mi))
                    break;
                rcTarget = mi.rcMonitor;
            } else {
                RECT* prc = (RECT*)lParam;
                rcTarget = *prc;
            }
            SetWindowPos(hwnd, nullptr, rcTarget.left, rcTarget.top, rcTarget.right - rcTarget.left,
                         rcTarget.bottom - rcTarget.top, SWP_NOZORDER | SWP_NOACTIVATE);

            // Font & layout perlu dihitung ulang di DPI baru
            if (self->m_hModernFont)
                DeleteObject(self->m_hModernFont);
            if (self->m_hTimeFont)
                DeleteObject(self->m_hTimeFont);
            if (self->m_hTipFont)
                DeleteObject(self->m_hTipFont);

            double dpi = GetDpiScale(hwnd);
            self->m_hModernFont =
                CreateFontW((int)(-14 * dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
            self->m_hTimeFont =
                CreateFontW((int)(-15 * dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
            self->m_hTipFont =
                CreateFontW((int)(-12 * dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");

            SendMessage(self->g_hTimeLabel, WM_SETFONT, (WPARAM)self->m_hTimeFont, TRUE);
            HWND hc = GetWindow(hwnd, GW_CHILD);
            while (hc) {
                if (hc != self->g_hTimeLabel)
                    SendMessage(hc, WM_SETFONT, (WPARAM)self->m_hModernFont, TRUE);
                hc = GetNextWindow(hc, GW_HWNDNEXT);
            }

            RECT rcC;
            GetClientRect(hwnd, &rcC);
            self->LayoutControls(rcC.right, rcC.bottom);
        }
        return 0;
    }
    case WM_SIZE:
        if (!self)
            break;
        if (wParam == SIZE_MINIMIZED) {
            self->m_wasMinimized = true;
            return 0;
        }
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
        if (self) {
            self->m_player.UpdateVideoSize();
            self->RecoverVideo();
        }
        return 0;
    case WM_MOUSEMOVE:
        if (self && self->m_isFullscreen)
            self->PokeOSControls();
        return 0;
    case WM_COMMAND:
        if (self)
            self->OnCommand(wParam, lParam);
        return 0;
    case WM_HSCROLL:
        if (self)
            self->OnHScroll(wParam, lParam);
        return 0;
    case WM_TIMER:
        if (!self)
            break;
        if (wParam == ID_TIMER_UPDATE) {
            self->OnTimerTick();
        } else if (wParam == ID_TIMER_OSI_HIDE) {
            // Auto-hide overlay + cursor saat idle di fullscreen.
            // [FIX GLITCH] Jangan sembunyikan selama kursor masih berada
            // di atas kontrol -- cukup reset timer (dihitung ulang 2.5 dtk lagi).
            if (self->m_isFullscreen && !self->m_isDraggingProgress) {
                if (self->CursorOverControls()) {
                    SetTimer(hwnd, ID_TIMER_OSI_HIDE, 2500, nullptr);
                } else {
                    self->ShowOSControls(false);
                    if (!self->m_cursorHidden) {
                        ShowCursor(FALSE);
                        self->m_cursorHidden = true;
                    }
                }
            }
        }
        return 0;
    case WM_APP_MEDIA_READY:
        if (self)
            self->OnMediaReady(); // durasi trackbar + fit window ke video
        return 0;

    case WM_ACTIVATEAPP:
        // Kehilangan fokus: tetap play (gaya VLC). Fokus kembali: pulihkan video.
        if (self && !wParam) {
            // Sembunyikan semua subtitle overlay saat focus hilang
            self->HideAllSubOverlays();
        } else if (self && wParam) {
            // Focus kembali: izinkan overlay ditampilkan lagi
            self->m_subsHidden = false;
        }
        if (self && self->m_isFullscreen) {
            if (!wParam) {
                // Focus hilang: turunkan dari topmost supaya app lain muncul di depan
                SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
            } else {
                // Focus kembali: kembalikan topmost, fullscreen tetap utuh
                MONITORINFO mi = {sizeof(mi)};
                HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                if (GetMonitorInfo(mon, &mi)) {
                    SetWindowPos(hwnd, HWND_TOPMOST, mi.rcMonitor.left, mi.rcMonitor.top,
                                 mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
                                 SWP_NOACTIVATE | SWP_NOOWNERZORDER);
                }
                self->RecoverVideo();
            }
        } else if (wParam && self) {
            self->RecoverVideo();
        }
        return 0;

    case WM_WTSSESSION_CHANGE:
        if (self && (wParam == WTS_SESSION_UNLOCK || wParam == WTS_REMOTE_CONNECT || wParam == WTS_CONSOLE_CONNECT))
            self->RecoverVideo();
        return 0;

    case WM_DISPLAYCHANGE:
        if (self) {
            self->m_player.UpdateVideoSize();
            self->RecoverVideo();
        }
        return 0;

    case WM_QUERYENDSESSION:
        return TRUE;

    case WM_ENDSESSION:
        if (self && wParam) { // shutdown/logoff: stop rapi
            self->m_player.Stop();
            self->SetPlayPauseUI(false);
        }
        return 0;

    case WM_APP_GRAPH_EVENT:
        if (self)
            self->m_player.HandleGraphEvent(); // EC_COMPLETE dll.
        return 0;

    case WM_APP_PLAYBACK_ENDED:
        if (self) {
            self->SetPlayPauseUI(false);
            self->SetProgressPos(self->m_progressRangeMax);
        }
        return 0;
    case WM_APP_AUDIO_MISSING:
        MessageBox(hwnd,
                   L"File ini punya track audio tapi codec-nya tidak ditemukan.\nVideo tetap diputar tanpa suara.",
                   L"Vidi Player", MB_OK | MB_ICONWARNING);
        break;
    case WM_APP_MEDIA_ERROR:
        if (self) {
            MessageBox(self->g_hMainWnd, L"Gagal memutar file (graph error).", L"Vidi", MB_OK | MB_ICONERROR);
            self->SetPlayPauseUI(false);
            self->SetProgressPos(0);
        }
        return 0;

    case WM_DESTROY:
        if (self->m_hModernFont)
            DeleteObject(self->m_hModernFont);
        if (self->m_hTimeFont)
            DeleteObject(self->m_hTimeFont);
        if (self->m_hTipFont)
            DeleteObject(self->m_hTipFont);
        if (self->m_hSubFont)
            DeleteObject(self->m_hSubFont);
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
        return 1; // [FIX] Custom draw handle semua painting

    // [FIX FULLSCREEN] Double-buffered paint: isi bg SEKALI, lalu
    // Windows paint normal di atasnya. Mengatasi glitch TBCD_TICS
    // yang overwrite background gelap.
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

        // 1) Fill background (gelap di fullscreen, putih di windowed)
        COLORREF bg = self->m_isFullscreen ? self->COLOR_TIP_BG : self->COLOR_MODERN_BG;
        HBRUSH hBgBrush = CreateSolidBrush(bg);
        FillRect(dcMem, &rc, hBgBrush);
        DeleteObject(hBgBrush);

        // 2) Windows paint trackbar normal (track, tick marks, dll)
        //    di atas background kita
        SendMessage(hwnd, WM_PRINTCLIENT, (WPARAM)dcMem, PRF_CLIENT);

        // 3) Blit hasil ke layar (flicker-free)
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
            self->SeekFromTrackbarClick((short)LOWORD(lParam));
        }
        return 0;
    case WM_MOUSEMOVE: {
        if (!self)
            break;
        int x = (short)LOWORD(lParam);
        if (GetCapture() == hwnd && self->m_isDraggingProgress) {
            self->DragSeekTo(x);
        } else {
            TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hwnd, 0};
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
        if (self)
            self->EndSeekDrag();
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

    int pos = (int)SendMessage(g_hProgress, TBM_GETPOS, 0, 0);
    int max = (int)SendMessage(g_hProgress, TBM_GETRANGEMAX, 0, 0);

    const int BAR_H = 5;
    const int THUMB_SIZE = (m_seekHot || m_isDraggingProgress) ? 14 : 12;
    int cy = (rc.top + rc.bottom) / 2;
    RECT track = {rc.left + 2, cy - BAR_H / 2, rc.right - 3, cy + BAR_H / 2};
    int width = track.right - track.left;

    // [FIX GDI] Simpan original objects, hapus semua di akhir
    HPEN hNullPen = CreatePen(PS_NULL, 0, 0);
    HBRUSH hTrack = CreateSolidBrush(COLOR_SEEK_TRACK);
    HGDIOBJ hOldPen = SelectObject(hdc, hNullPen);
    HGDIOBJ hOldBr = SelectObject(hdc, hTrack);

    RoundRect(hdc, track.left, track.top, track.right, track.bottom, BAR_H, BAR_H);

    bool hot = m_seekHot || m_isDraggingProgress;
    int fx = track.left + (max > 0 ? (int)(((double)pos / max) * width) : 0);
    if (fx > track.left + BAR_H) {
        HBRUSH hFill = CreateSolidBrush(hot ? COLOR_SEEK_FILL_HOT : COLOR_SEEK_FILL);
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

    // [FIX GDI] Restore semua original objects, baru delete yang kita buat
    SelectObject(hdc, hOldBr);
    SelectObject(hdc, hOldPen);
    DeleteObject(hTrack);
    DeleteObject(hNullPen);
}

// ==========================================
// [VOL 0-150] posisi slider -> volume native + DSP gain
// ==========================================
void VideoPlayerGUI::ApplyVolumeFromSlider(int pos) {
    if (pos < 0)
        pos = 0;
    if (pos > VOL_MAX)
        pos = VOL_MAX;
    float v = pos / 100.0f;               // 0 .. 1.5
    float native = (v > 1.0f) ? 1.0f : v; // <=100% via IBasicAudio
    float boost = (v > 1.0f) ? v : 1.0f;  // >100% via DSP
    m_player.SetVolume(native);
    m_player.SetDspGain(boost);
    if (pos > 0 && m_isMuted)
        m_isMuted = false; // geser manual -> mute lepas
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

LRESULT CALLBACK VideoPlayerGUI::VolumeSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                                    UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    VideoPlayerGUI* self = reinterpret_cast<VideoPlayerGUI*>(dwRefData);
    switch (uMsg) {
    case WM_ERASEBKGND:
        return 1;

    // [FIX FULLSCREEN] Double-buffered paint — sama seperti ProgressSubclassProc
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

        COLORREF bg = self->m_isFullscreen ? self->COLOR_TIP_BG : self->COLOR_MODERN_BG;
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
            int pos = (rc.right > 0) ? (int)(((double)x / rc.right) * self->VOL_MAX) : 0;
            if (pos < 0)
                pos = 0;
            if (pos > self->VOL_MAX)
                pos = self->VOL_MAX;
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
            int pos = (rc.right > 0) ? (int)(((double)x / rc.right) * self->VOL_MAX) : 0;
            if (pos < 0)
                pos = 0;
            if (pos > self->VOL_MAX)
                pos = self->VOL_MAX;
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
// VOLUME BAR — gradasi hijau(0%) -> kuning -> merah(150%), notch di 100%
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

    // [FIX GDI] Simpan original objects
    HPEN hNullPen = CreatePen(PS_NULL, 0, 0);
    HBRUSH hTrack = CreateSolidBrush(COLOR_SEEK_TRACK);
    HGDIOBJ hOldPen = SelectObject(hdc, hNullPen);
    HGDIOBJ hOldBr = SelectObject(hdc, hTrack);

    RoundRect(hdc, track.left, track.top, track.right, track.bottom, BAR_H, BAR_H);

    double ratio = (double)pos / VOL_MAX;
    if (ratio < 0)
        ratio = 0;
    if (ratio > 1)
        ratio = 1;
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

    // [FIX GDI] Restore + delete semua
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
    // untuk mencegah seek ke 0 apabila durasinya belum diketahui
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
    if (now - m_lastSeekTick > 120) { // throttled seek = smooth scrubbing
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
// MEDIA READY — set range trackbar sesuai durasi asli video
// ==========================================
void VideoPlayerGUI::OnMediaReady() {
    m_cachedDuration = m_player.GetDuration();
    double dur = m_cachedDuration;
    int range = static_cast<int>(dur * 10.0);
    if (range < 100)
        range = 100;

    m_progressRangeMax = range;
    SendMessage(g_hProgress, TBM_SETRANGEMIN, TRUE, 0);
    SendMessage(g_hProgress, TBM_SETRANGEMAX, TRUE, m_progressRangeMax);
    SetProgressPos(0);
    UpdateTimeLabel(0.0, dur);
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

    case IDC_BTN_FULLSCREEN:
    case IDM_VIEW_FULLSCREEN:
        if (m_isFullscreen)
            ExitFullscreen();
        else
            EnterFullscreen();
        break;

    case IDC_BTN_PLAYLIST:
        break;

    case IDC_BTN_LOOP:
        m_isLooping = !m_isLooping;
        SetToggleBtnState(g_hLoopBtn, m_isLooping);
        break;

    case IDC_BTN_SHUFFLE:
        m_isShuffle = !m_isShuffle;
        SetToggleBtnState(g_hShuffleBtn, m_isShuffle);
        break;

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
        UpdateVolumePercent(vol);
        break;
    }
    case IDM_AUDIO_VOLDOWN: {
        int vol = (int)SendMessage(g_hVolume, TBM_GETPOS, 0, 0);
        vol = (vol - 10 < 0) ? 0 : vol - 10;
        SendMessage(g_hVolume, TBM_SETPOS, TRUE, vol);
        ApplyVolumeFromSlider(vol);
        UpdateVolumePercent(vol);
        break;
    }
    case IDM_AUDIO_MUTE:
        ToggleMute();
        break;

    case IDM_APP_ESCAPE:
        if (m_isFullscreen)
            ExitFullscreen();
        break;

    case IDM_HELP_ABOUT:
        MessageBox(g_hMainWnd, L"Vidi Video Player\nDibangun dengan Win32 + DirectShow", L"About Vidi Player",
                   MB_OK | MB_ICONINFORMATION);
        break;
    }
}

// ==========================================
// TOGGLE MUTE — bisukan / aktifkan suara
// ==========================================
void VideoPlayerGUI::ToggleMute() {
    if (!m_isMuted) {
        m_lastVolume = SendMessage(g_hVolume, TBM_GETPOS, 0, 0) / 100.0f; // 0..1.5
        m_player.SetVolume(0.0f);
        m_player.SetDspGain(1.0f); // matikan juga boost saat mute
        SendMessage(g_hVolume, TBM_SETPOS, TRUE, 0);
        InvalidateRect(g_hVolume, nullptr, FALSE);
        UpdateVolumePercent(0);
        m_isMuted = true;
    } else {
        int back = (int)(m_lastVolume * 100.0f + 0.5f);
        SendMessage(g_hVolume, TBM_SETPOS, TRUE, back);
        ApplyVolumeFromSlider(back);
        InvalidateRect(g_hVolume, nullptr, FALSE);
        m_isMuted = false;
    }
}

void VideoPlayerGUI::UpdateVolumePercent(int pos) {
    if (!g_hVolPercent)
        return;
    wchar_t buf[8];
    swprintf_s(buf, L"%d%%", pos);
    SetWindowTextW(g_hVolPercent, buf);
}

void VideoPlayerGUI::SetToggleBtnState(HWND btn, bool active) {
    if (!btn)
        return;
    LONG style = GetWindowLong(btn, GWL_STYLE);
    if (active)
        style |= WS_BORDER;
    else
        style &= ~WS_BORDER;
    SetWindowLong(btn, GWL_STYLE, style);
    InvalidateRect(btn, nullptr, FALSE);
}

void VideoPlayerGUI::EnterFullscreen() {
    if (m_isFullscreen)
        return;

    MONITORINFO mi = {sizeof(mi)};
    HMONITOR mon = MonitorFromWindow(g_hMainWnd, MONITOR_DEFAULTTONEAREST);
    if (!GetWindowPlacement(g_hMainWnd, &m_prevPlacement) || !GetMonitorInfo(mon, &mi))
        return;

    m_isFullscreen = true;
    SendMessage(g_hMainWnd, WM_SETREDRAW, FALSE, 0);

    // [FIX] Tambah WS_POPUP agar window bisa menutupi taskbar.
    // Tanpa WS_POPUP, window tidak bisa fullscreen penuh.
    DWORD style = GetWindowLong(g_hMainWnd, GWL_STYLE);
    SetWindowLong(g_hMainWnd, GWL_STYLE, (style & ~WS_OVERLAPPEDWINDOW) | WS_POPUP);
    if (m_hMenuBar)
        SetMenu(g_hMainWnd, nullptr);

    // [FIX] HWND_TOPMOST memastikan window di atas taskbar (taskbar juga topmost).
    SetWindowPos(g_hMainWnd, HWND_TOPMOST, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                 mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

    SendMessage(g_hMainWnd, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(g_hMainWnd, nullptr, nullptr,
                 RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);

    m_player.UpdateVideoSize();
    RecoverVideo();
    PokeOSControls();
}

void VideoPlayerGUI::ExitFullscreen() {
    if (!m_isFullscreen)
        return;

    m_isFullscreen = false;
    SendMessage(g_hMainWnd, WM_SETREDRAW, FALSE, 0);

    // [FIX] Kembalikan style lengkap + HWND_NOTOPMOST.
    DWORD style = GetWindowLong(g_hMainWnd, GWL_STYLE);
    SetWindowLong(g_hMainWnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
    if (m_hMenuBar)
        SetMenu(g_hMainWnd, m_hMenuBar);
    SetWindowPlacement(g_hMainWnd, &m_prevPlacement);

    // [FIX] HWND_NOTOPMOST + SWP_FRAMECHANGED memastikan window
    // kembali ke bawah taskbar dan frame dihitung ulang.
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

    m_player.UpdateVideoSize();
    RecoverVideo();
}

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

    // Ukuran native video; hanya mengecil bila tak muat 90% work area (tidak upscale)
    double scaleX = ((double)availW * 0.9) / vw;
    double scaleY = ((double)availH * 0.9) / vh;
    double scale = (scaleX < scaleY) ? scaleX : scaleY;
    if (scale > 1.0)
        scale = 1.0;
    int cw = (int)(vw * scale + 0.5);
    int ch = (int)(vh * scale + 0.5);

    // Delta chrome (menu bar + border) dari ukuran window saat ini → aman utk DPI
    RECT rcC, rcW;
    GetClientRect(g_hMainWnd, &rcC);
    GetWindowRect(g_hMainWnd, &rcW);
    int extraW = (rcW.right - rcW.left) - rcC.right;
    int extraH = (rcW.bottom - rcW.top) - rcC.bottom;

    int newX = mi.rcWork.left + (availW - (cw + extraW)) / 2;
    int newY = mi.rcWork.top + (availH - (ch + extraH)) / 2;
    SetWindowPos(g_hMainWnd, nullptr, newX, newY, cw + extraW, ch + extraH, SWP_NOZORDER);
}

void VideoPlayerGUI::LayoutFullscreen(int width, int height) {
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

    // Center 8 buttons
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

    // Subtitle overlay di-update via UpdateSubtitleDisplay()
}
void VideoPlayerGUI::ShowOSControls(bool visible) {
    int cmd = visible ? SW_SHOW : SW_HIDE;
    HWND ctrls[] = {g_hProgress,      g_hSkipBack,    g_hPlayBtn,  g_hStopBtn,    g_hSkipForward,
                    g_hFullscreenBtn, g_hPlaylistBtn, g_hLoopBtn,  g_hShuffleBtn, g_hVolIcon,
                    g_hVolume,        g_hVolPercent,  g_hTimeLabel};
    for (HWND h : ctrls)
        if (h)
            ShowWindow(h, cmd);
    // Subtitle overlay: always visible when there's text
}

void VideoPlayerGUI::PokeOSControls() {
    if (!m_isFullscreen || !g_hMainWnd)
        return;

    if (m_cursorHidden) {
        ShowCursor(TRUE);
        m_cursorHidden = false;
    }
    ShowOSControls(true);
    // One-shot: kalau 2.5 detik tak ada gerakan, WM_TIMER_OSI_HIDE menyembunyikan
    SetTimer(g_hMainWnd, ID_TIMER_OSI_HIDE, 2500, nullptr);
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
        m_player.Play(); // pastikan graph Running lagi
    } else if (m_cachedDuration > 0.0) {
        m_player.ForceFrameRefresh(); // paused: paksa render 1 frame
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
            if (m_isDraggingProgress)
                return; // drag mouse sudah ditangani subclass
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

// ==========================================
// HELPER — set posisi progress + paksa repaint penuh channel
// ==========================================
void VideoPlayerGUI::SetProgressPos(int pos) {
    int cur = (int)SendMessage(g_hProgress, TBM_GETPOS, 0, 0);
    if (cur == pos)
        return;

    SendMessage(g_hProgress, TBM_SETPOS, TRUE, pos);
    // [FIX OPTIMASI] FALSE: custom draw CDDS_PREPAINT handle bg, tidak perlu erase
    InvalidateRect(g_hProgress, nullptr, FALSE);
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
    if (m_isDraggingProgress)
        return;
    DWORD now = GetTickCount();
    if (now - m_lastDurCheckTick > 500) {
        m_lastDurCheckTick = now;
        double fresh = m_player.GetDuration();
        if (fresh > 0.0 && fabs(fresh - m_cachedDuration) > 0.5) {
            m_cachedDuration = fresh;
            int range = static_cast<int>(fresh * 10.0);
            if (range < 100)
                range = 100;
            m_progressRangeMax = range;
            SendMessage(g_hProgress, TBM_SETRANGEMIN, TRUE, 0);
            SendMessage(g_hProgress, TBM_SETRANGEMAX, TRUE, m_progressRangeMax);
        }
    }
    double dur = m_cachedDuration;
    if (dur <= 0.0)
        return;
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
    if (sliderPos < 0)
        sliderPos = 0;
    if (sliderPos > m_progressRangeMax)
        sliderPos = m_progressRangeMax;
    SetProgressPos(sliderPos);
    UpdateTimeLabel(pos, dur);

    // Update subtitle overlay
    UpdateSubtitleDisplays(pos);
}

void VideoPlayerGUI::UpdateTimeLabel(double posSeconds, double durSeconds) {
    wchar_t buf[64];
    int p = (int)posSeconds, d = (int)durSeconds;

    if (d <= 0) {
        swprintf_s(buf, L"--:-- / --:--");
    } else if (d >= 3600) {
        swprintf_s(buf, L"%d:%02d:%02d / %d:%02d:%02d", p / 3600, (p % 3600) / 60, p % 60, d / 3600, (d % 3600) / 60,
                   d % 60);
    } else {
        swprintf_s(buf, L"%02d:%02d / %02d:%02d", p / 60, p % 60, d / 60, d % 60);
    }

    // Pastikan tidak ada karakter aneh
    SetWindowTextW(g_hTimeLabel, buf);
    InvalidateRect(g_hTimeLabel, nullptr, FALSE);

    // Update lebar hanya jika perlu
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
    if (m_hIconPlay && m_hIconPause) {
        HICON icon = playing ? m_hIconPause : m_hIconPlay;
        SendMessage(g_hPlayBtn, BM_SETIMAGE, IMAGE_ICON, (LPARAM)icon);
    }
    // Cegah screensaver/layar mati selama video berjalan
    SetThreadExecutionState(playing ? (ES_CONTINUOUS | ES_DISPLAY_REQUIRED) : ES_CONTINUOUS);
    // [FIX OPTIMASI] Timer throttling: 33ms saat play, 500ms saat pause/stop
    KillTimer(g_hMainWnd, ID_TIMER_UPDATE);
    SetTimer(g_hMainWnd, ID_TIMER_UPDATE, playing ? 33 : 500, nullptr);
}

void VideoPlayerGUI::OpenFileDialog() {
    wchar_t filePath[MAX_PATH] = {0};
    OPENFILENAME ofn = {};

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hMainWnd;
    ofn.lpstrFilter = L"Video Files\0*.mp4;*.mkv;*.avi;*.mov;*.wmv;*.webm;*.m4v;*.ts;*.flv\0"
                      L"Audio Files\0*.mp3;*.aac;*.flac;*.wav;*.ogg\0"
                      L"All Files\0*.*\0\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

    if (!GetOpenFileName(&ofn))
        return;

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
        MessageBox(g_hMainWnd, L"Gagal membuka file. Format mungkin tidak didukung.", L"Vidi", MB_OK | MB_ICONERROR);
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

    // --- Video ---
    HMENU hVideo = CreatePopupMenu();
    AppendMenu(hVideo, MF_STRING, IDM_VIEW_FULLSCREEN, L"Fullscreen\tF");
    AppendMenu(hVideo, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hVideo, MF_STRING, IDM_TAKE_SNAPSHOT, L"Take Snapshot");
    AppendMenu(hVideo, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hVideo, MF_STRING, IDM_ALWAYS_FIT_WINDOW, L"Fit Window");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hVideo, L"Video");

    // --- Subtitle ---
    HMENU hSubtitle = CreatePopupMenu();
    AppendMenu(hSubtitle, MF_STRING, IDM_SUB_ADD_FILE, L"Add Subtitle File...");
    AppendMenu(hSubtitle, MF_STRING, IDM_SUB_TRACK, L"Subtitle Track");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hSubtitle, L"Subtitle");

    // --- Tools ---
    HMENU hTools = CreatePopupMenu();
    AppendMenu(hTools, MF_STRING, IDM_EFFECTS_FILTERS, L"Effects and Filters");
    AppendMenu(hTools, MF_STRING, IDM_CODEC_INFO, L"Codec Information");
    AppendMenu(hTools, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hTools, MF_STRING, IDM_PREFERENCES, L"Preferences");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hTools, L"Tools");

    // --- View ---
    HMENU hView = CreatePopupMenu();
    AppendMenu(hView, MF_STRING, IDM_PLAYLIST, L"Playlist");
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
    double dpi = GetDpiScale(hwnd);

    m_hModernFont =
        CreateFontW((int)(-14 * dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                    CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
    m_hTimeFont = CreateFontW((int)(-15 * dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
    m_hTipFont = CreateFontW((int)(-12 * dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");

    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring exeDir = exePath;
    exeDir = exeDir.substr(0, exeDir.find_last_of(L'\\') + 1);
    std::wstring assetsDir = exeDir + L"assets\\";

    int iconSize = (int)(24 * dpi);
    m_hIconPlay = (HICON)LoadImageW(nullptr, (assetsDir + L"play-button-arrowhead.ico").c_str(), IMAGE_ICON, iconSize,
                                    iconSize, LR_LOADFROMFILE);
    m_hIconPause =
        (HICON)LoadImageW(nullptr, (assetsDir + L"pause.ico").c_str(), IMAGE_ICON, iconSize, iconSize, LR_LOADFROMFILE);
    m_hIconStop = (HICON)LoadImageW(nullptr, (assetsDir + L"stop-button.ico").c_str(), IMAGE_ICON, iconSize, iconSize,
                                    LR_LOADFROMFILE);
    m_hIconSkipBack = (HICON)LoadImageW(nullptr, (assetsDir + L"left-arrow.ico").c_str(), IMAGE_ICON, iconSize,
                                        iconSize, LR_LOADFROMFILE);
    m_hIconSkipForward = (HICON)LoadImageW(nullptr, (assetsDir + L"fast-forward.ico").c_str(), IMAGE_ICON, iconSize,
                                           iconSize, LR_LOADFROMFILE);
    m_hIconSpeaker = (HICON)LoadImageW(nullptr, (assetsDir + L"speaker.ico").c_str(), IMAGE_ICON, iconSize, iconSize,
                                       LR_LOADFROMFILE);
    m_hIconFullscreen = (HICON)LoadImageW(nullptr, (assetsDir + L"fullscreen.ico").c_str(), IMAGE_ICON, iconSize,
                                          iconSize, LR_LOADFROMFILE);
    m_hIconPlaylist = (HICON)LoadImageW(nullptr, (assetsDir + L"playlist.ico").c_str(), IMAGE_ICON, iconSize, iconSize,
                                        LR_LOADFROMFILE);
    m_hIconLoop =
        (HICON)LoadImageW(nullptr, (assetsDir + L"loop.ico").c_str(), IMAGE_ICON, iconSize, iconSize, LR_LOADFROMFILE);
    m_hIconShuffle = (HICON)LoadImageW(nullptr, (assetsDir + L"shuffle.ico").c_str(), IMAGE_ICON, iconSize, iconSize,
                                       LR_LOADFROMFILE);

    DWORD btnStyle = WS_CHILD | BS_ICON | BS_FLAT;

    // Video Area — tanpa border
    g_hVideoArea = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_BLACKRECT, 0, 0, 100, 100, hwnd,
                                   nullptr, nullptr, nullptr);
    SetWindowSubclass(g_hVideoArea, VideoAreaSubclassProc, 2, (DWORD_PTR)this);

    // Buttons
    g_hSkipBack =
        CreateWindowW(L"BUTTON", L"", btnStyle, 0, 0, 44, 44, hwnd, (HMENU)IDC_BTN_SKIPBACK, nullptr, nullptr);
    if (m_hIconSkipBack)
        SendMessage(g_hSkipBack, BM_SETIMAGE, IMAGE_ICON, (LPARAM)m_hIconSkipBack);

    g_hPlayBtn = CreateWindowW(L"BUTTON", L"", btnStyle, 0, 0, 48, 48, hwnd, (HMENU)IDC_BTN_PLAY, nullptr, nullptr);
    if (m_hIconPlay)
        SendMessage(g_hPlayBtn, BM_SETIMAGE, IMAGE_ICON, (LPARAM)m_hIconPlay);

    g_hStopBtn = CreateWindowW(L"BUTTON", L"", btnStyle, 0, 0, 44, 44, hwnd, (HMENU)IDC_BTN_STOP, nullptr, nullptr);
    if (m_hIconStop)
        SendMessage(g_hStopBtn, BM_SETIMAGE, IMAGE_ICON, (LPARAM)m_hIconStop);

    g_hSkipForward =
        CreateWindowW(L"BUTTON", L"", btnStyle, 0, 0, 44, 44, hwnd, (HMENU)IDC_BTN_SKIPFORWARD, nullptr, nullptr);
    if (m_hIconSkipForward)
        SendMessage(g_hSkipForward, BM_SETIMAGE, IMAGE_ICON, (LPARAM)m_hIconSkipForward);

    g_hFullscreenBtn =
        CreateWindowW(L"BUTTON", L"", btnStyle, 0, 0, 44, 44, hwnd, (HMENU)IDC_BTN_FULLSCREEN, nullptr, nullptr);
    if (m_hIconFullscreen)
        SendMessage(g_hFullscreenBtn, BM_SETIMAGE, IMAGE_ICON, (LPARAM)m_hIconFullscreen);

    g_hPlaylistBtn =
        CreateWindowW(L"BUTTON", L"", btnStyle, 0, 0, 44, 44, hwnd, (HMENU)IDC_BTN_PLAYLIST, nullptr, nullptr);
    if (m_hIconPlaylist)
        SendMessage(g_hPlaylistBtn, BM_SETIMAGE, IMAGE_ICON, (LPARAM)m_hIconPlaylist);

    g_hLoopBtn = CreateWindowW(L"BUTTON", L"", btnStyle, 0, 0, 44, 44, hwnd, (HMENU)IDC_BTN_LOOP, nullptr, nullptr);
    if (m_hIconLoop)
        SendMessage(g_hLoopBtn, BM_SETIMAGE, IMAGE_ICON, (LPARAM)m_hIconLoop);

    g_hShuffleBtn =
        CreateWindowW(L"BUTTON", L"", btnStyle, 0, 0, 44, 44, hwnd, (HMENU)IDC_BTN_SHUFFLE, nullptr, nullptr);
    if (m_hIconShuffle)
        SendMessage(g_hShuffleBtn, BM_SETIMAGE, IMAGE_ICON, (LPARAM)m_hIconShuffle);

    // Progress Bar
    g_hProgress = CreateWindowExW(0, TRACKBAR_CLASS, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS, 0, 0, 100, 24,
                                  hwnd, (HMENU)IDC_PROGRESS, nullptr, nullptr);
    SetWindowTheme(g_hProgress, L" ", L" ");
    SendMessage(g_hProgress, TBM_SETRANGEMIN, TRUE, 0);
    SendMessage(g_hProgress, TBM_SETRANGEMAX, TRUE, m_progressRangeMax);
    SendMessage(g_hProgress, TBM_SETTHUMBLENGTH, 12, 0);
    SetWindowSubclass(g_hProgress, ProgressSubclassProc, 1, (DWORD_PTR)this);

    // Time tooltip
    m_hTimeTip = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"STATIC", L"",
                                 WS_POPUP | SS_CENTER | SS_CENTERIMAGE, 0, 0, 80, 22, hwnd, nullptr, nullptr, nullptr);
    SetWindowLongPtr(m_hTimeTip, -8, (LONG_PTR)hwnd);
    SendMessage(m_hTimeTip, WM_SETFONT, (WPARAM)m_hTipFont, TRUE);

    // Volume Icon
    g_hVolIcon = CreateWindowW(L"STATIC", L"", WS_CHILD | SS_CENTER | SS_CENTERIMAGE, 0, 0, 20, 24, hwnd,
                               (HMENU)IDC_VOL_ICON, nullptr, nullptr);
    if (m_hIconSpeaker)
        SendMessage(g_hVolIcon, STM_SETIMAGE, IMAGE_ICON, (LPARAM)m_hIconSpeaker);

    // Volume Slider
    g_hVolume = CreateWindowExW(0, TRACKBAR_CLASS, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS, 0, 0, 100, 24,
                                hwnd, (HMENU)IDC_VOLUME, nullptr, nullptr);
    SetWindowTheme(g_hVolume, L" ", L" ");
    SendMessage(g_hVolume, TBM_SETRANGEMIN, TRUE, 0);
    SendMessage(g_hVolume, TBM_SETRANGEMAX, TRUE, VOL_MAX);
    SendMessage(g_hVolume, TBM_SETPOS, TRUE, 100);
    SetWindowSubclass(g_hVolume, VolumeSubclassProc, 3, (DWORD_PTR)this);

    // Volume Percent Label
    g_hVolPercent = CreateWindowW(L"STATIC", L"100%", WS_CHILD | SS_LEFT | SS_CENTERIMAGE, 0, 0, 40, 24, hwnd,
                                  (HMENU)IDC_VOL_PERCENT, nullptr, nullptr);

    // Time Label
    g_hTimeLabel = CreateWindowW(L"STATIC", L"--:-- / --:--",
                                 WS_CHILD | SS_LEFT | SS_CENTERIMAGE, // <-- pakai SS_LEFT
                                 0, 0, 150, 30, hwnd, (HMENU)IDC_TIME_LABEL, nullptr, nullptr);
    SendMessage(g_hTimeLabel, WM_SETFONT, (WPARAM)m_hTimeFont, TRUE);

    // Apply Fonts
    HWND hCtrl = GetWindow(hwnd, GW_CHILD);
    while (hCtrl) {
        if (hCtrl == g_hTimeLabel)
            SendMessage(hCtrl, WM_SETFONT, (WPARAM)m_hTimeFont, TRUE);
        else
            SendMessage(hCtrl, WM_SETFONT, (WPARAM)m_hModernFont, TRUE);
        hCtrl = GetNextWindow(hCtrl, GW_HWNDNEXT);
    }

    m_player.Initialize(g_hVideoArea, hwnd);
    CreateSubtitleOverlay(hwnd);
    SetTimer(hwnd, ID_TIMER_UPDATE, TIMER_INTERVAL_MS, nullptr);
}

// ==========================================
// SUBTITLE OVERLAY
// ==========================================
void VideoPlayerGUI::CreateSubtitleOverlay(HWND hwnd) {
    double dpi = GetDpiScale(hwnd);
    m_hSubFont = CreateFontW((int)(-22 * dpi), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");

    static bool s_registered = false;
    if (!s_registered) {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"VidiSubOverlay";
        RegisterClassW(&wc);
        s_registered = true;
    }

    for (int i = 0; i < MAX_SUB_OVERLAYS; ++i) {
        m_hSubOverlay[i] =
            CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE, L"VidiSubOverlay",
                            L"", WS_POPUP, 0, 0, 100, 40, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (m_hSubOverlay[i]) {
            SetLayeredWindowAttributes(m_hSubOverlay[i], 0, 255, LWA_ALPHA);
            SendMessage(m_hSubOverlay[i], WM_SETFONT, (WPARAM)m_hSubFont, TRUE);
        }
    }
}

void VideoPlayerGUI::HideAllSubOverlays() {
    m_subsHidden = true;
    for (int i = 0; i < MAX_SUB_OVERLAYS; ++i) {
        if (m_hSubOverlay[i])
            ShowWindow(m_hSubOverlay[i], SW_HIDE);
    }
}

void VideoPlayerGUI::UpdateSubtitleDisplays(double posSeconds) {
    if (m_subsHidden)
        return;
    auto entries = m_player.GetActiveSubtitles(posSeconds);

    RECT videoRC = {};
    if (g_hVideoArea)
        GetClientRect(g_hVideoArea, &videoRC);
    POINT tl = {videoRC.left, videoRC.top};
    POINT br = {videoRC.right, videoRC.bottom};
    if (g_hVideoArea) {
        ClientToScreen(g_hVideoArea, &tl);
        ClientToScreen(g_hVideoArea, &br);
    }
    int vidX = tl.x, vidY = tl.y;
    int vidW = br.x - tl.x, vidH = br.y - tl.y;

    double playResX = m_player.GetPlayResX();
    double playResY = m_player.GetPlayResY();
    if (playResX <= 0)
        playResX = 1280;
    if (playResY <= 0)
        playResY = 720;

    double dpi = GetDpiScale(g_hMainWnd);
    std::vector<int> withPos;
    std::vector<int> withoutPos;

    for (int i = 0; i < (int)entries.size(); ++i) {
        if (entries[i].text.empty())
            continue;
        if (entries[i].posX >= 0 && entries[i].posY >= 0)
            withPos.push_back(i);
        else
            withoutPos.push_back(i);
    }
    int used = 0;
    // Render subtitles with explicit \pos() — each keeps its own position
    for (int idx : withPos) {
        if (used >= MAX_SUB_OVERLAYS)
            break;
        const auto& e = entries[idx];

        HDC hdc = GetDC(m_hSubOverlay[used]);
        HGDIOBJ oldFont = SelectObject(hdc, m_hSubFont);

        const wchar_t* text = e.text.c_str();
        RECT rcCalc = {0, 0, 800, 200};
        DrawTextW(hdc, text, -1, &rcCalc, DT_CALCRECT | DT_CENTER | DT_WORDBREAK | DT_EDITCONTROL);

        int textW = rcCalc.right + (int)(40 * dpi);
        int textH = rcCalc.bottom + (int)(10 * dpi);

        int posX = vidX + (int)((e.posX / playResX) * vidW) - textW / 2;
        int posY = vidY + (int)((e.posY / playResY) * vidH) - textH / 2;

        if (posX < vidX)
            posX = vidX;
        if (posY < vidY)
            posY = vidY;
        if (posX + textW > vidX + vidW)
            posX = vidX + vidW - textW;
        if (posY + textH > vidY + vidH)
            posY = vidY + vidH - textH;

        SetWindowPos(m_hSubOverlay[used], HWND_TOPMOST, posX, posY, textW, textH, SWP_NOACTIVATE | SWP_SHOWWINDOW);

        RECT rcPaint = {0, 0, textW, textH};
        HBRUSH hbrBg = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(hdc, &rcPaint, hbrBg);
        DeleteObject(hbrBg);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        DrawTextW(hdc, text, -1, &rcPaint, DT_CENTER | DT_WORDBREAK | DT_EDITCONTROL);

        SelectObject(hdc, oldFont);
        ReleaseDC(m_hSubOverlay[used], hdc);
        used++;
    }

    // Render subtitles without \pos() — group by \an alignment, stack per group
    // \an: 1=BL 2=BC 3=BR 4=ML 5=MC 6=MR 7=TL 8=TC 9=TR
    // Track stack offset per alignment group
    int stackByAlignment[10] = {}; // index 0 unused, 1-9 for \an values

    for (int idx : withoutPos) {
        if (used >= MAX_SUB_OVERLAYS)
            break;
        const auto& e = entries[idx];

        HDC hdc = GetDC(m_hSubOverlay[used]);
        HGDIOBJ oldFont = SelectObject(hdc, m_hSubFont);

        const wchar_t* text = e.text.c_str();
        RECT rcCalc = {0, 0, 800, 200};
        DrawTextW(hdc, text, -1, &rcCalc, DT_CALCRECT | DT_CENTER | DT_WORDBREAK | DT_EDITCONTROL);

        int textW = rcCalc.right + (int)(40 * dpi);
        int textH = rcCalc.bottom + (int)(10 * dpi);

        int maxW = vidW - (int)(60 * dpi);
        if (textW > maxW)
            textW = maxW;

        int an = e.alignment;
        if (an < 1 || an > 9)
            an = 2; // default: bottom-center

        int margin = (int)(30 * dpi);
        int posX, posY;

        // Horizontal position based on alignment column
        if (an == 1 || an == 4 || an == 7)
            posX = vidX + margin; // left
        else if (an == 3 || an == 6 || an == 9)
            posX = vidX + vidW - textW - margin; // right
        else
            posX = vidX + (vidW - textW) / 2; // center

        // Vertical position based on alignment row + stack offset
        int offset = stackByAlignment[an];
        if (an >= 7) {
            // Top row: stack downward from top
            posY = vidY + margin + offset;
            stackByAlignment[an] += textH + (int)(4 * dpi);
        } else if (an >= 4) {
            // Middle row: stack downward from middle
            posY = vidY + vidH / 2 - textH / 2 + offset;
            stackByAlignment[an] += textH + (int)(4 * dpi);
        } else {
            // Bottom row (1,2,3): stack upward from bottom
            posY = vidY + vidH - textH - margin - offset;
            stackByAlignment[an] += textH + (int)(4 * dpi);
        }

        // Clamp to video area
        if (posX < vidX) posX = vidX;
        if (posY < vidY) posY = vidY;
        if (posX + textW > vidX + vidW) posX = vidX + vidW - textW;
        if (posY + textH > vidY + vidH) posY = vidY + vidH - textH;

        SetWindowPos(m_hSubOverlay[used], HWND_TOPMOST, posX, posY, textW, textH, SWP_NOACTIVATE | SWP_SHOWWINDOW);

        RECT rcPaint = {0, 0, textW, textH};
        HBRUSH hbrBg = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(hdc, &rcPaint, hbrBg);
        DeleteObject(hbrBg);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        DrawTextW(hdc, text, -1, &rcPaint, DT_CENTER | DT_WORDBREAK | DT_EDITCONTROL);

        SelectObject(hdc, oldFont);
        ReleaseDC(m_hSubOverlay[used], hdc);
        used++;
    }

    // Hide unused overlays
    for (int i = used; i < MAX_SUB_OVERLAYS; ++i) {
        ShowWindow(m_hSubOverlay[i], SW_HIDE);
    }
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

    if (!RegisterClass(&wc))
        return false;

    g_hMainWnd = CreateWindowEx(0, CLASS_NAME, L"Vidi Player", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                                CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, NULL, NULL, hInstance, this);

    if (!g_hMainWnd)
        return false;

    m_hAccel = CreatePlayerAccelTable();

    // Terima notifikasi lock/unlock/switch sesi (Win+L, RDP, fast user switching)
    WTSRegisterSessionNotification(g_hMainWnd, NOTIFY_FOR_THIS_SESSION);

    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);
    return true;
}

HACCEL VideoPlayerGUI::CreatePlayerAccelTable() {
    ACCEL acc[] = {
        {FVIRTKEY | FCONTROL | FNOINVERT, 'O', IDM_FILE_OPEN}, // Ctrl+O
        {FVIRTKEY | FNOINVERT, VK_SPACE, IDM_PLAYBACK_PLAY},   // play/pause
        {FVIRTKEY | FNOINVERT, 'S', IDM_PLAYBACK_STOP},         {FVIRTKEY | FNOINVERT, VK_LEFT, IDM_PLAYBACK_SKIPBACK},
        {FVIRTKEY | FNOINVERT, VK_RIGHT, IDM_PLAYBACK_SKIPFWD}, {FVIRTKEY | FNOINVERT, VK_UP, IDM_AUDIO_VOLUP},
        {FVIRTKEY | FNOINVERT, VK_DOWN, IDM_AUDIO_VOLDOWN},     {FVIRTKEY | FNOINVERT, 'M', IDM_AUDIO_MUTE},
        {FVIRTKEY | FNOINVERT, 'F', IDM_VIEW_FULLSCREEN},       {FVIRTKEY | FNOINVERT, VK_ESCAPE, IDM_APP_ESCAPE},
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
#include "../../include/gui/gui.hh"
#include "../../include/gui/constants.hh"
#include "../../include/kernels/ids.hh"

#include <wtsapi32.h>
#include <dwmapi.h>

namespace guiVidi {

// ============================================================
// Pre-created GDI brushes
// ============================================================

static const HBRUSH hBrushTip = CreateSolidBrush(COLOR_TIP_BG);

static const HBRUSH hBrushTimeFs = CreateSolidBrush(COLOR_TIP_BG);

static const HBRUSH hBrushNormal = CreateSolidBrush(COLOR_MODERN_BG);

// ============================================================
// Window Procedure
// ============================================================

LRESULT CALLBACK VideoPlayerGUI::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    VideoPlayerGUI* self = reinterpret_cast<VideoPlayerGUI*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (uMsg) {
        // ========================================================
        // WM_CREATE
        // ========================================================

    case WM_CREATE: {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);

        VideoPlayerGUI* pThis = reinterpret_cast<VideoPlayerGUI*>(pCreate->lpCreateParams);

        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));

        SetWindowTheme(hwnd, L"Explorer", nullptr);

        pThis->CreateMenuBar(hwnd);
        pThis->CreateControls(hwnd);
        pThis->UpdateMenuState(false);
        if (pThis->g_hTimeLabel) {
            SetWindowTextW(pThis->g_hTimeLabel, L"--:-- / --:--");
        }

        RECT rc{};
        GetClientRect(hwnd, &rc);

        pThis->LayoutControls(rc.right - rc.left, rc.bottom - rc.top);

        return 0;
    }

        // ========================================================
        // WM_ERASEBKGND
        // ========================================================

    case WM_ERASEBKGND: {
        if (!self)
            return 0;

        HDC hdc = reinterpret_cast<HDC>(wParam);

        RECT rc{};
        GetClientRect(hwnd, &rc);

        COLORREF bg = self->m_isFullscreen ? RGB(0, 0, 0) : COLOR_MODERN_BG;

        HBRUSH hBrush = CreateSolidBrush(bg);

        FillRect(hdc, &rc, hBrush);

        DeleteObject(hBrush);

        return 1;
    }

        // ========================================================
        // WM_CTLCOLORSTATIC
        // ========================================================

    case WM_CTLCOLORSTATIC: {
        if (!self)
            break;

        HDC hdc = reinterpret_cast<HDC>(wParam);
        HWND hCtl = reinterpret_cast<HWND>(lParam);

        // Tooltip waktu (hover di seek bar) — tetap dark
        if (hCtl == self->m_hTimeTip) {
            SetBkMode(hdc, OPAQUE);
            SetBkColor(hdc, COLOR_TIP_BG);
            SetTextColor(hdc, RGB(255, 255, 255));
            return (INT_PTR)hBrushTip;
        }

        // Time label — PUTIH + teks gelap
        if (hCtl == self->g_hTimeLabel) {
            SetBkMode(hdc, OPAQUE);
            SetBkColor(hdc, COLOR_MODERN_BG);
            SetTextColor(hdc, COLOR_MODERN_TEXT);
            return (INT_PTR)hBrushNormal;
        }

        // Fullscreen fallback (jarang kena karena overlay custom-paint)
        if (self->m_isFullscreen) {
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            return (INT_PTR)hBrushTimeFs;
        }

        // Static lainnya
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, COLOR_MODERN_TEXT);
        return (INT_PTR)hBrushNormal;
    }
        // ========================================================
        // WM_NOTIFY
        // ========================================================

    case WM_NOTIFY: {
        if (!self)
            break;

        LPNMHDR pnmh = reinterpret_cast<LPNMHDR>(lParam);

        if (!pnmh)
            break;

        if (pnmh->code != NM_CUSTOMDRAW)
            break;

        // ----------------------------------------------------
        // Progress bar
        // ----------------------------------------------------

        if (pnmh->hwndFrom == self->g_hProgress) {
            static bool s_warned = false;
            if (!s_warned) {
                s_warned = true;
                OutputDebugStringW(L"[VIDI] WARNING: NM_CUSTOMDRAW "
                                   L"reached for g_hProgress "
                                   L"(should not happen after "
                                   L"WM_PAINT direct-draw fix)\n");
            }

            LPNMCUSTOMDRAW pcd = reinterpret_cast<LPNMCUSTOMDRAW>(lParam);

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

        // ----------------------------------------------------
        // Volume bar
        // ----------------------------------------------------

        if (pnmh->hwndFrom == self->g_hVolume) {
            LPNMCUSTOMDRAW pcd = reinterpret_cast<LPNMCUSTOMDRAW>(lParam);

            if (pcd->dwDrawStage == CDDS_PREPAINT)
                return CDRF_NOTIFYITEMDRAW;

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

        // ========================================================
        // WM_GETMINMAXINFO
        // ========================================================

    case WM_GETMINMAXINFO: {
        MINMAXINFO* pmmi = reinterpret_cast<MINMAXINFO*>(lParam);

        if (!pmmi)
            return 0;

        double dpi = self ? GetDpiScale(hwnd) : 1.0;

        pmmi->ptMinTrackSize.x = static_cast<LONG>(500 * dpi);

        pmmi->ptMinTrackSize.y = static_cast<LONG>(320 * dpi);

        HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);

        MONITORINFO mi{sizeof(mi)};

        if (GetMonitorInfo(mon, &mi)) {
            pmmi->ptMaxTrackSize.x = mi.rcWork.right - mi.rcWork.left;

            pmmi->ptMaxTrackSize.y = mi.rcWork.bottom - mi.rcWork.top;
        }

        return 0;
    }

        // ========================================================
        // WM_DPICHANGED
        // ========================================================

    case WM_DPICHANGED: {
        if (self) {
            RECT rcTarget{};

            if (self->m_isFullscreen) {
                MONITORINFO mi{sizeof(mi)};

                HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);

                if (!GetMonitorInfo(mon, &mi))
                    break;

                rcTarget = mi.rcMonitor;
            } else {
                RECT* prc = reinterpret_cast<RECT*>(lParam);

                if (!prc)
                    break;

                rcTarget = *prc;
            }

            SetWindowPos(hwnd, nullptr, rcTarget.left, rcTarget.top, rcTarget.right - rcTarget.left,
                         rcTarget.bottom - rcTarget.top, SWP_NOZORDER | SWP_NOACTIVATE);

            if (self->m_hModernFont)
                DeleteObject(self->m_hModernFont);

            if (self->m_hTimeFont)
                DeleteObject(self->m_hTimeFont);

            if (self->m_hTipFont)
                DeleteObject(self->m_hTipFont);

            double dpi = GetDpiScale(hwnd);

            self->m_hModernFont =
                CreateFontW(static_cast<int>(-14 * dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");

            self->m_hTimeFont =
                CreateFontW(static_cast<int>(-15 * dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");

            self->m_hTipFont =
                CreateFontW(static_cast<int>(-12 * dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");

            if (self->g_hTimeLabel) {
                SendMessage(self->g_hTimeLabel, WM_SETFONT, reinterpret_cast<WPARAM>(self->m_hTimeFont), TRUE);
            }

            HWND hc = GetWindow(hwnd, GW_CHILD);

            while (hc) {
                if (hc != self->g_hTimeLabel) {
                    SendMessage(hc, WM_SETFONT, reinterpret_cast<WPARAM>(self->m_hModernFont), TRUE);
                }

                hc = GetNextWindow(hc, GW_HWNDNEXT);
            }

            RECT rcC{};
            GetClientRect(hwnd, &rcC);

            self->LayoutControls(rcC.right - rcC.left, rcC.bottom - rcC.top);
        }

        return 0;
    }

        // ========================================================
        // WM_SIZE
        // ========================================================

    case WM_SIZE: {
        if (!self)
            break;

        if (wParam == SIZE_MINIMIZED) {
            self->m_wasMinimized = true;

            // Subtitle juga tidak perlu terlihat ketika
            // window sedang minimized.
            for (int i = 0; i < MAX_SUB_OVERLAYS; ++i) {
                if (self->m_hSubOverlay[i]) {
                    ShowWindow(self->m_hSubOverlay[i], SW_HIDE);
                }
            }

            return 0;
        }

        // ----------------------------------------------------
        // Restore dari minimized
        // ----------------------------------------------------

        if (self->m_wasMinimized) {
            self->m_wasMinimized = false;

            self->m_player.UpdateVideoSize();

            self->RecoverVideo();
        }

        RECT rcSize{};

        if (!GetClientRect(hwnd, &rcSize)) {
            return 0;
        }

        const int clientW = rcSize.right - rcSize.left;

        const int clientH = rcSize.bottom - rcSize.top;

        // ----------------------------------------------------
        // Layout
        // ----------------------------------------------------

        if (self->m_isFullscreen) {
            self->LayoutFsOverlay(clientW, clientH);
        } else {
            self->LayoutControls(clientW, clientH);
        }

        // ----------------------------------------------------
        // Update video size
        // ----------------------------------------------------

        self->m_player.UpdateVideoSize();

        // ----------------------------------------------------
        // Recover frame
        // ----------------------------------------------------

        if (wParam == SIZE_MAXIMIZED || wParam == SIZE_RESTORED) {
            self->RecoverVideo();
        }

        // ----------------------------------------------------
        // Subtitle geometry refresh
        //
        // Hanya lakukan kalau aplikasi aktif.
        // Kalau Alt+Tab, overlay harus tetap hidden.
        // ----------------------------------------------------

        if (self->m_appActivate && !self->m_subsHidden) {
            self->UpdateSubtitleDisplays(self->m_lastSubPosition, true);
        }

        return 0;
    }

        // ========================================================
        // WM_EXITSIZEMOVE
        // ========================================================

    case WM_EXITSIZEMOVE: {
        if (self) {
            self->m_player.UpdateVideoSize();

            self->RecoverVideo();

            if (self->m_appActivate && !self->m_subsHidden) {
                self->UpdateSubtitleDisplays(self->m_lastSubPosition, true);
            }
        }

        return 0;
    }

        // ========================================================
        // WM_MOUSEMOVE
        //
        // FIX: reset hide-timer & unhide cursor saat fullscreen,
        // supaya overlay mau muncul lagi walau cursor bergerak
        // di area video (di luar overlay).
        // ========================================================

    case WM_MOUSEMOVE: {
        if (!self)
            break;

        if (self->m_isFullscreen) {
            if (self->m_cursorHidden) {
                ShowCursor(TRUE);
                self->m_cursorHidden = false;
            }
            self->PokeOSControls();
        }

        int x = static_cast<int>(static_cast<short>(LOWORD(lParam)));

        if (GetCapture() == hwnd && self->m_isDraggingProgress) {
            self->DragSeekTo(x);
        } else if (self->g_hProgress) {
            RECT rcProg{};

            GetWindowRect(self->g_hProgress, &rcProg);

            POINT pt{x, static_cast<LONG>(static_cast<short>(HIWORD(lParam)))};

            ClientToScreen(hwnd, &pt);

            if (!PtInRect(&rcProg, pt)) {
                TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, hwnd, 0};

                TrackMouseEvent(&tme);
            }
        }

        return 0;
    }

        // ========================================================
        // WM_COMMAND
        // ========================================================

    case WM_COMMAND: {
        if (self) {
            self->OnCommand(wParam, lParam);
        }

        return 0;
    }

        // ========================================================
        // WM_HSCROLL
        // ========================================================

    case WM_HSCROLL: {
        if (self) {
            self->OnHScroll(wParam, lParam);
        }

        return 0;
    }

        // ========================================================
        // WM_TIMER
        //
        // FIX: JANGAN reset timer di dalam WM_TIMER.
        // Timer fire = sudah 2.5 dtk tidak ada gerakan mouse.
        // Reset hanya boleh dari WM_MOUSEMOVE → PokeOSControls().
        // ========================================================

    case WM_TIMER: {
        if (!self)
            break;

        if (wParam == ID_TIMER_UPDATE) {
            self->OnTimerTick();
        } else if (wParam == ID_TIMER_OSI_HIDE) {
            if (self->m_isFullscreen && !self->m_isDraggingProgress) {
                self->ShowOSControls(false);
                if (!self->m_cursorHidden) {
                    ShowCursor(FALSE);
                    self->m_cursorHidden = true;
                }
            }
        } else if (wParam == ID_TIMER_SUBTITLE_REFRESH) {
            KillTimer(hwnd, ID_TIMER_SUBTITLE_REFRESH);

            // Safety guard: timer ini hanya boleh jalan saat VSFilter aktif.
            // Jika tidak, skip — trick pause-seek-play akan menyebabkan stutter.
            if (!self->m_player.IsVSFilterSubtitleActive()) {
                OutputDebugStringW(L"[VIDI] Subtitle refresh timer fired without VSFilter — skip\n");
                return 0;
            }

            OutputDebugStringW(L"[VIDI] Subtitle refresh: pause-play-seek trick\n");
            self->m_player.Pause();
            double p = self->m_player.GetPosition();
            self->m_player.Seek(p + 0.1);
            self->m_player.Play();
        }

        return 0;
    }

        // ========================================================
        // WM_APP_MEDIA_READY
        // ========================================================

    case WM_APP_MEDIA_READY: {
        if (self) {
            uint32_t gen = static_cast<uint32_t>(wParam);

            if (gen == self->m_player.GetMediaReadyGen()) {
                self->m_lastMediaReadyGen = gen;

                self->OnMediaReady();
            } else {
                OutputDebugStringW(L"[VIDI] Ignoring stale "
                                   L"WM_APP_MEDIA_READY\n");
            }
        }

        return 0;
    }

        // ========================================================
        // WM_ACTIVATE
        //
        // KHUSUS fullscreen.
        //
        // Jangan mengubah m_subsHidden.
        // ========================================================

    case WM_ACTIVATE: {

        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

        // ========================================================
        // WM_ACTIVATEAPP
        //
        // INI YANG MENGONTROL ALT+TAB.
        //
        // FIX: kill hide-timer saat app kehilangan fokus,
        // supaya timer tidak fire saat app di background.
        // ========================================================

    case WM_ACTIVATEAPP: {
        if (!self)
            return 0;

        if (wParam == FALSE) {
            self->m_appActivate = false;

            // Matikan hide-timer saat app tidak fokus.
            KillTimer(hwnd, ID_TIMER_OSI_HIDE);

            // Sembunyikan subtitle
            for (int i = 0; i < MAX_SUB_OVERLAYS; ++i) {
                if (self->m_hSubOverlay[i])
                    ShowWindow(self->m_hSubOverlay[i], SW_HIDE);
            }

            // Sembunyikan fullscreen overlay
            if (self->m_hFsOverlay)
                ShowWindow(self->m_hFsOverlay, SW_HIDE);

            // Paksa SEMUA child window Vidi turun.
            EnumChildWindows(
                hwnd,
                [](HWND child, LPARAM) -> BOOL {
                    SetWindowPos(child, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

                    return TRUE;
                },
                0);

            // Window utama juga turun.
            SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

            return 0;
        }

        // Kembali ke Vidi
        self->m_appActivate = true;

        if (!self->m_subsHidden) {
            self->UpdateSubtitleDisplays(self->m_lastSubPosition, true);
        }

        return 0;
    }

        // ========================================================
        // WM_APP_FS_DEACTIVATE
        // ========================================================

    case WM_APP_FS_DEACTIVATE: {

        return 0;
    }

        // ========================================================
        // WM_APP_FS_ACTIVATE
        // ========================================================

    case WM_APP_FS_ACTIVATE: {
        if (self && self->m_isFullscreen) {
            OutputDebugStringW(L"[VIDI] "
                               L"WM_APP_FS_ACTIVATE START\n");

            HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);

            MONITORINFO mi{sizeof(mi)};

            if (GetMonitorInfo(mon, &mi)) {
                RECT rcWindow{};

                GetWindowRect(hwnd, &rcWindow);

                int winW = rcWindow.right - rcWindow.left;

                int winH = rcWindow.bottom - rcWindow.top;

                int monW = mi.rcMonitor.right - mi.rcMonitor.left;

                int monH = mi.rcMonitor.bottom - mi.rcMonitor.top;

                // ------------------------------------------------
                // Pastikan fullscreen memenuhi monitor.
                // ------------------------------------------------

                if (winW != monW || winH != monH || rcWindow.left != mi.rcMonitor.left ||
                    rcWindow.top != mi.rcMonitor.top) {
                    SetWindowPos(hwnd, HWND_TOPMOST, mi.rcMonitor.left, mi.rcMonitor.top, monW, monH,
                                 SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
                } else {
                    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOACTIVATE);
                }

                // ------------------------------------------------
                // Client area aktual
                // ------------------------------------------------

                RECT rcClient{};

                GetClientRect(hwnd, &rcClient);

                int clientW = rcClient.right - rcClient.left;

                int clientH = rcClient.bottom - rcClient.top;

                self->LayoutFsOverlay(clientW, clientH);

                // Re-create overlay if destroyed during Alt+Tab
                if (!self->m_hFsOverlay) {
                    self->CreateFsOverlay();
                    self->LayoutFsOverlay(monW, monH);
                }

                self->m_player.UpdateVideoSize();

                self->RecoverVideo();
            }

            // ----------------------------------------------------
            // Restore subtitle state.
            // ----------------------------------------------------

            if (self->m_subsHidden) {
                self->m_subsHidden = false;
            }

            // ----------------------------------------------------
            // Pastikan subtitle mengikuti fullscreen geometry.
            // ----------------------------------------------------

            self->m_appActivate = true;

            self->UpdateSubtitleDisplays(self->m_lastSubPosition, true);

            OutputDebugStringW(L"[VIDI] "
                               L"WM_APP_FS_ACTIVATE END\n");
        }

        return 0;
    }

        // ========================================================
        // WM_WTSSESSION_CHANGE
        // ========================================================

    case WM_WTSSESSION_CHANGE: {
        if (self) {
            if (wParam == WTS_SESSION_UNLOCK || wParam == WTS_REMOTE_CONNECT || wParam == WTS_CONSOLE_CONNECT) {
                self->RecoverVideo();
            }
        }

        return 0;
    }

        // ========================================================
        // WM_DISPLAYCHANGE
        // ========================================================

    case WM_DISPLAYCHANGE: {
        if (self) {
            self->m_player.UpdateVideoSize();

            self->RecoverVideo();

            if (self->m_appActivate && !self->m_subsHidden) {
                self->UpdateSubtitleDisplays(self->m_lastSubPosition, true);
            }
        }

        return 0;
    }

        // ========================================================
        // WM_QUERYENDSESSION
        // ========================================================

    case WM_QUERYENDSESSION:
        return TRUE;

        // ========================================================
        // WM_ENDSESSION
        // ========================================================

    case WM_ENDSESSION: {
        if (self && wParam) {
            self->m_player.Stop();

            self->SetPlayPauseUI(false);
        }

        return 0;
    }

        // ========================================================
        // WM_APP_GRAPH_EVENT
        // ========================================================

    case WM_APP_GRAPH_EVENT: {
        if (self) {
            self->m_player.HandleGraphEvent();
        }

        return 0;
    }

        // ========================================================
        // WM_APP_PLAYBACK_ENDED
        // ========================================================

    case WM_APP_PLAYBACK_ENDED: {
        if (self) {
            // ===== AUTO-NEXT LOGIC =====
            bool handled = false;

            // 1. Kalau shuffle aktif → pilih random (bukan yang sekarang)
            if (self->m_isShuffle && self->m_playlist.size() > 1) {
                int next;
                do {
                    next = rand() % (int)self->m_playlist.size();
                } while (next == self->m_playlistIndex);
                self->PlayFileFromPlaylist(next);
                handled = true;
            }
            // sesi Kalau ada file berikutnya → lanjut
            else if (self->m_playlistIndex + 1 < (int)self->m_playlist.size()) {
                self->PlayFileFromPlaylist(self->m_playlistIndex + 1);
                handled = true;
            }
            //  file terakhir & loop aktif → ulang dari awal
            else if (self->m_isLooping && !self->m_playlist.empty()) {
                self->PlayFileFromPlaylist(0);
                handled = true;
            }

            // Kalau tidak ada yang handle → stop biasa
            if (!handled) {
                self->SetPlayPauseUI(false);
                self->SetProgressPos(self->m_progressRangeMax);
                self->UpdateMenuState(false);
                self->HideSubOverlayWindows();
            }
        }
        return 0;
    }

        // ========================================================
        // WM_APP_AUDIO_MISSING
        // ========================================================

    case WM_APP_AUDIO_MISSING: {
        MessageBox(hwnd,
                   L"File ini punya track audio "
                   L"tapi codec-nya tidak ditemukan.\n"
                   L"Video tetap diputar tanpa suara.",
                   L"Vidi Player", MB_OK | MB_ICONWARNING);

        break;
    }

        // ========================================================
        // WM_APP_MEDIA_ERROR
        // ========================================================

    case WM_APP_MEDIA_ERROR: {
        if (self) {
            MessageBox(self->g_hMainWnd,
                       L"Gagal memutar file "
                       L"(graph error).",
                       L"Vidi", MB_OK | MB_ICONERROR);

            self->SetPlayPauseUI(false);

            self->SetProgressPos(0);
            self->UpdateMenuState(false);
        }

        return 0;
    }

        // ========================================================
        // WM_SYSKEYDOWN
        // ========================================================

    case WM_SYSKEYDOWN: {
        if (wParam == VK_F4 && (GetAsyncKeyState(VK_MENU) & 0x8000)) {
            SendMessage(hwnd, WM_CLOSE, 0, 0);

            return 0;
        }

        break;
    }

        // ========================================================
        // WM_SYSCOMMAND
        // ========================================================

    case WM_SYSCOMMAND: {
        if ((wParam & 0xFFF0) == SC_CLOSE) {
            if (self) {
                self->m_isClosing = true;
            }
        }

        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

        // ========================================================
        // WM_CLOSE
        // ========================================================

    case WM_CLOSE: {
        if (self) {
            self->m_isClosing = true;
        }

        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

        // ========================================================
        // WM_DESTROY
        //
        // FIX: kill ID_TIMER_OSI_HIDE juga, bukan cuma UPDATE.
        // ========================================================

    case WM_DESTROY: {
        if (!self) {
            PostQuitMessage(0);
            return 0;
        }

        if (self->m_hModernFont)
            DeleteObject(self->m_hModernFont);

        if (self->m_hTimeFont)
            DeleteObject(self->m_hTimeFont);

        if (self->m_hTipFont)
            DeleteObject(self->m_hTipFont);

        if (self->m_hSubFont)
            DeleteObject(self->m_hSubFont);

        for (auto& [h, f] : self->m_subFontCache) {
            DeleteObject(f);
        }

        self->m_subFontCache.clear();

        for (int i = 0; i < MAX_SUB_OVERLAYS; ++i) {
            if (self->m_hSubBmp[i]) {
                DeleteObject(self->m_hSubBmp[i]);

                self->m_hSubBmp[i] = nullptr;
            }
        }

        for (int i = 0; i < MAX_SUB_OVERLAYS; ++i) {
            if (self->m_hSubOverlay[i]) {
                ShowWindow(self->m_hSubOverlay[i], SW_HIDE);

                DestroyWindow(self->m_hSubOverlay[i]);

                self->m_hSubOverlay[i] = nullptr;
            }
        }

        if (self->m_hFsOverlay) {
            ShowWindow(self->m_hFsOverlay, SW_HIDE);
            DestroyWindow(self->m_hFsOverlay);
            self->m_hFsOverlay = nullptr;
        }

        WTSUnRegisterSessionNotification(hwnd);

        KillTimer(hwnd, ID_TIMER_UPDATE);
        KillTimer(hwnd, ID_TIMER_OSI_HIDE);
        KillTimer(hwnd, ID_TIMER_SUBTITLE_REFRESH);
        PostQuitMessage(0);

        return 0;
    }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

} // namespace guiVidi
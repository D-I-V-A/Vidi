#include "../../include/gui/gui.hh"
#include "../../include/gui/constants.hh"
#include "../../include/kernels/ids.hh"

#include <wtsapi32.h>
#include <dwmapi.h>

namespace guiVidi {
// pre-created GDI brushes
static const HBRUSH hBrushTip = CreateSolidBrush(COLOR_TIP_BG);
static const HBRUSH hBrushTimeFs = CreateSolidBrush(COLOR_TIP_BG);
static const HBRUSH hBrushNormal = CreateSolidBrush(COLOR_MODERN_BG);

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
        COLORREF bg = self->m_isFullscreen ? RGB(0, 0, 0) : COLOR_MODERN_BG;
        HBRUSH hBrush = CreateSolidBrush(bg);
        FillRect(hdc, &rc, hBrush);
        DeleteObject(hBrush);
        return 1;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;

        if ((HWND)lParam == self->m_hTimeTip) {
            SetBkColor(hdc, COLOR_TIP_BG);
            SetTextColor(hdc, RGB(255, 255, 255));
            return (INT_PTR)hBrushTip;
        }

        // [FIX] Semua kontrol dapat dark bg saat fullscreen
        if (self->m_isFullscreen) {
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            return (INT_PTR)hBrushTimeFs;
        }

        if ((HWND)lParam == self->g_hTimeLabel) {
            SetBkColor(hdc, COLOR_TIP_BG);
            SetTextColor(hdc, RGB(255, 255, 255));
            return (INT_PTR)hBrushTimeFs;
        }

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, COLOR_MODERN_TEXT);
        return (INT_PTR)hBrushNormal;
    }

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

        HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = {sizeof(mi)};
        if (GetMonitorInfo(mon, &mi)) {
            pmmi->ptMaxTrackSize.x = mi.rcWork.right - mi.rcWork.left;
            pmmi->ptMaxTrackSize.y = mi.rcWork.bottom - mi.rcWork.top;
        }
        return 0;
    }
    case WM_DPICHANGED: {
        if (self) {
            RECT rcTarget;
            if (self->m_isFullscreen) {
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
        {
            wchar_t dbg[256];
            swprintf_s(dbg, L"[VIDI] WM_SIZE wParam=%llu lParam=0x%llX fullscreen=%d minimized=%d wasMinimized=%d\n",
                       (unsigned long long)wParam, (unsigned long long)lParam, self->m_isFullscreen,
                       (wParam == SIZE_MINIMIZED), self->m_wasMinimized);
            OutputDebugStringW(dbg);
        }
        if (wParam == SIZE_MINIMIZED) {
            self->m_wasMinimized = true;
            return 0;
        }
        if (self->m_wasMinimized) {
            self->m_wasMinimized = false;
            self->m_player.UpdateVideoSize();
            self->RecoverVideo();
        }
        RECT rcSize;
        GetClientRect(hwnd, &rcSize);
        {
            wchar_t dbg[256];
            swprintf_s(dbg, L"[VIDI] WM_SIZE GetClientRect: %dx%d (lParam was %dx%d)\n", rcSize.right, rcSize.bottom,
                       (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
            OutputDebugStringW(dbg);
        }
        self->LayoutControls(rcSize.right, rcSize.bottom);
        if (wParam == SIZE_MAXIMIZED || wParam == SIZE_RESTORED)
            self->RecoverVideo();
        return 0;

    case WM_EXITSIZEMOVE:
        if (self) {
            self->m_player.UpdateVideoSize();
            self->RecoverVideo();
        }
        return 0;
    case WM_MOUSEMOVE: {
        if (!self)
            break;
        int x = (short)LOWORD(lParam);
        if (GetCapture() == hwnd && self->m_isDraggingProgress) {
            self->DragSeekTo(x);
        } else if (self->g_hProgress) {
            RECT rcProg;
            GetWindowRect(self->g_hProgress, &rcProg);
            POINT pt = {x, (short)HIWORD(lParam)};
            // Konversi ke screen coords untuk comparison
            ClientToScreen(hwnd, &pt);
            if (!PtInRect(&rcProg, pt)) {
                // Mouse bukan di progress bar → handle normal
                TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hwnd, 0};
                TrackMouseEvent(&tme);
            }
        }
        return 0;
    }
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
            if (self->m_isFullscreen && !self->m_isDraggingProgress) {
                if (self->CursorOverControls()) {
                    SetTimer(hwnd, ID_TIMER_OSI_HIDE, FULLSCREEN_HIDE_MS, nullptr);
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
        if (self) {
            uint32_t gen = (uint32_t)wParam;
            if (gen == self->m_player.GetMediaReadyGen()) {
                self->m_lastMediaReadyGen = gen;
                self->OnMediaReady();
            } else {
                OutputDebugStringW(L"[VIDI] Ignoring stale WM_APP_MEDIA_READY\n");
            }
        }
        return 0;
    case WM_ACTIVATE:
        if (self && self->m_isFullscreen) {
            if (wParam == WA_INACTIVE) {
                SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOACTIVATE);
                for (int i = 0; i < MAX_SUB_OVERLAYS; i++) {
                    if (self->m_hSubOverlay[i])
                        ShowWindow(self->m_hSubOverlay[i], SW_HIDE);
                }
            } else {
                PostMessage(hwnd, WM_APP_FS_ACTIVATE, 0, 0);
            }
        }
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    case WM_ACTIVATEAPP:
        if (self) {
            if (!wParam) {
                if (self->m_isClosing)
                    return 0;
                self->m_subsHidden = true;
                for (int i = 0; i < MAX_SUB_OVERLAYS; i++) {
                    if (self->m_hSubOverlay[i])
                        ShowWindow(self->m_hSubOverlay[i], SW_HIDE);
                }
                self->m_lastVideoClickTick = 0;
            } else {
                self->m_subsHidden = false;
                for (int i = 0; i < MAX_SUB_OVERLAYS; i++) {
                    if (self->m_hSubOverlay[i] && self->m_hSubBmp[i])
                        ShowWindow(self->m_hSubOverlay[i], SW_SHOW);
                }
            }
        }
        return 0;

    case WM_APP_FS_DEACTIVATE:
        if (self && self->m_isFullscreen && !self->m_isClosing) {
            self->ExitFullscreen();
        }
        return 0;

    case WM_APP_FS_ACTIVATE:
        if (self && self->m_isFullscreen) {
            HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = {sizeof(mi)};
            if (GetMonitorInfo(mon, &mi)) {
                RECT rcWindow;
                GetWindowRect(hwnd, &rcWindow);
                int winW = rcWindow.right - rcWindow.left;
                int winH = rcWindow.bottom - rcWindow.top;
                int monW = mi.rcMonitor.right - mi.rcMonitor.left;
                int monH = mi.rcMonitor.bottom - mi.rcMonitor.top;

                // Cek apakah ukuran window sudah sama dengan monitor
                if (winW != monW || winH != monH) {
                    // Resize ke ukuran monitor (hanya jika diperlukan)
                    SetWindowPos(hwnd, HWND_TOPMOST, mi.rcMonitor.left, mi.rcMonitor.top, monW, monH,
                                 SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
                    // Layout ulang karena ukuran berubah
                    RECT rcClient;
                    GetClientRect(hwnd, &rcClient);
                    self->LayoutFullscreen(rcClient.right, rcClient.bottom);
                } else {
                    // Ukuran sudah pas, cukup set topmost tanpa resize
                    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER);
                    RECT rcClient;
                    GetClientRect(hwnd, &rcClient);
                    self->LayoutFullscreen(rcClient.right, rcClient.bottom);
                }
            }
            // Restore subtitle yang di-hide saat WA_INACTIVE
            if (self->m_subsHidden) {
                self->m_subsHidden = false;
                for (int i = 0; i < MAX_SUB_OVERLAYS; i++) {
                    if (self->m_hSubOverlay[i] && self->m_hSubBmp[i])
                        ShowWindow(self->m_hSubOverlay[i], SW_SHOW);
                }
            }
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
        if (self && wParam) {
            self->m_player.Stop();
            self->SetPlayPauseUI(false);
        }
        return 0;

    case WM_APP_GRAPH_EVENT:
        if (self)
            self->m_player.HandleGraphEvent();
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
    case WM_SYSKEYDOWN:
        if (wParam == VK_F4 && (GetAsyncKeyState(VK_MENU) & 0x8000)) {
            SendMessage(hwnd, WM_CLOSE, 0, 0);
            return 0;
        }
        break;
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_CLOSE) {
            if (self)
                self->m_isClosing = true;
        }
        return DefWindowProc(hwnd, uMsg, wParam, lParam);

    case WM_CLOSE:
        if (self) {
            self->m_isClosing = true;
        }
        return DefWindowProc(hwnd, uMsg, wParam, lParam);

    case WM_DESTROY:
        if (self->m_hModernFont)
            DeleteObject(self->m_hModernFont);
        if (self->m_hTimeFont)
            DeleteObject(self->m_hTimeFont);
        if (self->m_hTipFont)
            DeleteObject(self->m_hTipFont);
        if (self->m_hSubFont)
            DeleteObject(self->m_hSubFont);
        for (auto& [h, f] : self->m_subFontCache)
            DeleteObject(f);
        self->m_subFontCache.clear();
        for (int i = 0; i < MAX_SUB_OVERLAYS; i++) {
            if (self->m_hSubBmp[i]) {
                DeleteObject(self->m_hSubBmp[i]);
                self->m_hSubBmp[i] = nullptr;
            }
        }
        for (int i = 0; i < MAX_SUB_OVERLAYS; i++) {
            if (self->m_hSubOverlay[i]) {
                ShowWindow(self->m_hSubOverlay[i], SW_HIDE);
                DestroyWindow(self->m_hSubOverlay[i]);
                self->m_hSubOverlay[i] = nullptr;
            }
        }
        WTSUnRegisterSessionNotification(hwnd);
        KillTimer(hwnd, ID_TIMER_UPDATE);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

} // namespace guiVidi
#include "../../include/gui/gui.hh"
#include "../../include/kernels/ids.hh"
#include <string>
#include <cmath>

namespace guiVidi {

// ==========================================
// LAYOUT CONTROLS
// ==========================================
void VideoPlayerGUI::LayoutControls(int width, int height) {
    const int MARGIN = 0;           // VLC pakai edge-to-edge
    const int BOTTOM_BAR_H = 60;    // Tinggi control bar ala VLC
    const int PROGRESS_H = 18;      // Hit-area trackbar (bar visual tipis digambar manual)
    const int PROGRESS_PAD_Y = 2;   // Padding vertikal progress
    const int BTN_SIZE = 36;
    const int GAP = 6;

    int videoHeight = height - BOTTOM_BAR_H;
    if (videoHeight < 100) videoHeight = 100;

    // 1. Video Area (full width, minus margin opsional)
    if (g_hVideoArea) {
        SetWindowPos(g_hVideoArea, NULL, MARGIN, MARGIN, 
                     width - (MARGIN*2), videoHeight, SWP_NOZORDER | SWP_SHOWWINDOW);
    }
    m_player.UpdateVideoSize();

    int barY = videoHeight;
    int progressY = barY + PROGRESS_PAD_Y;
    int btnRowY = progressY + PROGRESS_H + GAP;

    // 2. Progress Bar (Full Width, tipis di atas tombol)
    if (g_hProgress) {
        SetWindowPos(g_hProgress, NULL, 8, progressY, 
                     width - 16, PROGRESS_H, SWP_NOZORDER | SWP_SHOWWINDOW);
    }

    // 3. Tombol Playback (Rata Kiri)
    int btnX = 8;
    if (g_hSkipBack)     SetWindowPos(g_hSkipBack,     NULL, btnX, btnRowY, BTN_SIZE, BTN_SIZE, SWP_NOZORDER | SWP_SHOWWINDOW);
    btnX += BTN_SIZE + 4;
    if (g_hPlayBtn)      SetWindowPos(g_hPlayBtn,      NULL, btnX, btnRowY, BTN_SIZE, BTN_SIZE, SWP_NOZORDER | SWP_SHOWWINDOW);
    btnX += BTN_SIZE + 4;
    if (g_hPauseBtn)     SetWindowPos(g_hPauseBtn,     NULL, btnX, btnRowY, BTN_SIZE, BTN_SIZE, SWP_NOZORDER | SWP_SHOWWINDOW);
    btnX += BTN_SIZE + 4;
    if (g_hStopBtn)      SetWindowPos(g_hStopBtn,      NULL, btnX, btnRowY, BTN_SIZE, BTN_SIZE, SWP_NOZORDER | SWP_SHOWWINDOW);
    btnX += BTN_SIZE + 4;
    if (g_hSkipForward)  SetWindowPos(g_hSkipForward,  NULL, btnX, btnRowY, BTN_SIZE, BTN_SIZE, SWP_NOZORDER | SWP_SHOWWINDOW);

    // 4. Volume & Time (Rata Kanan)
    int rightX = width - 180;
    if (g_hVolume)       SetWindowPos(g_hVolume,       NULL, rightX, btnRowY + 6, 80, 24, SWP_NOZORDER | SWP_SHOWWINDOW);
    if (g_hTimeLabel)    SetWindowPos(g_hTimeLabel,    NULL, rightX + 90, btnRowY + 6, 80, 24, SWP_NOZORDER | SWP_SHOWWINDOW);
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

        // [VLC-STYLE] Custom draw seekbar: track abu tipis + fill oranye + thumb kotak putih
        case WM_NOTIFY:
        {
            LPNMHDR pnmh = (LPNMHDR)lParam;
            if (self && pnmh->hwndFrom == self->g_hProgress && pnmh->code == NM_CUSTOMDRAW) {
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
            break;
        }

        case WM_SIZE:
            if (self) self->LayoutControls(LOWORD(lParam), HIWORD(lParam));
            return 0;
        case WM_COMMAND:
            if(self) self->OnCommand(wParam,lParam);
            return 0;
        case WM_HSCROLL:
            if(self) self->OnHScroll(wParam,lParam);
            return 0;
        case WM_TIMER:
            if(wParam == ID_TIMER_UPDATE && self) self->OnTimerTick();
            return 0;
        case WM_DESTROY:
            if (self->m_hModernFont) DeleteObject(self->m_hModernFont);
            if (self->m_hTimeFont) DeleteObject(self->m_hTimeFont);
            if (self->m_hTipFont) DeleteObject(self->m_hTipFont);
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
            return 1;  // digambar sendiri di DrawVlcSeekbar (anti-flicker)

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

    wchar_t buf[16];
    swprintf_s(buf, L"%02d:%02d", (int)seconds / 60, (int)seconds % 60);
    SetWindowText(m_hTimeTip, buf);

    POINT pt = { m_hotX, -30 };
    ClientToScreen(g_hProgress, &pt);
    SetWindowPos(m_hTimeTip, HWND_TOPMOST, pt.x - 32, pt.y, 64, 22,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void VideoPlayerGUI::HideTimeTip() {
    if (m_hTimeTip) ShowWindow(m_hTimeTip, SW_HIDE);
}

// ==========================================
// MEDIA READY — set range trackbar sesuai durasi asli video
// ==========================================
void VideoPlayerGUI::OnMediaReady() {
    m_cachedDuration = m_player.GetDuration();
    double dur = m_cachedDuration;
    // Presisi 0.1 detik per step, proporsional ke durasi asli
    int range = static_cast<int>(dur * 10.0);
    if (range < 100) range = 100;           // minimum, jaga video super pendek
    if (range > 1000000) range = 1000000;   // batas aman biar nggak overflow

    m_progressRangeMax = range;
    SendMessage(g_hProgress, TBM_SETRANGE, TRUE, MAKELPARAM(0, m_progressRangeMax));
    SetProgressPos(0);
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
            vol = (vol + 10 > 100) ? 100 : vol + 10;
            SendMessage(g_hVolume, TBM_SETPOS, TRUE, vol);
            m_player.SetVolume(vol / 100.0f);
            break;
        }
        case IDM_AUDIO_VOLDOWN: {
            int vol = (int)SendMessage(g_hVolume, TBM_GETPOS, 0, 0);
            vol = (vol - 10 < 0) ? 0 : vol - 10;
            SendMessage(g_hVolume, TBM_SETPOS, TRUE, vol);
            m_player.SetVolume(vol / 100.0f);
            break;
        }
        case IDM_AUDIO_MUTE:
            ToggleMute();
            break;

        case IDM_VIEW_FULLSCREEN:
            ToggleFullscreen();
            break;

        case IDM_HELP_ABOUT:
            MessageBox(g_hMainWnd,
                L"Vidi Video Player\nDibangun dengan Win32 + Media Foundation",
                L"About Vidi", MB_OK | MB_ICONINFORMATION);
            break;
    }
}

// implementasi toggle and fullscreen toggle
void VideoPlayerGUI::ToggleMute() {
    if (!m_isMuted) {
        m_lastVolume = SendMessage(g_hVolume, TBM_GETPOS, 0, 0) / 100.0f;
        m_player.SetVolume(0.0f);
        SendMessage(g_hVolume, TBM_SETPOS, TRUE, 0);
        m_isMuted = true;
    } else {
        m_player.SetVolume(m_lastVolume);
        SendMessage(g_hVolume, TBM_SETPOS, TRUE, static_cast<int>(m_lastVolume * 100));
        m_isMuted = false;
    }
}

void VideoPlayerGUI::ToggleFullscreen() {
    DWORD style = GetWindowLong(g_hMainWnd, GWL_STYLE);

    if (style & WS_OVERLAPPEDWINDOW) {
        MONITORINFO mi = { sizeof(mi) };
        if (GetWindowPlacement(g_hMainWnd, &m_prevPlacement) &&
            GetMonitorInfo(MonitorFromWindow(g_hMainWnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLong(g_hMainWnd, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(g_hMainWnd, HWND_TOP,
                mi.rcMonitor.left, mi.rcMonitor.top,
                mi.rcMonitor.right - mi.rcMonitor.left,
                mi.rcMonitor.bottom - mi.rcMonitor.top,
                SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    } else {
        SetWindowLong(g_hMainWnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(g_hMainWnd, &m_prevPlacement);
        SetWindowPos(g_hMainWnd, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
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
        m_player.SetVolume(vol / 100.0f);
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
    if (m_isDraggingProgress) return;

    double dur = m_cachedDuration;
    // menambahkan logic mkv dengan durasi nya cokk
    if(dur <=0.0){
        double fresh = m_player.GetDuration();
        if(fresh > 0.0){
            m_cachedDuration = fresh;
            dur = fresh;
            int range = static_cast<int>(dur * 10.0);
            if (range < 100) range = 100;
            if (range > 1000000) range = 1000000;
            m_progressRangeMax = range;
            SendMessage(g_hProgress, TBM_SETRANGE, TRUE, MAKELPARAM(0, m_progressRangeMax));
        }else{
            return ;
        }
    }
    if (m_hasPendingSeek) {
        double actualPos = m_player.GetPosition();
        DWORD elapsed = GetTickCount() - m_pendingSeekStartTick;
        bool settled = (fabs(actualPos - m_pendingSeekTarget) < 1.0) || (elapsed > 3000);

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
    int sliderPos = static_cast<int>((pos/dur)* m_progressRangeMax);
    SetProgressPos(sliderPos);
    UpdateTimeLabel(pos, dur);
}

void VideoPlayerGUI::UpdateTimeLabel(double posSeconds, double durSeconds) {
    wchar_t buf[64];
    int posMin = (int)posSeconds / 60, posSec = (int)posSeconds % 60;
    int durMin = (int)durSeconds / 60, durSec = (int)durSeconds % 60;

    swprintf_s(buf, L"%02d:%02d / %02d:%02d", posMin, posSec, durMin, durSec);
    SetWindowText(g_hTimeLabel, buf);
}

void VideoPlayerGUI::SetPlayPauseUI(bool playing) {
    m_isPlaying = playing;
    EnableWindow(g_hPlayBtn, !playing);
    EnableWindow(g_hPauseBtn, playing);
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
    SendMessage(g_hProgress, TBM_SETTHUMBLENGTH, 0, 10);   // channel ramping
    SetWindowSubclass(g_hProgress, ProgressSubclassProc, 1, (DWORD_PTR)this);

    // Tooltip waktu seekbar (popup gelap di atas cursor)
    m_hTimeTip = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        L"STATIC", L"", WS_POPUP | SS_CENTER | SS_CENTERIMAGE,
        0, 0, 64, 22, hwnd, nullptr, nullptr, nullptr);
    SetWindowLongPtr(m_hTimeTip, -8 /* GWL_HWNDPARENT */, (LONG_PTR)hwnd);  // route WM_CTLCOLORSTATIC
    SendMessage(m_hTimeTip, WM_SETFONT, (WPARAM)m_hTipFont, TRUE);

    // Volume Slider
    g_hVolume = CreateWindowExW(0, TRACKBAR_CLASS, L"", 
        WS_CHILD | TBS_HORZ | TBS_NOTICKS, 
        0, 0, 100, 24, hwnd, (HMENU)IDC_VOLUME, nullptr, nullptr);
    SetWindowTheme(g_hVolume, L" ", L" ");
    SendMessage(g_hVolume, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
    SendMessage(g_hVolume, TBM_SETPOS, TRUE, 100);

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

    m_hAccel = CreatePlayerAccelTable();   // <-- tambahin ini

    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);
    return true;
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
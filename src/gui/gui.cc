#include "../../include/gui/gui.hh"
#include "../../include/kernels/ids.hh"
#include <string>
#include <cmath>

namespace guiVidi {

// ==========================================
// LAYOUT CONTROLS
// ==========================================
void VideoPlayerGUI::LayoutControls(int width, int height) {
    const int MARGIN       = 10;
    const int PROGRESS_H   = 25;
    const int BUTTON_ROW_H = 30;
    const int GAP          = 8;

    int bottomBarHeight = PROGRESS_H + GAP + BUTTON_ROW_H + MARGIN;
    int videoHeight = height - bottomBarHeight - MARGIN;

    if (g_hVideoArea)
        SetWindowPos(g_hVideoArea, NULL, MARGIN, MARGIN, width - (MARGIN * 2), videoHeight, SWP_NOZORDER);

    int videoBottomY = MARGIN + videoHeight;
    int progressY = videoBottomY + GAP;

    if (g_hProgress)
        SetWindowPos(g_hProgress, NULL, MARGIN, progressY, width - (MARGIN * 2), PROGRESS_H, SWP_NOZORDER);

    int buttonY = progressY + PROGRESS_H + GAP;

    if (g_hSkipBack)
        SetWindowPos(g_hSkipBack, NULL, MARGIN, buttonY, 40, BUTTON_ROW_H, SWP_NOZORDER);
    if (g_hPlayBtn)
        SetWindowPos(g_hPlayBtn, NULL, MARGIN + 45, buttonY, 40, BUTTON_ROW_H, SWP_NOZORDER);
    if (g_hPauseBtn)
        SetWindowPos(g_hPauseBtn, NULL, MARGIN + 90, buttonY, 40, BUTTON_ROW_H, SWP_NOZORDER);
    if (g_hStopBtn)
        SetWindowPos(g_hStopBtn, NULL, MARGIN + 135, buttonY, 40, BUTTON_ROW_H, SWP_NOZORDER);
    if (g_hSkipForward)
        SetWindowPos(g_hSkipForward, NULL, MARGIN + 180, buttonY, 40, BUTTON_ROW_H, SWP_NOZORDER);

    if (g_hTimeLabel)
        SetWindowPos(g_hTimeLabel, NULL, width - MARGIN - 150, buttonY, 100, BUTTON_ROW_H, SWP_NOZORDER);
    if (g_hVolume)
        SetWindowPos(g_hVolume, NULL, width - MARGIN - 260, buttonY, 100, BUTTON_ROW_H, SWP_NOZORDER);
}

// ==========================================
// WINDOW PROC
// ==========================================
LRESULT CALLBACK VideoPlayerGUI::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    VideoPlayerGUI* self = reinterpret_cast<VideoPlayerGUI*>(
        GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (uMsg) {
        case WM_CREATE:
        {
            CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
            VideoPlayerGUI* pThis = reinterpret_cast<VideoPlayerGUI*>(pCreate->lpCreateParams);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);

            pThis->CreateMenuBar(hwnd);
            pThis->CreateControls(hwnd);
            return 0;
        }

        case WM_SIZE:
        {
            if (self) self->LayoutControls(LOWORD(lParam), HIWORD(lParam));
            return 0;
        }

        case WM_COMMAND:
            if (self) self->OnCommand(wParam, lParam);
            return 0;

        case WM_HSCROLL:
            if (self) self->OnHScroll(wParam, lParam);
            return 0;

        case WM_TIMER:
            if (wParam == ID_TIMER_UPDATE && self) self->OnTimerTick();
            return 0;

        case WM_APP_MEDIA_READY:
            if (self) {
                self->OnMediaReady();
                self->SetPlayPauseUI(true);
            }
            return 0;

        case WM_APP_PLAYBACK_ENDED:
            if (self) self->SetPlayPauseUI(false);
            return 0;

        case WM_APP_MEDIA_ERROR:
            MessageBox(hwnd, L"Gagal memutar video.", L"Error", MB_ICONERROR);
            return 0;

        case WM_DESTROY:
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

    if (uMsg == WM_LBUTTONDOWN && self) {
        int mouseX = LOWORD(lParam);
        self->SeekFromTrackbarClick(mouseX);
    }

    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

void VideoPlayerGUI::SeekFromTrackbarClick(int mouseX) {
    RECT rc;
    GetClientRect(g_hProgress, &rc);
    if (rc.right <= 0) return;

    double ratio = static_cast<double>(mouseX) / static_cast<double>(rc.right);
    if (ratio < 0.0) ratio = 0.0;
    if (ratio > 1.0) ratio = 1.0;

    int newPos = static_cast<int>(ratio * m_progressRangeMax);
    SendMessage(g_hProgress, TBM_SETPOS, TRUE, newPos);

    double targetSec = ratio * m_cachedDuration;

    UpdateTimeLabel(targetSec, m_cachedDuration);
    m_player.Seek(targetSec);

    m_hasPendingSeek = true;
    m_pendingSeekTarget = targetSec;
    m_pendingSeekStartTick = GetTickCount();
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
    SendMessage(g_hProgress, TBM_SETPOS, TRUE, 0);
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
            SendMessage(g_hProgress, TBM_SETPOS, TRUE, 0);
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
// HSCROLL — drag & klik trackbar
// ==========================================
void VideoPlayerGUI::OnHScroll(WPARAM wParam, LPARAM lParam) {
    HWND hCtrl = (HWND)lParam;
    int code = LOWORD(wParam);

    if (hCtrl == g_hProgress) {
        if (code == TB_THUMBTRACK || code == TB_THUMBPOSITION) {
            m_isDraggingProgress = true;

            int pos = (int)SendMessage(g_hProgress, TBM_GETPOS, 0, 0);
            double targetSec = (static_cast<double>(pos) / m_progressRangeMax) * m_cachedDuration;

            UpdateTimeLabel(targetSec, m_cachedDuration);

            DWORD now = GetTickCount();
            if (now - m_lastSeekTick > 120) {
                m_player.Seek(targetSec);
                m_lastSeekTick = now;
            }
        }

        if (code == TB_ENDTRACK) {
            int pos = (int)SendMessage(g_hProgress, TBM_GETPOS, 0, 0);
            double targetSec = (static_cast<double>(pos) / m_progressRangeMax) * m_cachedDuration;
            m_player.Seek(targetSec);
            m_isDraggingProgress = false;

            m_hasPendingSeek = true;
            m_pendingSeekTarget = targetSec;
            m_pendingSeekStartTick = GetTickCount();
        }
    }
    else if (hCtrl == g_hVolume) {
        int vol = (int)SendMessage(g_hVolume, TBM_GETPOS, 0, 0);
        m_player.SetVolume(vol / 100.0f);
    }
}

// ==========================================
// TIMER TICK — auto update posisi & label
// ==========================================
void VideoPlayerGUI::OnTimerTick() {
    if (m_isDraggingProgress) return;

    double dur = m_cachedDuration;

    if (m_hasPendingSeek) {
        double actualPos = m_player.GetPosition();
        DWORD elapsed = GetTickCount() - m_pendingSeekStartTick;
        bool settled = (fabs(actualPos - m_pendingSeekTarget) < 1.0) || (elapsed > 3000);

        if (settled) {
            m_hasPendingSeek = false;
        } else {
            if (dur > 0.0) {
                int sliderPos = static_cast<int>((m_pendingSeekTarget / dur) * m_progressRangeMax);
                SendMessage(g_hProgress, TBM_SETPOS, TRUE, sliderPos);
            }
            UpdateTimeLabel(m_pendingSeekTarget, dur);
            return;
        }
    }

    double pos = m_player.GetPosition();
    if (dur > 0.0) {
        int sliderPos = static_cast<int>((pos / dur) * m_progressRangeMax);
        SendMessage(g_hProgress, TBM_SETPOS, TRUE, sliderPos);
    }
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
    wchar_t filePath[MAX_PATH] = L"";

    OPENFILENAME ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hMainWnd;
    ofn.lpstrFilter = L"Video Files\0*.mp4;*.avi;*.wmv;*.mkv\0All Files\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileName(&ofn)) {
        m_player.OpenFile(filePath);
        // playback beneran mulai setelah WM_APP_MEDIA_READY diterima
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

    g_hVideoArea = CreateWindowEx(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_BLACKRECT,
        0, 0, 640, 360, hwnd, nullptr, nullptr, nullptr);

    g_hPlayBtn = CreateWindow(L"BUTTON", L"Play", WS_CHILD | WS_VISIBLE,
        0, 0, 60, 30, hwnd, (HMENU)IDC_BTN_PLAY, nullptr, nullptr);

    g_hPauseBtn = CreateWindow(L"BUTTON", L"Pause", WS_CHILD | WS_VISIBLE,
        65, 0, 60, 30, hwnd, (HMENU)IDC_BTN_PAUSE, nullptr, nullptr);

    g_hStopBtn = CreateWindow(L"BUTTON", L"Stop", WS_CHILD | WS_VISIBLE,
        130, 0, 60, 30, hwnd, (HMENU)IDC_BTN_STOP, nullptr, nullptr);

    g_hSkipBack = CreateWindow(L"BUTTON", L"<<10s", WS_CHILD | WS_VISIBLE,
        195, 0, 60, 30, hwnd, (HMENU)IDC_BTN_SKIPBACK, nullptr, nullptr);

    g_hSkipForward = CreateWindow(L"BUTTON", L"10s>>", WS_CHILD | WS_VISIBLE,
        260, 0, 60, 30, hwnd, (HMENU)IDC_BTN_SKIPFORWARD, nullptr, nullptr);

    // Progress bar — range default sementara (0-1000), akan di-replace OnMediaReady()
    g_hProgress = CreateWindowEx(0, TRACKBAR_CLASS, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
        0, 40, 500, 30, hwnd, (HMENU)IDC_PROGRESS, nullptr, nullptr);
    SendMessage(g_hProgress, TBM_SETRANGE, TRUE, MAKELPARAM(0, m_progressRangeMax));
    SetWindowSubclass(g_hProgress, ProgressSubclassProc, 1, (DWORD_PTR)this);

    g_hVolume = CreateWindowEx(0, TRACKBAR_CLASS, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
        510, 40, 120, 30, hwnd, (HMENU)IDC_VOLUME, nullptr, nullptr);
    SendMessage(g_hVolume, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
    SendMessage(g_hVolume, TBM_SETPOS, TRUE, 100);

    g_hTimeLabel = CreateWindow(L"STATIC", L"00:00 / 00:00", WS_CHILD | WS_VISIBLE,
        0, 75, 200, 20, hwnd, (HMENU)IDC_TIME_LABEL, nullptr, nullptr);

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
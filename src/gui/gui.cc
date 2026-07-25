#include "../../include/gui/gui.hh"
#include "../../include/gui/utils.hh"
#include "../../include/kernels/ids.hh"
#include <string>

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

    // --- INI YANG KURANG ---
    if (g_hPauseBtn)
        SetWindowPos(g_hPauseBtn, NULL, MARGIN + 90, buttonY, 40, BUTTON_ROW_H, SWP_NOZORDER);

    // Geser Stop & SkipForward biar nggak numpuk sama Pause
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
        }   // <-- kurung tutup WAJIB ada persis di sini, sebelum case berikutnya

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
            if (self) self->SetPlayPauseUI(true);
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


void VideoPlayerGUI::OnCommand(WPARAM wParam, LPARAM lParam) {
    switch (LOWORD(wParam)) {
        case IDC_BTN_PLAY:
            m_player.Play();
            SetPlayPauseUI(true);
            break;
        case IDC_BTN_PAUSE:
            m_player.Pause();
            SetPlayPauseUI(false);
            break;
        case IDC_BTN_STOP:
            m_player.Stop();
            SetPlayPauseUI(false);
            SendMessage(g_hProgress, TBM_SETPOS, TRUE, 0);
            break;
        case IDC_BTN_SKIPBACK: {
            double pos = m_player.GetPosition();
            m_player.Seek(pos > 10.0 ? pos - 10.0 : 0.0);
            break;
        }
        case IDC_BTN_SKIPFORWARD: {
            double pos = m_player.GetPosition();
            double dur = m_player.GetDuration();
            m_player.Seek((pos + 10.0 < dur) ? pos + 10.0 : dur);
            break;
        }
        case IDM_FILE_OPEN:
            OpenFileDialog();
            break;
    }
}
void VideoPlayerGUI::OnHScroll(WPARAM wParam, LPARAM lParam) {
    HWND hCtrl = (HWND)lParam;
    int code = LOWORD(wParam);

    if (hCtrl == g_hProgress) {
        if (code == TB_THUMBTRACK || code == TB_THUMBPOSITION) {
            m_isDraggingProgress = true;  // freeze auto-update selama drag
        }
        if (code == TB_ENDTRACK) {
            int pos = (int)SendMessage(g_hProgress, TBM_GETPOS, 0, 0);
            double dur = m_player.GetDuration();
            double targetSec = (pos / 1000.0) * dur;
            m_player.Seek(targetSec);
            m_isDraggingProgress = false;
        }
    }
    else if (hCtrl == g_hVolume) {
        int vol = (int)SendMessage(g_hVolume, TBM_GETPOS, 0, 0);
        m_player.SetVolume(vol / 100.0f);
    }
}
void VideoPlayerGUI::OnTimerTick() {
    if (m_isDraggingProgress) return; // jangan overwrite pas user lagi drag

    double pos = m_player.GetPosition();
    double dur = m_player.GetDuration();

    if (dur > 0.0) {
        int sliderPos = static_cast<int>((pos / dur) * 1000.0);
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
    HMENU hFileMenu = CreatePopupMenu();

    AppendMenu(hFileMenu, MF_STRING, IDM_FILE_OPEN, L"Open...");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hFileMenu, L"File");

    SetMenu(hwnd, hMenuBar);
}
// ==========================================
// CREATE CONTROLS
// ==========================================
void VideoPlayerGUI::CreateControls(HWND hwnd) {
    g_hMainWnd = hwnd;
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_BAR_CLASSES;   // dibutuhkan buat trackbar (progress & volume)
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

    // Progress bar pakai trackbar, bukan progress bar biasa, karena perlu bisa di-drag
    g_hProgress = CreateWindowEx(0, TRACKBAR_CLASS, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
        0, 40, 500, 30, hwnd, (HMENU)IDC_PROGRESS, nullptr, nullptr);
    SendMessage(g_hProgress, TBM_SETRANGE, TRUE, MAKELPARAM(0, 1000)); // 0-1000 = persentase*10

    g_hVolume = CreateWindowEx(0, TRACKBAR_CLASS, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
        510, 40, 120, 30, hwnd, (HMENU)IDC_VOLUME, nullptr, nullptr);
    SendMessage(g_hVolume, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
    SendMessage(g_hVolume, TBM_SETPOS, TRUE, 100); // default full volume

    g_hTimeLabel = CreateWindow(L"STATIC", L"00:00 / 00:00", WS_CHILD | WS_VISIBLE,
        0, 75, 200, 20, hwnd, (HMENU)IDC_TIME_LABEL, nullptr, nullptr);

    // Init player, kasih hwnd utama sebagai notify target
    m_player.Initialize(g_hVideoArea, hwnd);

    // Timer buat update progress bar tiap 250ms
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

    if (!RegisterClass(&wc)) {
        return false;
    }

    g_hMainWnd = CreateWindowEx(
        0, CLASS_NAME, L"Video Player UI", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 620, 470,
        NULL, NULL, hInstance, this
    );

    if (!g_hMainWnd) {
        return false;
    }

    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);
    return true;
}

int VideoPlayerGUI::Run() {
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

} // namespace guiVidi
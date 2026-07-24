#ifndef GUI_HH
#define GUI_HH

#include <windows.h>
#include <commctrl.h>
#include <string>

// Define ID kontrol
#define IDC_PLAY         1001
#define IDC_PAUSE        1002
#define IDC_STOP         1003
#define IDC_SKIP_BACK    1004
#define IDC_SKIP_FORWARD 1005
#define IDC_PROGRESS     1006
#define IDC_VOLUME       1007
#define IDC_TIME_LABEL   1008
#define IDC_BACK         1009
#define IDC_FORWARD      1010
#define IDC_SETTINGS     1011
#define IDC_VIDEO_AREA   1012

namespace guiVidi {

class VideoPlayerGUI {
private:
    HWND g_hPlayBtn, g_hPauseBtn, g_hStopBtn;
    HWND g_hSkipBack, g_hSkipForward;
    HWND g_hProgress, g_hVolume, g_hTimeLabel;
    HWND g_hVideoArea;
    HWND g_hToolbar;
    HWND g_hMainWnd;

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

public:
    VideoPlayerGUI() : g_hPlayBtn(NULL), g_hPauseBtn(NULL), g_hStopBtn(NULL),
                       g_hSkipBack(NULL), g_hSkipForward(NULL),
                       g_hProgress(NULL), g_hVolume(NULL), g_hTimeLabel(NULL),
                       g_hVideoArea(NULL), g_hToolbar(NULL), g_hMainWnd(NULL) {}

    bool Initialize(HINSTANCE hInstance, int nCmdShow);
    int Run();
    void CreateControls(HWND hwnd);
};

// Implementasi WindowProc
LRESULT CALLBACK VideoPlayerGUI::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    VideoPlayerGUI* pGUI = nullptr;
    
    if (uMsg == WM_CREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pGUI = reinterpret_cast<VideoPlayerGUI*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pGUI));
        pGUI->g_hMainWnd = hwnd;
        pGUI->CreateControls(hwnd);
    } else {
        pGUI = reinterpret_cast<VideoPlayerGUI*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (pGUI) {
        switch (uMsg) {
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
            case IDC_PLAY:
                MessageBoxW(hwnd, L"▶ Play", L"Info", MB_OK);
                break;
            case IDC_PAUSE:
                MessageBoxW(hwnd, L"⏸ Pause", L"Info", MB_OK);
                break;
            case IDC_STOP:
                MessageBoxW(hwnd, L"⏹ Stop", L"Info", MB_OK);
                break;
            case IDC_SKIP_BACK:
                MessageBoxW(hwnd, L"⏮ Skip Backward", L"Info", MB_OK);
                break;
            case IDC_SKIP_FORWARD:
                MessageBoxW(hwnd, L" Skip Forward", L"Info", MB_OK);
                break;
            case IDC_BACK:
                MessageBoxW(hwnd, L"← Back", L"Info", MB_OK);
                break;
            case IDC_FORWARD:
                MessageBoxW(hwnd, L"→ Forward", L"Info", MB_OK);
                break;
            case IDC_SETTINGS:
                MessageBoxW(hwnd, L"⚙ Settings", L"Info", MB_OK);
                break;
            }
            break;

        case WM_HSCROLL: {
            HWND hCtrl = (HWND)lParam;
            if (hCtrl == pGUI->g_hProgress) {
                int pos = (int)SendMessageW(pGUI->g_hProgress, TBM_GETPOS, 0, 0);
                std::wstring text = L"Seek: " + std::to_wstring(pos) + L"%";
                SetWindowTextW(pGUI->g_hTimeLabel, text.c_str());
            }
            else if (hCtrl == pGUI->g_hVolume) {
                int vol = (int)SendMessageW(pGUI->g_hVolume, TBM_GETPOS, 0, 0);
            }
            break;
        }

        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            if (pGUI->g_hVideoArea) {
                SetWindowPos(pGUI->g_hVideoArea, NULL, 10, 40, width - 20, height - 100, SWP_NOZORDER);
            }
            break;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void VideoPlayerGUI::CreateControls(HWND hwnd) {
    // 1. Buat Toolbar di bagian atas
    g_hToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL,
        WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | CCS_TOP | TBSTYLE_LIST,
        0, 0, 0, 0,
        hwnd, (HMENU)IDC_BACK, GetModuleHandle(NULL), NULL);
    
    TBBUTTON tbb[] = {
        {0, IDC_BACK, TBSTATE_ENABLED, BTNS_BUTTON, {0, 0}, 0, 0},
        {1, IDC_FORWARD, TBSTATE_ENABLED, BTNS_BUTTON, {0, 0}, 0, 0},
        {2, IDC_SETTINGS, TBSTATE_ENABLED, BTNS_BUTTON, {0, 0}, 0, 0}
    };
    
    SendMessage(g_hToolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    SendMessage(g_hToolbar, TB_ADDBUTTONS, 3, (LPARAM)&tbb);
    SendMessage(g_hToolbar, TB_SETBUTTONSIZE, 0, MAKELONG(40, 30));
    
    // 2. Buat area video
    g_hVideoArea = CreateWindow(L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | SS_SUNKEN,
        10, 40, 580, 300,
        hwnd, (HMENU)IDC_VIDEO_AREA, GetModuleHandle(NULL), NULL);
    
    // 3. Buat progress slider
    g_hProgress = CreateWindow(TRACKBAR_CLASS, L"",
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
        10, 350, 400, 30,
        hwnd, (HMENU)IDC_PROGRESS, GetModuleHandle(NULL), NULL);
    SendMessage(g_hProgress, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
    SendMessage(g_hProgress, TBM_SETPOS, TRUE, 0);
    
    // 4. Tombol Skip Backward
    g_hSkipBack = CreateWindow(L"BUTTON", L"<<",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        420, 350, 40, 30,
        hwnd, (HMENU)IDC_SKIP_BACK, GetModuleHandle(NULL), NULL);
    
    // 5. Tombol Play
    g_hPlayBtn = CreateWindow(L"BUTTON", L"▶",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        465, 350, 40, 30,
        hwnd, (HMENU)IDC_PLAY, GetModuleHandle(NULL), NULL);
    
    // 6. Tombol Stop
    g_hStopBtn = CreateWindow(L"BUTTON", L"■",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        510, 350, 40, 30,
        hwnd, (HMENU)IDC_STOP, GetModuleHandle(NULL), NULL);
    
    // 7. Tombol Skip Forward
    g_hSkipForward = CreateWindow(L"BUTTON", L">>",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        555, 350, 40, 30,
        hwnd, (HMENU)IDC_SKIP_FORWARD, GetModuleHandle(NULL), NULL);
    
    // 8. Icon Speaker
    CreateWindow(L"STATIC", L"🔊",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        10, 390, 30, 25,
        hwnd, NULL, GetModuleHandle(NULL), NULL);
    
    // 9. Volume slider
    g_hVolume = CreateWindow(TRACKBAR_CLASS, L"",
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
        45, 385, 150, 30,
        hwnd, (HMENU)IDC_VOLUME, GetModuleHandle(NULL), NULL);
    SendMessage(g_hVolume, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
    SendMessage(g_hVolume, TBM_SETPOS, TRUE, 70);
    
    // 10. Label waktu
    g_hTimeLabel = CreateWindow(L"STATIC", L"00:00 / 00:00",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        200, 390, 150, 20,
        hwnd, (HMENU)IDC_TIME_LABEL, GetModuleHandle(NULL), NULL);
}

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
        0,
        CLASS_NAME,
        L"Video Player UI",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 620, 470,
        NULL,
        NULL,
        hInstance,
        this
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

}

#endif // GUI_HH
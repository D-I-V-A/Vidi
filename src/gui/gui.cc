#include "../../include/gui/gui.hh"
#include "../../include/kernels/ids.hh"
#include <WtsApi32.h>

namespace guiVidi {

bool VideoPlayerGUI::Initialize(HINSTANCE hInstance, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"Vidi-VideoPlayerWindow";
    WNDCLASS wc = {};
    wc.lpfnWndProc = VideoPlayerGUI::WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClass(&wc))
        return false;
    g_hMainWnd = CreateWindowEx(0, CLASS_NAME, L"Vidi-Player", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                                CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, NULL, NULL, hInstance, this);
    if (!g_hMainWnd)
        return false;
    m_hAccel = CreatePlayerAccelTable();

    WTSRegisterSessionNotification(g_hMainWnd, NOTIFY_FOR_THIS_SESSION);

    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);
    return true;
}

HACCEL VideoPlayerGUI::CreatePlayerAccelTable() {
    ACCEL acc[] = {
        {FVIRTKEY | FCONTROL | FNOINVERT, 'O', IDM_FILE_OPEN},  {FVIRTKEY | FNOINVERT, VK_SPACE, IDM_PLAYBACK_PLAY},
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
        if (msg.message == WM_SYSKEYDOWN && msg.wParam == VK_F4 && (GetAsyncKeyState(VK_MENU) & 0x8000)) {
            SendMessage(g_hMainWnd, WM_CLOSE, 0, 0);
            continue;
        }
        if (!TranslateAccelerator(g_hMainWnd, m_hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return (int)msg.wParam;
}

} // namespace guiVidi

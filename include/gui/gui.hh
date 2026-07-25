#ifndef GUI_HH
#define GUI_HH

#include <windows.h>
#include <commctrl.h>

#include "utils.hh"

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
    void CreateMenuBar(HWND hwnd);
    void LayoutControls(int width, int height);
    void CreateControls(HWND hwnd);

public:
    VideoPlayerGUI() : g_hPlayBtn(nullptr), g_hPauseBtn(nullptr), g_hStopBtn(nullptr),
                       g_hSkipBack(nullptr), g_hSkipForward(nullptr),
                       g_hProgress(nullptr), g_hVolume(nullptr), g_hTimeLabel(nullptr),
                       g_hVideoArea(nullptr), g_hToolbar(nullptr), g_hMainWnd(nullptr) {}

    bool Initialize(HINSTANCE hInstance, int nCmdShow);
    int Run();
};

} // namespace guiVidi

#endif // GUI_HH
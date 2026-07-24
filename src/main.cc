#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif


#include <windows.h>
#include <commctrl.h>   // untuk inisialisasi kontrol umum
#include "gui/GUI.hh"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    PWSTR pCmdLine, int nCmdShow) {
    
    // Inisialisasi common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_BAR_CLASSES | ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icex);
    
    // Buat instance GUI
    guiVidi::VideoPlayerGUI gui;
    
    if (!gui.Initialize(hInstance, nCmdShow)) {
        MessageBoxW(NULL, L"Failed to create window!", L"Error", MB_ICONERROR);
        return -1;
    }
    
    // Jalankan message loop
    return gui.Run();
}
#include <windows.h>
#include <combaseapi.h>  // ← WAJIB
#include "../include/gui/gui.hh"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // 1. Initialize COM Library (WAJIB untuk GetOpenFileName)
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) {
        MessageBox(nullptr, L"COM Init Failed", L"Error", MB_ICONERROR);
        return -1;
    }

    guiVidi::VideoPlayerGUI player;
    if (!player.Initialize(hInstance, nCmdShow)) {
        CoUninitialize();
        return -1;
    }

    int result = player.Run();
    
    // 2. Cleanup COM
    CoUninitialize();
    return result;
}
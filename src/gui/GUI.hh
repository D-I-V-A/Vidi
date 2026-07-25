#ifndef GUI_HH
#define GUI_HH

#include <commctrl.h>
#include <string>
#include <windows.h>

#include "constants.hh"

namespace guiVidi {

class VideoPlayerGUI {
private:
  HWND g_hPlayBtn, g_hPauseBtn, g_hStopBtn;
  HWND g_hSkipBack, g_hSkipForward;
  HWND g_hProgress, g_hVolume, g_hTimeLabel;
  HWND g_hVideoArea;
  HWND g_hToolbar;
  HWND g_hMainWnd;

  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                     LPARAM lParam);
  void CreateMenuBar(HWND hwnd);
  void LayoutControls(int width, int height);

public:
  VideoPlayerGUI()
      : g_hPlayBtn(NULL), g_hPauseBtn(NULL), g_hStopBtn(NULL),
        g_hSkipBack(NULL), g_hSkipForward(NULL), g_hProgress(NULL),
        g_hVolume(NULL), g_hTimeLabel(NULL), g_hVideoArea(NULL),
        g_hToolbar(NULL), g_hMainWnd(NULL) {}

  bool Initialize(HINSTANCE hInstance, int nCmdShow);
  int Run();
  void CreateControls(HWND hwnd);
};

// layout
void VideoPlayerGUI::LayoutControls(int width, int height) {
  const int MARGIN = 10;       // jarak tepi window
  const int PROGRESS_H = 25;   // tinggi progress bar
  const int BUTTON_ROW_H = 30; // tinggi baris tombol/volume/label
  const int GAP = 8;           // jarak antar elemen (video->progress->tombol)

  int bottomBarHeight = PROGRESS_H + GAP + BUTTON_ROW_H + MARGIN;

  // Video area: isi semua ruang di atas bottom bar
  int videoHeight =
      height - bottomBarHeight - MARGIN; // dikurangi margin atas juga
  if (g_hVideoArea)
    SetWindowPos(g_hVideoArea, NULL, MARGIN, MARGIN, width - (MARGIN * 2),
                 videoHeight, SWP_NOZORDER);

  int videoBottomY = MARGIN + videoHeight;

  // Progress bar: full width, langsung di bawah video (+ GAP kecil)
  int progressY = videoBottomY + GAP;
  if (g_hProgress)
    SetWindowPos(g_hProgress, NULL, MARGIN, progressY, width - (MARGIN * 2),
                 PROGRESS_H, SWP_NOZORDER);

  // langsung di bawah progress bar
  int buttonY = progressY + PROGRESS_H + GAP;

  if (g_hSkipBack)
    SetWindowPos(g_hSkipBack, NULL, MARGIN, buttonY, 40, BUTTON_ROW_H,
                 SWP_NOZORDER);
  if (g_hPlayBtn)
    SetWindowPos(g_hPlayBtn, NULL, MARGIN + 45, buttonY, 40, BUTTON_ROW_H,
                 SWP_NOZORDER);
  if (g_hStopBtn)
    SetWindowPos(g_hStopBtn, NULL, MARGIN + 90, buttonY, 40, BUTTON_ROW_H,
                 SWP_NOZORDER);
  if (g_hSkipForward)
    SetWindowPos(g_hSkipForward, NULL, MARGIN + 135, buttonY, 40, BUTTON_ROW_H,
                 SWP_NOZORDER);

  if (g_hTimeLabel)
    SetWindowPos(g_hTimeLabel, NULL, width - MARGIN - 150, buttonY, 100,
                 BUTTON_ROW_H, SWP_NOZORDER);
  if (g_hVolume)
    SetWindowPos(g_hVolume, NULL, width - MARGIN - 260, buttonY, 100,
                 BUTTON_ROW_H, SWP_NOZORDER);
}

// ==========================================
// IMPLEMENTASI WINDOW PROC
// ==========================================
LRESULT CALLBACK VideoPlayerGUI::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                            LPARAM lParam) {
  VideoPlayerGUI *pGUI = nullptr;

  if (uMsg == WM_CREATE) {
    CREATESTRUCT *pCreate = reinterpret_cast<CREATESTRUCT *>(lParam);
    pGUI = reinterpret_cast<VideoPlayerGUI *>(pCreate->lpCreateParams);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pGUI));
    pGUI->g_hMainWnd = hwnd;

    pGUI->CreateMenuBar(hwnd);
    pGUI->CreateControls(hwnd);

    RECT rc;
    GetClientRect(hwnd, &rc);
    pGUI->LayoutControls(rc.right - rc.left, rc.bottom - rc.top);
    return 0;
  } else {
    pGUI = reinterpret_cast<VideoPlayerGUI *>(
        GetWindowLongPtr(hwnd, GWLP_USERDATA));
  }

  if (pGUI) {
    switch (uMsg) {
    case WM_COMMAND:
      switch (LOWORD(wParam)) {
      // --- Handler Menu ---
      case IDM_QUIT:
        PostQuitMessage(0);
        break;
      case IDM_ABOUT:
        MessageBoxW(hwnd, L"Video Player Clone v1.0", L"About",
                    MB_OK | MB_ICONINFORMATION);
        break;
      case IDM_OPEN_FILE:
        MessageBoxW(hwnd, L"Fitur Open File akan segera hadir!", L"Media",
                    MB_OK);
        break;

      // --- Handler Toolbar (FIX: sebelumnya tidak ada handler) ---
      case IDC_BACK:
        MessageBoxW(hwnd, L"Toolbar: Back", L"Info", MB_OK);
        break;
      case IDC_FORWARD:
        MessageBoxW(hwnd, L"Toolbar: Forward", L"Info", MB_OK);
        break;
      case IDC_SETTINGS:
        MessageBoxW(hwnd, L"Toolbar: Settings", L"Info", MB_OK);
        break;

      // --- Handler Tombol UI ---
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
        MessageBoxW(hwnd, L"⏭ Skip Forward", L"Info", MB_OK);
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
      break;
    }
    case WM_SIZE: {
      int width = LOWORD(lParam);
      int height = HIWORD(lParam);
      pGUI->LayoutControls(width, height);
      break;
    }
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    }
  }

  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// ==========================================
// IMPLEMENTASI CREATE MENU BAR
// ==========================================
void VideoPlayerGUI::CreateMenuBar(HWND hwnd) {
  HMENU hMenuBar = CreateMenu();
  if (!hMenuBar)
    return;

  HMENU hMenuMedia = CreatePopupMenu();
  AppendMenu(hMenuMedia, MF_STRING, IDM_OPEN_FILE, L"&Open File...\tCtrl+O");
  AppendMenu(hMenuMedia, MF_STRING, IDM_OPEN_FOLDER,
             L"Open &Folder...\tCtrl+F");
  AppendMenu(hMenuMedia, MF_SEPARATOR, 0, NULL);
  AppendMenu(hMenuMedia, MF_STRING, IDM_QUIT, L"&Quit\tCtrl+Q");
  AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hMenuMedia, L"&Media");

  HMENU hMenuPlayback = CreatePopupMenu();
  AppendMenu(hMenuPlayback, MF_STRING, IDM_PLAY, L"&Play\tCtrl+P");
  AppendMenu(hMenuPlayback, MF_STRING, IDM_PAUSE, L"Pa&use\tCtrl+Space");
  AppendMenu(hMenuPlayback, MF_STRING, IDM_STOP, L"&Stop\tCtrl+S");
  AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hMenuPlayback, L"&Playback");

  HMENU hMenuAudio = CreatePopupMenu();
  AppendMenu(hMenuAudio, MF_STRING, IDM_INCREASE_VOL,
             L"&Increase Volume\tCtrl+Up");
  AppendMenu(hMenuAudio, MF_STRING, IDM_DECREASE_VOL,
             L"&Decrease Volume\tCtrl+Down");
  AppendMenu(hMenuAudio, MF_STRING, IDM_MUTE, L"&Mute\tCtrl+M");
  AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hMenuAudio, L"&Audio");

  HMENU hMenuVideo = CreatePopupMenu();
  AppendMenu(hMenuVideo, MF_STRING, IDM_FULLSCREEN, L"&Fullscreen\tF11");
  AppendMenu(hMenuVideo, MF_STRING, IDM_VIDEO_EFFECTS, L"&Video Effects");
  AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hMenuVideo, L"&Video");

  HMENU hMenuSubtitle = CreatePopupMenu();
  AppendMenu(hMenuSubtitle, MF_STRING, IDM_SUB_ADD_FILE,
             L"&Add Subtitle File...");
  AppendMenu(hMenuSubtitle, MF_STRING, IDM_SUB_TRACK, L"&Sub Track");
  AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hMenuSubtitle, L"Sub&title");

  HMENU hMenuTools = CreatePopupMenu();
  AppendMenu(hMenuTools, MF_STRING, IDM_PREFERENCES, L"&Preferences\tCtrl+P");
  AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hMenuTools, L"&Tools");

  HMENU hMenuView = CreatePopupMenu();
  AppendMenu(hMenuView, MF_STRING, IDM_PLAYLIST, L"&Playlist\tCtrl+L");
  AppendMenu(hMenuView, MF_STRING, IDM_ALWAYS_ON_TOP, L"&Always on Top");
  AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hMenuView, L"V&iew");

  HMENU hMenuHelp = CreatePopupMenu();
  AppendMenu(hMenuHelp, MF_STRING, IDM_HELP_WEBSITE, L"&Website");
  AppendMenu(hMenuHelp, MF_SEPARATOR, 0, NULL);
  AppendMenu(hMenuHelp, MF_STRING, IDM_ABOUT, L"&About...");
  AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hMenuHelp, L"&Help");

  SetMenu(hwnd, hMenuBar);
  DrawMenuBar(hwnd);
}

// ==========================================
// IMPLEMENTASI CREATE CONTROLS
// ==========================================
void VideoPlayerGUI::CreateControls(HWND hwnd) {
  // Buat area video
  g_hVideoArea = CreateWindow(
      L"STATIC", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | SS_SUNKEN, 10, 10,
      580, 300, hwnd, (HMENU)IDC_VIDEO_AREA, GetModuleHandle(NULL), NULL);

  // 3. Buat progress slider
  g_hProgress = CreateWindow(
      TRACKBAR_CLASS, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS, 10,
      400, 400, 30, hwnd, (HMENU)IDC_PROGRESS, GetModuleHandle(NULL), NULL);
  SendMessage(g_hProgress, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
  SendMessage(g_hProgress, TBM_SETPOS, TRUE, 0);

  // 4-7. Tombol Kontrol
  g_hSkipBack = CreateWindow(
      L"BUTTON", L"<<", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 420, 400, 40, 30,
      hwnd, (HMENU)IDC_SKIP_BACK, GetModuleHandle(NULL), NULL);
  g_hPlayBtn = CreateWindow(
      L"BUTTON", L"▶", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 465, 400, 40, 30,
      hwnd, (HMENU)IDC_PLAY, GetModuleHandle(NULL), NULL);
  g_hStopBtn = CreateWindow(
      L"BUTTON", L"■", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 510, 400, 40, 30,
      hwnd, (HMENU)IDC_STOP, GetModuleHandle(NULL), NULL);
  g_hSkipForward = CreateWindow(
      L"BUTTON", L">>", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 555, 400, 40, 30,
      hwnd, (HMENU)IDC_SKIP_FORWARD, GetModuleHandle(NULL), NULL);

  // 8. Icon Speaker
  CreateWindow(L"STATIC", L"🔊", WS_CHILD | WS_VISIBLE | SS_CENTER, 10, 440, 30,
               25, hwnd, NULL, GetModuleHandle(NULL), NULL);

  // 9. Volume slider
  g_hVolume = CreateWindow(
      TRACKBAR_CLASS, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS, 45,
      435, 150, 30, hwnd, (HMENU)IDC_VOLUME, GetModuleHandle(NULL), NULL);
  SendMessage(g_hVolume, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
  SendMessage(g_hVolume, TBM_SETPOS, TRUE, 70);

  // 10. Label waktu
  g_hTimeLabel = CreateWindow(
      L"STATIC", L"00:00 / 00:00", WS_CHILD | WS_VISIBLE | SS_CENTER, 200, 440,
      150, 20, hwnd, (HMENU)IDC_TIME_LABEL, GetModuleHandle(NULL), NULL);
}

// ==========================================
// IMPLEMENTASI INITIALIZE & RUN
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

  g_hMainWnd = CreateWindowEx(0, CLASS_NAME, L"Video Player UI",
                              WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                              620, 470, NULL, NULL, hInstance, this);

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

#endif
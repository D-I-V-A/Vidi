#ifndef GUI_HH
#define GUI_HH

#include <windows.h>
#include <commctrl.h>
#include <vsstyle.h>
#include <Uxtheme.h>

#include "../kernels/directShowPlayer.hh"

// [FIX] HAPUS #define DI BAWAH INI agar tidak bentrok dengan static const di dalam class!
// #define COLOR_MODERN_BG RGB(255, 255, 255)
// #define COLOR_MODERN_PRIMARY RGB(0, 120, 212)
// #define COLOR_MODERN_HOVER RGB(240, 240, 240)
// #define COLOR_MODERN_BORDER RGB(200, 200, 200)
// #define COLOR_MODERN_TEXT RGB(50, 50, 50)

namespace guiVidi {

class VideoPlayerGUI {
private:
    HWND g_hPlayBtn, g_hPauseBtn, g_hStopBtn;
    HWND g_hSkipBack, g_hSkipForward;
    HWND g_hProgress, g_hVolume, g_hTimeLabel;
    HWND g_hVideoArea;
    HWND g_hToolbar;
    HWND g_hMainWnd;
    HACCEL m_hAccel;
    HICON m_hIconPlay, m_hIconPause, m_hIconStop, m_hIconSkipBack, m_hIconSkipForward;
    HFONT m_hModernFont;
    HFONT m_hTimeFont;
    HFONT m_hTipFont;
    
    // Warna modern (Sudah benar pakai static const di sini)
    static const COLORREF COLOR_MODERN_BG = RGB(255, 255, 255);
    static const COLORREF COLOR_MODERN_PRIMARY = RGB(0, 120, 212);
    static const COLORREF COLOR_MODERN_TEXT = RGB(50, 50, 50);
    static const COLORREF COLOR_SEEK_TRACK    = RGB(224, 224, 224); // track abu muda
    static const COLORREF COLOR_SEEK_FILL     = RGB(255, 140, 0);   // oranye VLC
    static const COLORREF COLOR_SEEK_FILL_HOT = RGB(255, 170, 51);  // hover/drag
    static const COLORREF COLOR_TIP_BG        = RGB(30, 30, 30);

    kernelPlayerVidi::DirectShowPlayer m_player;
    bool m_isDraggingProgress;
    bool m_isPlaying;
    DWORD m_lastSeekTick;
    bool m_hasPendingSeek;
    double m_pendingSeekTarget;
    DWORD m_pendingSeekStartTick;
    int m_progressRangeMax;
    float m_lastVolume;
    bool m_isMuted;
    double m_cachedDuration;
    WINDOWPLACEMENT m_prevPlacement;

    // VLC-style seekbar state
    HWND m_hTimeTip;      // popup tooltip waktu saat scrubbing
    bool m_seekHot;       // mouse sedang hover di atas bar
    int  m_hotX;          // posisi x cursor relatif kontrol progress

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void CreateMenuBar(HWND hwnd);
    void LayoutControls(int width, int height);
    void CreateControls(HWND hwnd);
    void OnCommand(WPARAM wParam, LPARAM lParam);
    void OnHScroll(WPARAM wParam, LPARAM lParam);
    void OnTimerTick();
    void OpenFileDialog();
    void UpdateTimeLabel(double posSeconds, double durSeconds);
    void SetPlayPauseUI(bool playing);
    void SeekFromTrackbarClick(int mouseX);
    void SetProgressPos(int pos);
    void DrawVlcSeekbar(HDC hdc);
    void DragSeekTo(int x);
    void EndSeekDrag();
    void UpdateSeekFromPos(int pos);
    void ShowTimeTip(double seconds);
    void HideTimeTip();
    void OnMediaReady();
    void ToggleMute();
    void ToggleFullscreen();

    static LRESULT CALLBACK ProgressSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam,
        LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

public:
    VideoPlayerGUI() : g_hPlayBtn(nullptr), g_hPauseBtn(nullptr), g_hStopBtn(nullptr),
                g_hSkipBack(nullptr), g_hSkipForward(nullptr),
                m_hIconPlay(nullptr), m_hIconPause(nullptr), m_hIconStop(nullptr),
                m_hIconSkipBack(nullptr), m_hIconSkipForward(nullptr),
                g_hProgress(nullptr), g_hVolume(nullptr), g_hTimeLabel(nullptr),
                g_hVideoArea(nullptr), g_hToolbar(nullptr), g_hMainWnd(nullptr),
                m_hAccel(nullptr), m_hModernFont(nullptr), m_hTimeFont(nullptr), m_hTipFont(nullptr), // <-- Tambahkan inisialisasi font
                m_isDraggingProgress(false), m_isPlaying(false),
                m_lastSeekTick(0), m_hasPendingSeek(false), m_pendingSeekTarget(0.0),
                m_pendingSeekStartTick(0), m_progressRangeMax(1000),
                m_lastVolume(1.0f), m_isMuted(false), m_cachedDuration(0.0),
                m_hTimeTip(nullptr), m_seekHot(false), m_hotX(0),
                m_prevPlacement{ sizeof(WINDOWPLACEMENT) } {}

    bool Initialize(HINSTANCE hInstance, int nCmdShow);
    int Run();
};

} // namespace guiVidi
#endif // GUI_HH
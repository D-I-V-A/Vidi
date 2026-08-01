#ifndef GUI_HH
#define GUI_HH

#include <windows.h>
#include <commctrl.h>

#include "../kernels/directShowPlayer.hh"

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
    kernelPlayerVidi::DirectShowPlayer m_player;

    bool m_isDraggingProgress;
    bool m_isPlaying;
    DWORD m_lastSeekTick;
    bool m_hasPendingSeek;
    double m_pendingSeekTarget;
    DWORD m_pendingSeekStartTick;
    int m_progressRangeMax;

    // --- TAMBAHIN INI ---
    float m_lastVolume;   // simpan volume sebelum di-mute
    bool  m_isMuted;
    double m_cachedDuration; // cache durasi video, diupdate saat media ready
    WINDOWPLACEMENT m_prevPlacement;
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
    void OnMediaReady();

    // --- TAMBAHIN INI ---
    void ToggleMute();
    void ToggleFullscreen();

    static LRESULT CALLBACK ProgressSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam,
        LPARAM lParam, UINT_PTR uIdSubclass,
        DWORD_PTR dwRefData);

public:
    VideoPlayerGUI() : g_hPlayBtn(nullptr), g_hPauseBtn(nullptr), g_hStopBtn(nullptr),
                g_hSkipBack(nullptr), g_hSkipForward(nullptr),
                g_hProgress(nullptr), g_hVolume(nullptr), g_hTimeLabel(nullptr),
                g_hVideoArea(nullptr), g_hToolbar(nullptr), g_hMainWnd(nullptr),
                m_hAccel(nullptr),
                m_isDraggingProgress(false), m_isPlaying(false),
                m_lastSeekTick(0),
                m_hasPendingSeek(false), m_pendingSeekTarget(0.0),
                m_pendingSeekStartTick(0), m_progressRangeMax(1000),
                m_lastVolume(1.0f), m_isMuted(false),
                m_cachedDuration(0.0),
                m_prevPlacement{ sizeof(WINDOWPLACEMENT) }   // <-- tambahin ini
                {}

    bool Initialize(HINSTANCE hInstance, int nCmdShow);
    int Run();
};

} // namespace guiVidi

#endif // GUI_HH
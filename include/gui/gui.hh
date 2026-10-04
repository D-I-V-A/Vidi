#ifndef GUI_HH
#define GUI_HH

#include "constants.hh"
#include "../kernels/directShowPlayer.hh"

#include <windows.h>
#include <commctrl.h>
#include <vsstyle.h>
#include <Uxtheme.h>
#include <map>

namespace guiVidi {

class VideoPlayerGUI {
  private:
    HWND g_hPlayBtn, g_hStopBtn;
    HWND g_hSkipBack, g_hSkipForward;
    HWND g_hFullscreenBtn, g_hPlaylistBtn, g_hLoopBtn, g_hShuffleBtn;
    HWND g_hProgress, g_hVolume, g_hTimeLabel;
    HWND g_hVolIcon, g_hVolPercent;
    HWND g_hVideoArea;
    HWND g_hToolbar;
    HWND g_hMainWnd;
    HACCEL m_hAccel;
    HMENU m_hMenuBar;
    HICON m_hIconPlay, m_hIconPause, m_hIconStop, m_hIconSkipBack, m_hIconSkipForward;
    HICON m_hIconFullscreen, m_hIconPlaylist, m_hIconLoop, m_hIconShuffle, m_hIconSpeaker;
    HFONT m_hModernFont;
    HFONT m_hTimeFont;
    HFONT m_hTipFont;

    kernelPlayerVidi::DirectShowPlayer m_player;
    bool m_isDraggingProgress;
    bool m_isPlaying;
    DWORD m_lastSeekTick;
    DWORD m_lastDurCheckTick;
    DWORD m_subtitleSeekUntilTick;
    bool m_hasPendingSeek;
    double m_pendingSeekTarget;
    DWORD m_pendingSeekStartTick;
    int m_progressRangeMax;
    float m_lastVolume;
    bool m_isMuted;
    double m_cachedDuration;
    WINDOWPLACEMENT m_prevPlacement;
    bool m_isFullscreen;
    bool m_transitionDark;
    bool m_cursorHidden;
    bool m_wasMinimized;
    POINT m_lastCursor;
    HWND m_hTimeTip;
    HWND m_hFsOverlay;
    bool m_seekHot;
    int m_hotX;

    bool m_volHot;
    bool m_volDrag;
    int m_volHotX;

    bool m_isLooping;
    bool m_isShuffle;

    DWORD m_lastVideoClickTick;
    short m_lastVideoClickX, m_lastVideoClickY;

    HWND m_hSubOverlay[MAX_SUB_OVERLAYS];
    HBITMAP m_hSubBmp[2];
    void* m_pSubBmpBits[2];
    int m_subBmpW[2] = {};
    int m_subBmpH[2] = {};
    HFONT m_hSubFont;
    std::map<int, HFONT> m_subFontCache;
    bool m_appActivate = true;
    bool m_subsHidden;
    bool m_isClosing;
    int m_lastUsedOverlays = 0;
    uint32_t m_lastMediaReadyGen = 0;
    size_t m_lastSubContentHash = 0;
    int m_lastSubFrameW = 0;
    int m_lastSubFrameH = 0;
    int m_lastSubOverlayX = 0;
    int m_lastSubOverlayY = 0;
    DWORD m_lastSubRenderTick = 0;
    double m_lastSubPosition = 0.0;
    bool m_subNeedsUpdate = false;
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
    void DrawVlcVolumeBar(HDC hdc);
    void ApplyVolumeFromSlider(int sliderPos);
    void ShowVolTip(int sliderPos);
    void DragSeekTo(int x);
    void EndSeekDrag();
    void UpdateSeekFromPos(int pos);
    void ShowTimeTip(double seconds);
    void HideTimeTip();
    void OnMediaReady();
    void ToggleMute();
    void EnterFullscreen();
    void ExitFullscreen();
    void FitWindowToVideo();
    void ShowOSControls(bool visible);
    void PokeOSControls();
    bool CursorOverControls();
    bool CursorOverFsOverlay();
    void CreateFsOverlay();
    void DestroyFsOverlay();
    void LayoutFsOverlay(int screenW, int screenH);
    void ShowFsOverlay(bool visible);
    static LRESULT CALLBACK FsOverlayWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void RecoverVideo();
    void LayoutFullscreen(int width, int height);
    void CreateSubtitleOverlay(HWND hwnd);
    void UpdateSubtitleDisplays(double posSeconds, bool force = false);
    void BeginSubtitleSeekDelay();
    void BeginSubtitleDelay();
    void HideAllSubOverlays();
    HACCEL CreatePlayerAccelTable();
    void UpdateVolumePercent(int pos);
    void SetToggleBtnState(HWND btn, bool active);
    static LRESULT CALLBACK ProgressSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                                 UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
    static LRESULT CALLBACK VolumeSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass,
                                               DWORD_PTR dwRefData);
    static LRESULT CALLBACK VideoAreaSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                                  UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

  public:
    VideoPlayerGUI()
        : g_hPlayBtn(nullptr),
          g_hStopBtn(nullptr),
          g_hSkipBack(nullptr),
          g_hSkipForward(nullptr),
          g_hFullscreenBtn(nullptr),
          g_hPlaylistBtn(nullptr),
          g_hLoopBtn(nullptr),
          g_hShuffleBtn(nullptr),
          g_hVolIcon(nullptr),
          g_hVolPercent(nullptr),
          m_hIconPlay(nullptr),
          m_hIconPause(nullptr),
          m_hIconStop(nullptr),
          m_hIconSkipBack(nullptr),
          m_hIconSkipForward(nullptr),
          m_hIconFullscreen(nullptr),
          m_hIconPlaylist(nullptr),
          m_hIconLoop(nullptr),
          m_hIconShuffle(nullptr),
          m_hIconSpeaker(nullptr),
          g_hProgress(nullptr),
          g_hVolume(nullptr),
          g_hTimeLabel(nullptr),
          g_hVideoArea(nullptr),
          g_hToolbar(nullptr),
          g_hMainWnd(nullptr),
          m_hAccel(nullptr),
          m_hMenuBar(nullptr),
          m_hModernFont(nullptr),
          m_hTimeFont(nullptr),
          m_hTipFont(nullptr),
          m_isDraggingProgress(false),
          m_isPlaying(false),
          m_lastSeekTick(0),
          m_lastDurCheckTick(0),
          m_subtitleSeekUntilTick(0),
          m_hasPendingSeek(false),
          m_pendingSeekTarget(0.0),
          m_pendingSeekStartTick(0),
          m_progressRangeMax(1000),
          m_lastVolume(1.0f),
          m_isMuted(false),
          m_cachedDuration(0.0),
          m_hTimeTip(nullptr),
          m_hFsOverlay(nullptr),
          m_seekHot(false),
          m_hotX(0),
          m_volHot(false),
          m_volDrag(false),
          m_volHotX(0),
          m_isLooping(false),
          m_isShuffle(false),
          m_isFullscreen(false),
          m_transitionDark(false),
          m_cursorHidden(false),
          m_wasMinimized(false),
          m_lastCursor{-1, -1},
          m_lastVideoClickTick(0),
          m_lastVideoClickX(0),
          m_lastVideoClickY(0),
          m_hSubFont(nullptr),
          m_hSubBmp{nullptr, nullptr},
          m_pSubBmpBits{nullptr, nullptr},
          m_subsHidden(false),
          m_isClosing(false),
          m_prevPlacement{sizeof(WINDOWPLACEMENT)} {
        for (int i = 0; i < MAX_SUB_OVERLAYS; ++i)
            m_hSubOverlay[i] = nullptr;
    }

    bool Initialize(HINSTANCE hInstance, int nCmdShow);
    int Run();
};

} // namespace guiVidi
#endif // GUI_HH
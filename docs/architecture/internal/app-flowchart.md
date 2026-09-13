# Flowchart Internal Aplikasi

Dokumentasi ini menjelaskan alur eksekusi internal aplikasi Vidi secara detail untuk developer.

---

## 1. Alur Eksekusi dari WinMain hingga Destroy

```
┌─────────────────────────────────────────────────────────────────┐
│              ALUR EKSEKUSI LENGKAP APLIKASI                     │
└─────────────────────────────────────────────────────────────────┘

WinMain()
    │
    ├─► CoInitializeEx(COINIT_APARTMENTTHREADED)
    │       │
    │       ├─► FAILED? → MessageBox("COM Init Failed") → return -1
    │       │
    │       └─► SUCCESS
    │
    ├─► VideoPlayerGUI player
    │
    ├─► player.Initialize(hInstance, nCmdShow)
    │       │
    │       ├─► RegisterClassEx() → Window Class
    │       │
    │       ├─► CreateWindowEx() → Main Window (HWND)
    │       │
    │       ├─► CreateMenuBar() → Menu System
    │       │
    │       ├─► CreateControls() → Buttons, Trackbar, Labels
    │       │
    │       ├─► LayoutControls() → Position Controls
    │       │
    │       ├─► Initialize DirectShow Player
    │       │       │
    │       │       └─► m_player.Initialize(hVideoWnd, hNotifyWnd)
    │       │
    │       ├─► SetTimer(TIMER_ID, 50ms)
    │       │
    │       └─► return TRUE/FALSE
    │
    ├─► player.Run()
    │       │
    │       ├─► MESSAGE LOOP:
    │       │       │
    │       │       ├─► PeekMessage()
    │       │       │       │
    │       │       │       ├─► Message Available
    │       │       │       │       │
    │       │       │       │       ├─► TranslateAccelerator()
    │       │       │       │       │       │
    │       │       │       │       │       ├─► Accel Match → OnCommand()
    │       │       │       │       │       │
    │       │       │       │       │       └─► No Match → TranslateMessage()
    │       │       │       │       │               │
    │       │       │       │       │               └─► DispatchMessage()
    │       │       │       │       │                       │
    │       │       │       │       │                       └─► WindowProc()
    │       │       │       │       │
    │       │       │       │       └─► Check Quit Message
    │       │       │       │
    │       │       │       └─► No Message
    │       │       │               │
    │       │       │               └─► OnTimerTick()
    │       │       │                       │
    │       │       │                       ├─► Update Progress Bar
    │       │       │                       ├─► Update Time Label
    │       │       │                       ├─► Update Subtitle Display
    │       │       │                       └─► Handle Pending Seek
    │       │       │
    │       │       └─► Loop Back to PeekMessage()
    │       │
    │       └─► return result
    │
    └─► CoUninitialize()
        return result
```

---

## 2. Alur Window Message Handling

```
┌─────────────────────────────────────────────────────────────────┐
│               WINDOW MESSAGE HANDLING FLOW                      │
└─────────────────────────────────────────────────────────────────┘

WindowProc(hwnd, uMsg, wParam, lParam)
    │
    ├─► WM_CREATE
    │       │
    │       └─► return 0
    │
    ├─► WM_COMMAND
    │       │
    │       ├─► LOWORD(wParam) = Command ID
    │       │
    │       ├─► IDM_FILE_OPEN → OpenFileDialog()
    │       │       │
    │       │       ├─► GetOpenFileName()
    │       │       │       │
    │       │       │       ├─► File Selected → OpenFile(path)
    │       │       │       │
    │       │       │       └─► Cancel → Return
    │       │       │
    │       │       └─► Update Title Bar
    │       │
    │       ├─► IDM_PLAY_PAUSE → TogglePlayPause()
    │       │       │
    │       │       ├─► Is Playing? → Pause()
    │       │       │
    │       │       └─► Is Paused/Stopped? → Play()
    │       │
    │       ├─► IDM_STOP → Stop()
    │       │       │
    │       │       ├─► m_player.Stop()
    │       │       ├─► SetPlayPauseUI(false)
    │       │       └─► Reset Progress Bar
    │       │
    │       ├─► IDM_SEEK_FORWARD → Seek(+10 seconds)
    │       │
    │       ├─► IDM_SEEK_BACKWARD → Seek(-10 seconds)
    │       │
    │       ├─► IDM_VOLUME_UP → Volume(+5%)
    │       │
    │       ├─► IDM_VOLUME_DOWN → Volume(-5%)
    │       │
    │       ├─► IDM_TOGGLE_MUTE → ToggleMute()
    │       │
    │       ├─► IDM_TOGGLE_FULLSCREEN → ToggleFullscreen()
    │       │
    │       └─► Default → DefWindowProc()
    │
    ├─► WM_HSCROLL
    │       │
    │       ├─► Progress Bar → HandleSeek()
    │       │       │
    │       │       ├─► SB_THUMBTRACK → DragSeekTo()
    │       │       │
    │       │       ├─► SB_THUMBPOSITION → EndSeekDrag()
    │       │       │
    │       │       └─► SB_LINELEFT/RIGHT → Seek(±1 second)
    │       │
    │       └─► Volume Bar → HandleVolume()
    │               │
    │               └─► ApplyVolumeFromSlider()
    │
    ├─► WM_VSCROLL
    │       │
    │       └─► Volume Bar → HandleVolume()
    │
    ├─► WM_TIMER
    │       │
    │       ├─► TIMER_ID → OnTimerTick()
    │       │       │
    │       │       ├─► UpdateProgress()
    │       │       │       │
    │       │       │       ├─► Get Position & Duration
    │       │       │       ├─► Calculate Progress %
    │       │       │       └─► Update Progress Bar
    │       │       │
    │       │       ├─► UpdateTimeLabel()
    │       │       │
    │       │       └─► UpdateSubtitleDisplays()
    │       │               │
    │       │               ├─► Get Active Subtitles
    │       │               ├─► Render ASS Frame
    │       │               └─► Draw to Overlay
    │       │
    │       └─► TIMER_FULLSCREEN → HandleFullscreenCursor()
    │               │
    │               └─► HideCursor() / ShowCursor()
    │
    ├─► WM_PAINT
    │       │
    │       ├─► BeginPaint()
    │       │
    │       ├─► DrawVlcSeekbar(hdc)
    │       │
    │       ├─► DrawVlcVolumeBar(hdc)
    │       │
    │       └─► EndPaint()
    │
    ├─► WM_SIZE
    │       │
    │       └─► LayoutControls(LOWORD(lParam), HIWORD(lParam))
    │               │
    │               ├─► Position Video Area
    │               ├─► Position Controls
    │               └─► Resize Subtitle Overlay
    │
    ├─► WM_KEYDOWN
    │       │
    │       ├─► VK_SPACE → TogglePlayPause()
    │       ├─► VK_LEFT → Seek(-10 seconds)
    │       ├─► VK_RIGHT → Seek(+10 seconds)
    │       ├─► VK_UP → Volume(+5%)
    │       ├─► VK_DOWN → Volume(-5%)
    │       ├─► VK_F → ToggleFullscreen()
    │       ├─► VK_M → ToggleMute()
    │       └─► VK_S → Stop()
    │
    ├─► WM_LBUTTONDOWN
    │       │
    │       ├─► Click on Video Area → TogglePlayPause()
    │       │
    │       ├─► Click on Progress Bar → StartSeekDrag()
    │       │
    │       └─► Click on Volume Bar → StartVolumeDrag()
    │
    ├─► WM_LBUTTONUP
    │       │
    │       ├─► EndSeekDrag()
    │       │
    │       └─► EndVolumeDrag()
    │
    ├─► WM_MOUSEMOVE
    │       │
    │       ├─► IsDraggingProgress → DragSeekTo()
    │       │
    │       ├─► IsDraggingVolume → DragVolumeTo()
    │       │
    │       └─► ShowControls() (if hidden)
    │
    ├─► WM_MOUSEWHEEL
    │       │
    │       └─► Volume(±10%)
    │
    ├─► WM_CLOSE
    │       │
    │       ├─► ConfirmClose()
    │       │
    │       └─► DestroyWindow()
    │
    ├─► WM_DESTROY
    │       │
    │       ├─► m_player.Shutdown()
    │       │
    │       ├─► KillTimer()
    │       │
    │       └─► PostQuitMessage(0)
    │
    └─► Default → DefWindowProc()
```

---

## 3. Alur Open File

```
┌─────────────────────────────────────────────────────────────────┐
│                    OPEN FILE FLOW                               │
└─────────────────────────────────────────────────────────────────┘

OpenFileDialog()
    │
    ├─► ZeroMemory(&ofn, sizeof(ofn))
    │
    ├─► ofn.lpstrFilter = "Video Files\0*.mp4;*.mkv;*.avi\0..."
    │
    ├─► ofn.lpstrFile = szFile (MAX_PATH buffer)
    │
    ├─► ofn.nMaxFile = MAX_PATH
    │
    ├─► ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST
    │
    ├─► GetOpenFileName(&ofn)
    │       │
    │       ├─► Return TRUE
    │       │       │
    │       │       └─► OpenFile(szFile)
    │       │
    │       └─► Return FALSE
    │               │
    │               └─► Return (Cancel)
    │
    └─► End

OpenFile(path)
    │
    ├─► m_player.Stop()
    │
    ├─► m_player.OpenFile(path)
    │       │
    │       ├─► CreateGraph()
    │       │       │
    │       │       ├─► CoCreateInstance(CLSID_FilterGraph)
    │       │       │
    │       │       ├─► AddSourceFilter(path)
    │       │       │       │
    │       │       │       └─► IGraphBuilder::AddSourceFilter()
    │       │       │
    │       │       ├─► LoadUnregisteredFilter(LAVSplitter.dll)
    │       │       │       │
    │       │       │       ├─► LoadLibraryW()
    │       │       │       ├─► GetProcAddress("DllGetClassObject")
    │       │       │       └─► CreateInstance(IBaseFilter)
    │       │       │
    │       │       ├─► AddFilter(LAVSplitter)
    │       │       │
    │       │       ├─► ConnectFilters(Source → LAVSplitter)
    │       │       │       │
    │       │       │       ├─► FindPin(Source, OUTPUT)
    │       │       │       ├─► FindPin(LAVSplitter, INPUT)
    │       │       │       └─► IPin::Connect()
    │       │       │
    │       │       ├─► LoadUnregisteredFilter(LAVVideo.dll)
    │       │       ├─► AddFilter(LAVVideo)
    │       │       │
    │       │       ├─► LoadUnregisteredFilter(LAVAudio.dll)
    │       │       ├─► AddFilter(LAVAudio)
    │       │       │
    │       │       ├─► AddVideoRenderer()
    │       │       │       │
    │       │       │       ├─► CoCreateInstance(CLSID_VideoMixingRenderer)
    │       │       │       ├─► SetVideoWindow(hVideoWnd)
    │       │       │       └─► AddFilter(VMR-7)
    │       │       │
    │       │       ├─► ConnectFilters(LAVSplitter → LAVVideo → VMR-7)
    │       │       │
    │       │       ├─► ConnectFilters(LAVSplitter → LAVAudio → AudioRenderer)
    │       │       │
    │       │       └─► RenderSubtitles()
    │       │               │
    │       │               ├─► m_subReader.LoadFromFile(path)
    │       │               │
    │       │               └─► CreateSubtitleOverlay()
    │       │
    │       └─► return TRUE/FALSE
    │
    ├─► OnMediaReady()
    │       │
    │       ├─► UpdateVideoSize()
    │       │
    │       ├─► FitWindowToVideo()
    │       │
    │       ├─► SetPlayPauseUI(false)
    │       │
    │       └─► UpdateTitleBar(path)
    │
    └─► Play()
```

---

## 4. Alur Playback Control

```
┌─────────────────────────────────────────────────────────────────┐
│                 PLAYBACK CONTROL FLOW                           │
└─────────────────────────────────────────────────────────────────┘

Play()
    │
    ├─► m_player.Play()
    │       │
    │       ├─► IMediaControl::Run()
    │       │
    │       └─► m_isPlaying = true
    │
    ├─► SetPlayPauseUI(true)
    │       │
    │       ├─► SendMessage(g_hPlayBtn, BM_SETIMAGE, ...)
    │       │       └─► Set Pause Icon
    │       │
    │       └─► UpdateTooltip("Pause")
    │
    └─► Start Subtitle Timer

Pause()
    │
    ├─► m_player.Pause()
    │       │
    │       ├─► IMediaControl::Pause()
    │       │
    │       └─► m_isPlaying = false
    │
    └─► SetPlayPauseUI(false)
            │
            ├─► Set Play Icon
            └─► UpdateTooltip("Play")

Stop()
    │
    ├─► m_player.Stop()
    │       │
    │       ├─► IMediaControl::Stop()
    │       │
    │       ├─► IMediaSeeking::SetPosition(0)
    │       │
    │       └─► m_isPlaying = false
    │
    ├─► SetPlayPauseUI(false)
    │
    ├─► SetProgressPos(0)
    │
    ├─► UpdateTimeLabel(0, duration)
    │
    └─► HideAllSubOverlays()

Seek(seconds)
    │
    ├─► Validate Position
    │       │
    │       ├─► Clamp(0, duration)
    │       │
    │       └─► Round to Nearest Frame
    │
    ├─► m_player.Seek(seconds)
    │       │
    │       ├─► Convert to Reference Time
    │       │       │
    │       │       └─► llSeekPos = (LONGLONG)(seconds * 10000000)
    │       │
    │       ├─► IMediaSeeking::SetPosition(llSeekPos)
    │       │
    │       └─► ForceFrameRefresh()
    │
    ├─► SetProgressPos(progress)
    │
    ├─► UpdateTimeLabel(seconds, duration)
    │
    └─► UpdateSubtitleDisplays(seconds)
```

---

## 5. Alur Timer Tick

```
┌─────────────────────────────────────────────────────────────────┐
│                     TIMER TICK FLOW                             │
└─────────────────────────────────────────────────────────────────┘

OnTimerTick() (every 50ms)
    │
    ├─► UpdateProgress()
    │       │
    │       ├─► Get Current Position
    │       │       │
    │       │       └─► m_player.GetPosition()
    │       │
    │       ├─► Get Duration
    │       │       │
    │       │       └─► m_player.GetDuration()
    │       │
    │       ├─► Calculate Progress %
    │       │       │
    │       │       └─► progress = (position / duration) * 1000
    │       │
    │       ├─► Update Progress Bar
    │       │       │
    │       │       └─► SendMessage(g_hProgress, TBM_SETPOS, ...)
    │       │
    │       └─► Update Time Label
    │               │
    │               └─► UpdateTimeLabel(position, duration)
    │
    ├─► HandlePendingSeek()
    │       │
    │       ├─► Has Pending Seek?
    │       │       │
    │       │       ├─► YES → Check Debounce
    │       │       │       │
    │       │       │       ├─► Debounce Expired → Execute Seek
    │       │       │       │
    │       │       │       └─► Still Debouncing → Wait
    │       │       │
    │       │       └─► NO → Skip
    │       │
    │       └─► Execute Pending Seek
    │               │
    │               └─► Seek(m_pendingSeekTarget)
    │
    ├─► UpdateSubtitleDisplays(position)
    │       │
    │       ├─► Subtitles Loaded?
    │       │       │
    │       │       ├─► YES → Get Active Subtitles
    │       │       │       │
    │       │       │       ├─► Filter by Time
    │       │       │       │
    │       │       │       └─► Return matching SubtitleEntry
    │       │       │
    │       │       └─► NO → Hide All Overlays
    │       │
    │       ├─► Active Subs Found?
    │       │       │
    │       │       ├─► YES → Render Subtitle
    │       │       │       │
    │       │       │       ├─► assRenderer.RenderFrame()
    │       │       │       │
    │       │       │       ├─► Get ASS_Image List
    │       │       │       │
    │       │       │       └─► Draw to Overlay Bitmap
    │       │       │
    │       │       └─► NO → Hide All Overlays
    │       │
    │       └─► Update Overlay Window
    │               │
    │               └─► InvalidateRect(hSubOverlay, ...)
    │
    └─► Check Media Events
            │
            ├─► EC_COMPLETE → HandlePlaybackEnd()
            │       │
            │       ├─► Is Looping? → Seek(0), Play()
            │       │
            │       └─► Stop()
            │
            └─► Other Events → HandleGraphEvent()
```

---

## 6. Alur Fullscreen

```
┌─────────────────────────────────────────────────────────────────┐
│                 FULLSCREEN FLOW                                 │
└─────────────────────────────────────────────────────────────────┘

ToggleFullscreen()
    │
    ├─► Is Fullscreen?
    │       │
    │       ├─► YES → ExitFullscreen()
    │       │
    │       └─► NO → EnterFullscreen()
    │
    └─► End

EnterFullscreen()
    │
    ├─► Save Current Window Placement
    │       │
    │       └─► GetWindowPlacement(m_hMainWnd, &m_prevPlacement)
    │
    ├─► Remove Window Styles
    │       │
    │       ├─► Remove WS_OVERLAPPEDWINDOW
    │       │
    │       └─► Add WS_POPUP | WS_MAXIMIZE
    │
    ├─► Set Fullscreen Rect
    │       │
    │       └─► GetWindowRect(GetDesktopWindow(), &rcDesktop)
    │
    ├─► SetWindowPos(HWND_TOP, rcDesktop)
    │
    ├─► Hide Controls
    │       │
    │       ├─► ShowWindow(g_hToolbar, SW_HIDE)
    │       ├─► ShowWindow(g_hProgress, SW_HIDE)
    │       ├─► ShowWindow(g_hVolume, SW_HIDE)
    │       └─► ShowWindow(g_hTimeLabel, SW_HIDE)
    │
    ├─► Fit Video to Screen
    │       │
    │       └─► FitWindowToVideo()
    │
    ├─► Start Fullscreen Timer
    │       │
    │       └─► SetTimer(TIMER_FULLSCREEN, 3000ms)
    │
    └─► m_isFullscreen = true

ExitFullscreen()
    │
    ├─► Restore Window Styles
    │       │
    │       ├─► Remove WS_POPUP | WS_MAXIMIZE
    │       │
    │       └─► Add WS_OVERLAPPEDWINDOW
    │
    ├─► Restore Window Placement
    │       │
    │       └─► SetWindowPlacement(m_hMainWnd, &m_prevPlacement)
    │
    ├─► Show Controls
    │       │
    │       ├─► ShowWindow(g_hToolbar, SW_SHOW)
    │       ├─► ShowWindow(g_hProgress, SW_SHOW)
    │       ├─► ShowWindow(g_hVolume, SW_SHOW)
    │       └─► ShowWindow(g_hTimeLabel, SW_SHOW)
    │
    ├─► Kill Timer
    │       │
    │       └─► KillTimer(TIMER_FULLSCREEN)
    │
    ├─► Show Cursor
    │       │
    │       └─► ShowCursor(TRUE)
    │
    └─► m_isFullscreen = false

HandleFullscreenCursor()
    │
    ├─► Get Cursor Position
    │       │
    │       └─► GetCursorPos(&pt)
    │
    ├─► Over Controls Area?
    │       │
    │       ├─► YES → ShowCursor(TRUE), Reset Timer
    │       │
    │       └─► NO → HideCursor(FALSE)
    │
    └─► Update Last Cursor Position
```

---

## 7. Alur Subtitle Rendering

```
┌─────────────────────────────────────────────────────────────────┐
│              SUBTITLE RENDERING FLOW                            │
└─────────────────────────────────────────────────────────────────┘

SubtitleLoadThreadProc(lpParam)
    │
    ├─► LoadFFmpegDLLs()
    │       │
    │       ├─► LoadLibrary("avformat-59.dll")
    │       ├─► LoadLibrary("avcodec-59.dll")
    │       ├─► LoadLibrary("avutil-57.dll")
    │       │
    │       └─► GetProcAddress() for each function
    │
    ├─► avformat_alloc_context()
    │
    ├─► avformat_open_input(path)
    │       │
    │       ├─► FAILED → Return false
    │       │
    │       └─► SUCCESS
    │
    ├─► avformat_find_stream_info()
    │
    ├─► Find Subtitle Stream
    │       │
    │       ├─► Loop through streams
    │       │
    │       ├─► Check codec_type == AVMEDIA_TYPE_SUBTITLE
    │       │
    │       └─► Return stream index
    │
    ├─► avcodec_find_decoder()
    │
    ├─► avcodec_alloc_context3()
    │
    ├─► avcodec_open2()
    │
    ├─► Read Subtitle Packets
    │       │
    │       ├─► while (av_read_frame() >= 0)
    │       │       │
    │       │       ├─► Is Subtitle Packet?
    │       │       │       │
    │       │       │       ├─► YES → avcodec_decode_subtitle2()
    │       │       │       │       │
    │       │       │       │       ├─► Parse SRT/ASS Format
    │       │       │       │       │
    │       │       │       │       └─► Add to m_subtitles
    │       │       │       │
    │       │       │       └─► NO → Skip
    │       │       │
    │       │       └─► Check m_stopLoading
    │       │               │
    │       │               ├─► TRUE → Break
    │       │               │
    │       │               └─► FALSE → Continue
    │       │
    │       └─► End of File
    │
    ├─► avformat_close_input()
    │
    ├─► FreeFFmpegDLLs()
    │
    └─► m_isLoaded = true

RenderSubtitleEntry(entry)
    │
    ├─► Convert to ASS Format
    │       │
    │       └─► Format: "Dialogue: 0,..."
    │
    ├─► assRenderer.LoadTrack(assData)
    │       │
    │       ├─► ass_new_library()
    │       ├─► ass_new_renderer()
    │       ├─► ass_read_memory()
    │       │
    │       └─► Return track
    │
    ├─► assRenderer.RenderFrame(timestamp)
    │       │
    │       ├─► ass_render_frame()
    │       │
    │       └─► Return ASS_Image list
    │
    ├─► Draw to Bitmap
    │       │
    │       ├─► Create DIB Section
    │       │
    │       ├─► For each ASS_Image:
    │       │       │
    │       │       ├─► Get bitmap data
    │       │       │
    │       │       ├─► Apply color/alpha
    │       │       │
    │       │       └─► Draw to overlay bitmap
    │       │
    │       └─► Update overlay window
    │
    └─► InvalidateRect(hSubOverlay)
```

---

## 8. Alur Error Handling

```
┌─────────────────────────────────────────────────────────────────┐
│                 ERROR HANDLING FLOW                             │
└─────────────────────────────────────────────────────────────────┘

Error Occurs
    │
    ├─► Log Error
    │       │
    │       ├─► OutputDebugString()
    │       │
    │       └─► (Optional) Write to Log File
    │
    ├─► Check Error Type
    │       │
    │       ├─► COM Error
    │       │       │
    │       │       ├─► HRESULT Code
    │       │       │
    │       │       └─► FormatMessage()
    │       │
    │       ├─► File Not Found
    │       │       │
    │       │       └─► Show "File not found" MessageBox
    │       │
    │       ├─► Codec Not Supported
    │       │       │
    │       │       └─► Show "Codec not supported" MessageBox
    │       │
    │       └─► Generic Error
    │               │
    │               └─► Show "Error" MessageBox
    │
    ├─► Is Recoverable?
    │       │
    │       ├─► YES
    │       │       │
    │       │       ├─► Try Recovery
    │       │       │       │
    │       │       │       ├─► Retry Operation
    │       │       │       │
    │       │       │       ├─► Fallback to Alternative
    │       │       │       │       │
    │       │       │       │       ├─► DirectShow → Media Foundation
    │       │       │       │       │
    │       │       │       │       └─► LAV Filters → Default Filters
    │       │       │       │
    │       │       │       └─► Skip Operation
    │       │       │
    │       │       ├─► Recovery Success?
    │       │       │       │
    │       │       │       ├─► YES → Continue
    │       │       │       │
    │       │       │       └─► NO → Show Error, Continue
    │       │       │
    │       │       └─► Log Recovery Attempt
    │       │
    │       └─► NO
    │               │
    │               ├─► Show Fatal Error
    │               │       │
    │               │       └─► MessageBox(MB_ICONERROR)
    │               │
    │               ├─► Cleanup Resources
    │               │       │
    │               │       ├─► m_player.Shutdown()
    │               │       │
    │               │       ├─► CoUninitialize()
    │               │       │
    │               │       └─► Free Libraries
    │               │
    │               └─► Exit Application
    │                       │
    │                       └─► return -1
    │
    └─► Continue Execution
```

---

## Ringkasan Alur Internal

| Alur | Entry Point | Output |
|---|---|---|
| Startup | `WinMain()` | Application Running |
| Window Init | `Initialize()` | Window Created |
| Message Loop | `Run()` | Events Handled |
| Open File | `OpenFileDialog()` → `OpenFile()` | Video Loaded |
| Playback | `Play()`, `Pause()`, `Stop()` | Media Control |
| Timer | `OnTimerTick()` | UI Updated |
| Subtitle | `SubtitleLoadThreadProc()` | Subtitles Rendered |
| Fullscreen | `EnterFullscreen()`/`ExitFullscreen()` | Display Mode |
| Error | Error callbacks | Recovery or Exit |

---

## Referensi

- [Window Messages](https://learn.microsoft.com/en-us/windows/win32/winmsg/about-messages-and-message-queues)
- [DirectShow Filter Graph](https://learn.microsoft.com/en-us/windows/win32/directshow/filter-graphs)
- [COM Programming](https://learn.microsoft.com/en-us/windows/win32/com/com-programming-basics)

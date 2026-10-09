# Arsitektur Khusus Proyek

Dokumentasi ini menjelaskan arsitektur internal proyek Vidi secara detail untuk developer yang bekerja pada proyek ini.

---

## 1. Struktur Direktori

```
D:\Project Pribadi\Vidi\
│
├── CMakeLists.txt              # Konfigurasi build utama
├── CMakePresets.json            # Preset untuk vcpkg integration
├── vcpkg.json                   # Dependencies (libass)
├── README.md                    # Dokumentasi proyek
├── LICENSE                      # MIT License
├── .clang-format                # Code formatting rules
├── .clang-tidy                  # Static analysis configuration
├── .gitignore                   # Git ignore rules
│
├── src/                         # SOURCE FILES
│   ├── main.cc                  # Entry point (WinMain)
│   ├── app.rc                   # Windows resource file
│   ├── app.manifest             # DPI awareness manifest
│   │
│   ├── gui/                     # GUI Layer (namespace: guiVidi)
│   │   ├── gui.cc               # Window init & main loop
│   │   ├── gui_windowproc.cc    # Window message handling
│   │   ├── gui_layout.cc        # Control positioning
│   │   ├── gui_controls.cc      # Button & trackbar creation
│   │   ├── gui_progressbar.cc   # VLC-style seekbar
│   │   ├── gui_volumebar.cc     # Volume slider
│   │   ├── gui_fullscreen.cc    # Fullscreen toggle
│   │   └── gui_subtitle.cc      # Subtitle overlay
│   │
│   └── kernels/                 # Kernel Layer (namespace: kernelPlayerVidi)
│       ├── directShowPlayer.cc  # DirectShow backend
│       ├── subtitleReader.cc    # FFmpeg subtitle parsing
│       └── assRenderer.cc       # libass rendering
│
├── include/                     # HEADER FILES
│   ├── gui/
│   │   ├── gui.hh               # VideoPlayerGUI class
│   │   └── constants.hh         # Colors, dimensions, utilities
│   │
│   └── kernels/
│       ├── directShowPlayer.hh  # DirectShowPlayer class
│       ├── subtitleReader.hh    # SubtitleReader class
│       ├── assRenderer.hh       # AssRenderer class
│       ├── ffmpeg_dynload.hh    # FFmpeg dynamic loading
│       ├── sg_min.hh            # Sample Grabber COM interface
│       └── ids.hh               # DirectShow IDs & constants
│
├── assets/                      # Application icons
│   ├── play-button-arrowhead.ico
│   ├── pause.ico
│   ├── stop-button.ico
│   ├── fast-forward.ico
│   └── left-arrow.ico
│
├── filters/                     # LAV Filters (DirectShow)
│   ├── x64/                     # 64-bit filters + FFmpeg DLLs
│   └── x86/                     # 32-bit filters + FFmpeg DLLs
│
├── docs/                        # Documentation
│   ├── CONTRIBUTING.md          # Contribution guidelines
│   └── architecture/            # This documentation
│
├── .github/
│   └── workflows/
│       └── ci-cd.yaml           # CI/CD pipeline
│
└── build/                       # Build output (gitignored)
    ├── Vidi.sln                 # Visual Studio solution
    ├── Vidi.vcxproj             # Visual Studio project
    └── Release/                 # Release build output
```

---

## 2. Arsitektur Dua Lapisan

Vidi menggunakan arsitektur **dua lapisan** yang terpisah secara bersih:

```
┌─────────────────────────────────────────────────────────────────┐
│                      GUI LAYER                                  │
│                   (namespace: guiVidi)                          │
│                                                                 │
│  Tanggung Jawab:                                                │
│  • Manajemen window Win32                                       │
│  • Pembuatan & handling UI controls                            │
│  • Event handling (WindowProc)                                  │
│  • Custom rendering (seekbar, volume, subtitle overlay)        │
│  • Mode fullscreen                                              │
│                                                                 │
│  File Utama:                                                    │
│  • gui.cc, gui_windowproc.cc, gui_layout.cc                   │
│  • gui_controls.cc, gui_progressbar.cc, gui_volumebar.cc      │
│  • gui_fullscreen.cc, gui_subtitle.cc                         │
│  • gui.hh, constants.hh                                        │
│                                                                 │
│  Dependency:                                                    │
│  • Win32 API (user32, gdi32, comctl32, comdlg32)              │
│  • Kernel Layer (hanya untuk API pemutaran)                    │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ Hanya API calls:
                              │ • Play(), Pause(), Stop()
                              │ • Seek(), GetDuration()
                              │ • SetVolume(), GetPosition()
                              │ • OpenFile(), Shutdown()
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                      KERNEL LAYER                               │
│                (namespace: kernelPlayerVidi)                    │
│                                                                 │
│  Tanggung Jawab:                                                │
│  • Pemutaran video/audio (DirectShow, Media Foundation)       │
│  • Decoding subtitle (FFmpeg dynamic loading)                  │
│  • Rendering subtitle (libass)                                 │
│  • Manajemen filter graph DirectShow                          │
│  • Load LAV Filters (unregistered)                            │
│                                                                 │
│  File Utama:                                                    │
│  • directShowPlayer.cc, directShowPlayer.hh                   │
│  • subtitleReader.cc, subtitleReader.hh                       │
│  • assRenderer.cc, assRenderer.hh                             │
│  • ffmpeg_dynload.hh, sg_min.hh, ids.hh                      │
│                                                                 │
│  Dependency:                                                    │
│  • DirectShow (strmiids, ole32)                                │
│  • LAV Filters (dynamic loading)                              │
│  • FFmpeg DLLs (dynamic loading)                              │
│  • libass.dll                                                  │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Prinsip Pemisahan

1. **GUI Layer tidak mengetahui implementasi backend**
   - Hanya menggunakan interface `DirectShowPlayer`
   - Tidak ada include header backend di GUI layer

2. **Kernel Layer tidak mengetahui UI**
   - Menggunakan callback/window handle untuk notifikasi
   - Tidak ada dependensi pada控件 atau window management

3. **Backend dapat diganti**
   - DirectShow bisa diganti dengan FFmpeg/libVLC
   - Media Foundation bisa diaktifkan kembali
   - Tanpa mengubah kode GUI

---

## 3. Dependency Graph

```
┌─────────────────────────────────────────────────────────────────┐
│                       Vidi.exe                                  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                   GUI Layer                              │   │
│  │                                                          │   │
│  │  VideoPlayerGUI                                          │   │
│  │       │                                                  │   │
│  │       ├──► user32.dll    (Window management)             │   │
│  │       ├──► gdi32.dll     (GDI drawing)                   │   │
│  │       ├──► comctl32.dll  (Common controls)               │   │
│  │       ├──► comdlg32.dll  (Common dialogs)                │   │
│  │       ├──► uxtheme.dll   (Visual styles)                 │   │
│  │       ├──► dwmapi.dll    (Desktop Window Manager)        │   │
│  │       └──► Kernel Layer  (DirectShowPlayer)              │   │
│  │                                                          │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                   Kernel Layer                           │   │
│  │                                                          │   │
│  │  DirectShowPlayer                                        │   │
│  │       │                                                  │   │
│  │       ├──► strmiids.lib  (DirectShow GUIDs)             │   │
│  │       ├──► ole32.dll     (COM)                           │   │
│  │       ├──► oleaut32.dll  (OLE Automation)               │   │
│  │       ├──► wtsapi32.dll  (Terminal Services)            │   │
│  │       │                                                  │   │
│  │       ├──► LAV Filters (Dynamic Load)                    │   │
│  │       │    ├── LAVSplitter.dll                           │   │
│  │       │    ├── LAVVideo.dll                              │   │
│  │       │    └── LAVAudio.dll                              │   │
│  │       │                                                  │   │
│  │       ├──► VSFilter.dll (Dynamic Load, optional)        │   │
│  │       │                                                  │   │
│  │       ├──► SubtitleReader                                │   │
│  │       │    └──► FFmpeg DLLs (Dynamic Load)              │   │
│  │       │         ├── avformat-XX.dll                      │   │
│  │       │         ├── avcodec-XX.dll                       │   │
│  │       │         └── avutil-XX.dll                        │   │
│  │       │                                                  │   │
│  │       └──► AssRenderer                                   │   │
│  │            └──► libass.dll                               │   │
│  │                 ├── harfbuzz.dll                         │   │
│  │                 ├── freetype.dll                         │   │
│  │                 └── fribidi.dll                          │   │
│  │                                                          │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 4. Komponen Utama

### 4.1 GUI Layer Components

#### VideoPlayerGUI Class

```cpp
namespace guiVidi {
class VideoPlayerGUI {
private:
    // Window handles
    HWND g_hMainWnd;
    HWND g_hVideoArea;
    HWND g_hToolbar;
    
    // Control handles
    HWND g_hPlayBtn, g_hStopBtn;
    HWND g_hSkipBack, g_hSkipForward;
    HWND g_hFullscreenBtn, g_hPlaylistBtn;
    HWND g_hProgress, g_hVolume;
    HWND g_hTimeLabel;
    
    // Menu
    HMENU m_hMenuBar;
    HACCEL m_hAccel;
    
    // Fonts
    HFONT m_hModernFont;
    HFONT m_hTimeFont;
    HFONT m_hTipFont;
    
    // Icons
    HICON m_hIconPlay, m_hIconPause, m_hIconStop;
    HICON m_hIconSkipBack, m_hIconSkipForward;
    HICON m_hIconFullscreen;
    
    // State
    bool m_isPlaying;
    bool m_isFullscreen;
    bool m_isLooping;
    bool m_isShuffle;
    bool m_isDraggingProgress;
    double m_cachedDuration;
    float m_lastVolume;
    bool m_isMuted;
    
    // Kernel layer
    kernelPlayerVidi::DirectShowPlayer m_player;
    
    // Subtitle overlay
    HWND m_hSubOverlay[MAX_SUB_OVERLAYS];
    HBITMAP m_hSubBmp[2];
    void* m_pSubBmpBits[2];
    
public:
    bool Initialize(HINSTANCE hInstance, int nCmdShow);
    int Run();
    
private:
    static LRESULT CALLBACK WindowProc(...);
    void CreateMenuBar(HWND hwnd);
    void CreateControls(HWND hwnd);
    void LayoutControls(int width, int height);
    void OnCommand(WPARAM wParam, LPARAM lParam);
    void OpenFileDialog();
    void SetPlayPauseUI(bool playing);
    void DrawVlcSeekbar(HDC hdc);
    void DrawVlcVolumeBar(HDC hdc);
    void EnterFullscreen();
    void ExitFullscreen();
    void UpdateSubtitleDisplays(double posSeconds);
};
}
```

#### File Responsibilities

| File | Tanggung Jawab |
|---|---|
| `gui.cc` | Inisialisasi window, main message loop |
| `gui_windowproc.cc` | Handle semua Windows messages |
| `gui_layout.cc` | Posisi dan ukuran controls |
| `gui_controls.cc` | Buat buttons, trackbar, labels |
| `gui_progressbar.cc` | Custom seekbar rendering (VLC-style) |
| `gui_volumebar.cc` | Custom volume slider rendering |
| `gui_fullscreen.cc` | Toggle fullscreen mode |
| `gui_subtitle.cc` | Subtitle overlay rendering |
| `constants.hh` | Warna, dimensi, DPI helpers, utilities |

---

### 4.2 Kernel Layer Components

#### DirectShowPlayer Class

```cpp
namespace kernelPlayerVidi {
class DirectShowPlayer {
private:
    // DirectShow interfaces
    IGraphBuilder* m_pGraph;
    IMediaControl* m_pControl;
    IMediaEventEx* m_pEvent;
    IMediaSeeking* m_pSeeking;
    IVideoWindow* m_pVideoWindow;
    IBasicAudio* m_pBasicAudio;
    IBasicVideo* m_pBasicVideo;
    
    // LAV Filters (dynamic loading)
    HMODULE m_hLavSplitterDll;
    HMODULE m_hLavVideoDll;
    HMODULE m_hLavAudioDll;
    HMODULE m_hVSFilterDll;
    
    // Video window
    HWND m_hVideoWnd;
    HWND m_hNotifyWnd;
    
    // Sample Grabber (for audio processing)
    IBaseFilter* m_pGrabberBF;
    ISampleGrabber* m_pGrabber;
    std::atomic<float> m_dspGain;
    
    // Subtitle
    SubtitleReader m_subReader;
    HANDLE m_hSubThread;
    std::atomic<uint32_t> m_mediaReadyGen;
    
    // State
    bool m_graphBuilt;
    
public:
    bool Initialize(HWND hVideoWnd, HWND hNotifyWnd);
    bool OpenFile(const wchar_t* path);
    void Play();
    void Pause();
    void Stop();
    void SetVolume(float vol);
    void Seek(double seconds);
    double GetDuration();
    double GetPosition();
    void Shutdown();
    
    void UpdateVideoSize();
    void HandleGraphEvent();
    SubtitleReader& GetSubtitleReader();
    
private:
    bool CreateGraph();
    bool AddVideoRenderer();
    bool EnsureGainFilter();
    void DestroyGraph();
    IBaseFilter* LoadUnregisteredFilter(...);
};
}
```

#### SubtitleReader Class

```cpp
namespace kernelPlayerVidi {
class SubtitleReader {
private:
    // FFmpeg contexts (dynamic loading)
    AVFormatContext* m_fmtCtx;
    AVCodecContext* m_codecCtx;
    int m_subtitleStreamIndex;
    
    // Parsed subtitles
    std::vector<SubtitleEntry> m_subtitles;
    bool m_isLoaded;
    std::atomic<bool> m_stopLoading;
    
public:
    bool LoadFromFile(const wchar_t* path);
    void StopLoading();
    bool IsLoaded() const;
    std::vector<SubtitleEntry> GetSubtitles() const;
    std::vector<SubtitleEntry> GetActiveSubtitles(double time);
    
private:
    bool LoadFFmpegDLLs();
    void FreeFFmpegDLLs();
};
}
```

#### AssRenderer Class

```cpp
namespace kernelPlayerVidi {
class AssRenderer {
private:
    ASS_Library* m_assLibrary;
    ASS_Renderer* m_assRenderer;
    ASS_Track* m_assTrack;
    bool m_isInitialized;
    
public:
    bool Initialize();
    void Shutdown();
    bool LoadTrack(const char* assData);
    void RenderFrame(int64_t timestamp, ASS_Image** images);
    void SetResolution(int width, int height);
};
}
```

---

## 5. Integration Points

### 5.1 DirectShow Integration

```
┌─────────────────────────────────────────────────────────────────┐
│                  DirectShow Filter Graph                        │
│                                                                 │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐        │
│  │  Source      │    │ LAV         │    │ Video       │        │
│  │  Filter      │───►│ Splitter    │───►│ Renderer    │        │
│  │  (File)      │    │ (Demux)     │    │ (VMR-7)     │        │
│  └─────────────┘    └──────┬──────┘    └─────────────┘        │
│                            │                                    │
│                            │                                    │
│                       ┌────┴────┐                              │
│                       │         │                              │
│                       ▼         ▼                              │
│                ┌──────────┐ ┌──────────┐                       │
│                │ LAV      │ │ LAV      │                       │
│                │ Video    │ │ Audio    │                       │
│                │ Decoder  │ │ Decoder  │                       │
│                └────┬─────┘ └────┬─────┘                       │
│                     │            │                              │
│                     ▼            ▼                              │
│                ┌──────────┐ ┌──────────┐ ┌──────────┐        │
│                │ Video    │ │ Audio    │ │ Sample   │        │
│                │ Renderer │ │ Renderer │ │ Grabber  │        │
│                │ (VMR-7)  │ │ (DirectS)│ │ (Audio)  │        │
│                └──────────┘ └──────────┘ └──────────┘        │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 5.2 LAV Filters Loading

```cpp
// Dynamic loading tanpa registrasi
IBaseFilter* DirectShowPlayer::LoadUnregisteredFilter(
    const wchar_t* dllPath, 
    REFCLSID clsid, 
    HMODULE* pOutModule
) {
    HMODULE hDll = LoadLibraryW(dllPath);
    if (!hDll) return nullptr;
    
    using CreateInstanceFunc = HRESULT(WINAPI*)(IUnknown*, REFIID, void**);
    auto pfnCreate = reinterpret_cast<CreateInstanceFunc>(
        GetProcAddress(hDll, "DllGetClassObject")
    );
    
    // ... create filter instance
}
```

### 5.3 FFmpeg Dynamic Loading

```cpp
// Contoh dynamic loading FFmpeg
namespace ffmpeg {
    // Function pointers
    static avformat_alloc_context_t* avformat_alloc_context = nullptr;
    static avformat_open_input_t* avformat_open_input = nullptr;
    static avformat_find_stream_info_t* avformat_find_stream_info = nullptr;
    static avcodec_find_decoder_t* avcodec_find_decoder = nullptr;
    static avcodec_alloc_context3_t* avcodec_alloc_context3 = nullptr;
    static avcodec_open2_t* avcodec_open2 = nullptr;
    
    bool LoadDLLs() {
        HMODULE hAvFormat = LoadLibraryW(L"avformat-59.dll");
        HMODULE hAvCodec = LoadLibraryW(L"avcodec-59.dll");
        HMODULE hAvUtil = LoadLibraryW(L"avutil-57.dll");
        
        if (!hAvFormat || !hAvCodec || !hAvUtil) return false;
        
        // Load function pointers
        avformat_alloc_context = (avformat_alloc_context_t*)
            GetProcAddress(hAvFormat, "avformat_alloc_context");
        // ... load other functions
    }
}
```

### 5.4 Subtitle Rendering Pipeline

```
┌─────────────────────────────────────────────────────────────────┐
│              SUBTITLE RENDERING PIPELINE                        │
│                                                                 │
│  Video File                                                    │
│       │                                                         │
│       ▼                                                         │
│  ┌─────────────┐                                               │
│  │ FFmpeg      │  Decode subtitle stream                       │
│  │ (Dynamic)   │  from container                               │
│  └──────┬──────┘                                               │
│         │                                                       │
│         ▼                                                       │
│  ┌─────────────┐                                               │
│  │ Subtitle    │  Parse SRT/ASS format                         │
│  │ Reader      │  into SubtitleEntry structs                   │
│  └──────┬──────┘                                               │
│         │                                                       │
│         ▼                                                       │
│  ┌─────────────┐                                               │
│  │ Time-based  │  Get active subtitles                         │
│  │ Filter      │  for current playback position                │
│  └──────┬──────┘                                               │
│         │                                                       │
│         ▼                                                       │
│  ┌─────────────┐                                               │
│  │ ASS         │  Render subtitle to                           │
│  │ Renderer    │  bitmap images                                │
│  └──────┬──────┘                                               │
│         │                                                       │
│         ▼                                                       │
│  ┌─────────────┐                                               │
│  │ Overlay     │  Draw bitmap on top of                        │
│  │ Window      │  video area                                   │
│  └─────────────┘                                               │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 6. Build System

### 6.1 CMake Configuration

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
project(Vidi VERSION 0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES
    src/main.cc
    src/gui/gui.cc
    src/gui/gui_windowproc.cc
    src/gui/gui_layout.cc
    src/gui/gui_controls.cc
    src/gui/gui_progressbar.cc
    src/gui/gui_volumebar.cc
    src/gui/gui_fullscreen.cc
    src/gui/gui_subtitle.cc
    src/kernels/directShowPlayer.cc
    src/kernels/subtitleReader.cc
    src/kernels/assRenderer.cc
    src/app.rc
)

# Build executable
add_executable(${PROJECT_NAME} WIN32 ${SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    include
    src
    src/gui
    src/kernels
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    user32 gdi32 comctl32 comdlg32
    strmiids ole32 oleaut32
    uxtheme wtsapi32 dwmapi
    libass::libass
)

# Post-build: copy assets and filters
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
)
```

### 6.2 CI/CD Pipeline

```yaml
# .github/workflows/ci-cd.yaml
name: CI/CD Pipeline

on:
  push:
    branches: [main, development, 'feature/**']
  pull_request:
    branches: [main]

jobs:
  lint:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - name: Check clang-format
        run: |
          clang-format --dry-run --Werror src/**/*.cc include/**/*.hh

  build:
    runs-on: windows-latest
    needs: lint
    steps:
      - uses: actions/checkout@v4
      - name: Setup vcpkg
        uses: lukka/run-vcpkg@v11
      - name: Configure CMake
        run: cmake -B build --preset vcpkg
      - name: Build
        run: cmake --build build --config Release
      - name: Create MSI Installer
        run: cpack --config build/CPackConfig.cmake
```

---

## 7. Code Style

### 7.1 Clang Format

```yaml
# .clang-format
BasedOnStyle: Microsoft
IndentWidth: 4
ColumnLimit: 120
PointerAlignment: Left
SortIncludes: false  # Windows.h ordering sensitivity
```

### 7.2 Naming Conventions

| Jenis | Konvensi | Contoh |
|---|---|---|
| Class | PascalCase | `VideoPlayerGUI`, `DirectShowPlayer` |
| Method | PascalCase | `Initialize()`, `OpenFile()`, `Play()` |
| Member Variable | Hungarian + camelCase | `m_player`, `g_hPlayBtn`, `m_isPlaying` |
| Constant | UPPER_SNAKE_CASE | `MAX_SUB_OVERLAYS`, `TIMER_ID` |
| Namespace | camelCase + suffix | `guiVidi`, `kernelPlayerVidi` |
| File | snake_case | `gui_windowproc.cc`, `directShowPlayer.cc` |

### 7.3 Header Guards

```cpp
// Traditional include guards (not #pragma once)
#ifndef GUI_HH
#define GUI_HH

// ... declarations

#endif // GUI_HH
```

---

## 8. Runtime Dependencies

### 8.1 Required DLLs

```
Vidi.exe
├── user32.dll          (Windows)
├── gdi32.dll           (Windows)
├── comctl32.dll        (Windows)
├── comdlg32.dll        (Windows)
├── ole32.dll           (Windows)
├── oleaut32.dll        (Windows)
├── uxtheme.dll         (Windows)
├── wtsapi32.dll        (Windows)
├── dwmapi.dll          (Windows)
├── strmiids.dll        (DirectShow)
│
├── libass.dll          (vcpkg)
│   ├── harfbuzz.dll
│   ├── freetype.dll
│   └── fribidi.dll
│
├── LAVSplitter.dll     (filters/x64)
├── LAVVideo.dll        (filters/x64)
├── LAVAudio.dll        (filters/x64)
│
├── avformat-59.dll     (FFmpeg, optional)
├── avcodec-59.dll      (FFmpeg, optional)
└── avutil-57.dll       (FFmpeg, optional)
```

### 8.2 File Structure After Build

```
build/Release/
├── Vidi.exe
├── libass.dll
├── harfbuzz.dll
├── freetype.dll
├── fribidi.dll
├── LAVSplitter.dll
├── LAVVideo.dll
├── LAVAudio.dll
├── assets/
│   ├── play-button-arrowhead.ico
│   ├── pause.ico
│   ├── stop-button.ico
│   ├── fast-forward.ico
│   └── left-arrow.ico
└── filters/
    ├── x64/
    └── x86/
```

---

## Referensi

- [Win32 API Documentation](https://learn.microsoft.com/en-us/windows/win32/)
- [DirectShow Documentation](https://learn.microsoft.com/en-us/windows/win32/directshow)
- [Media Foundation Documentation](https://learn.microsoft.com/en-us/windows/win32/medfound/)
- [libass Documentation](https://github.com/libass/libass)
- [FFmpeg Documentation](https://ffmpeg.org/documentation.html)

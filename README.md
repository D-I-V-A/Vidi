# 🎬 Vidi

Vidi adalah aplikasi pemutar video untuk **Windows** yang dibangun dengan **Win32 API** untuk GUI, dan mendukung dua backend pemutaran sekaligus:

- **Media Foundation** (`IMFPMediaPlayer`) — backend utama, stabil untuk format umum (H.264, MP4, AAC).
- **DirectShow** + **LAV Filters** — backend alternatif untuk codec luas (HEVC, VP9, FLAC, dll).

Subtitle rendering ditangani oleh **libass** (untuk format ASS/SSA) dan **FFmpeg** (untuk decoding subtitle dari container video).

---

## ✨ Fitur

- ▶️ Kontrol playback dasar: Play, Pause, Stop
- ⏩ Skip maju/mundur 10 detik
- 🎚️ Progress bar (timeline) dengan skala dinamis mengikuti durasi asli video
- 🖱️ Klik langsung di timeline untuk seek instan (bukan sekadar drag)
- 🔊 Kontrol volume + mute/unmute
- 🖥️ Toggle fullscreen
- ⌨️ Keyboard shortcut lengkap
- 📁 Buka file video langsung dari menu (`Open...`)
- 🧭 Menu bar bergaya VLC (Media, Playback, Audio, View, Help)
- 📝 Subtitle support — SRT, ASS/SSA dengan karaoke lyrics rendering

---

## ⌨️ Keyboard Shortcut

| Tombol | Aksi |
|---|---|
| `Space` | Play / Pause |
| `S` | Stop |
| `←` / `→` | Skip mundur / maju 10 detik |
| `↑` / `↓` | Volume naik / turun |
| `M` | Mute / Unmute |
| `F` | Toggle fullscreen |
| `Ctrl+O` | Buka file video |

---

## 🏗️ Arsitektur

Project ini dipisah jadi 2 layer utama:

```
src/
├── main.cc
├── gui/                          → namespace guiVidi
│   ├── gui.cc                    # Inisialisasi window & main loop
│   ├── gui_windowproc.cc         # Window message handling (WindowProc)
│   ├── gui_layout.cc             # Control layout / positioning
│   ├── gui_controls.cc           # Pembuatan tombol & trackbar
│   ├── gui_progressbar.cc        # VLC-style seekbar rendering
│   ├── gui_volumebar.cc          # Volume slider
│   ├── gui_fullscreen.cc         # Toggle fullscreen
│   └── gui_subtitle.cc           # Subtitle overlay rendering
└── kernels/                      → namespace kernelPlayerVidi
    ├── directShowPlayer.hh/cc    # DirectShow backend + LAV Filters
    ├── mfVideoPlayer.hh/cc       # Media Foundation backend
    ├── subtitleReader.hh/cc      # FFmpeg subtitle parsing
    ├── assRenderer.hh/cc         # libass subtitle rendering
    ├── sg_min.hh                 # Sample Grabber COM interface
    ├── ids.hh                    # DirectShow IDs & constants
    └── ffmpeg_dynload.hh         # FFmpeg dynamic loading (no link-time dep)

include/
├── gui/
│   ├── gui.hh                    # VideoPlayerGUI class declaration
│   └── constants.hh              # Colors, dimensions, DPI helpers
└── kernels/
    └── (header files mirrors src/kernels/)

filters/
├── x64/                          # LAV Filters (DirectShow) — 64-bit
└── x86/                          # LAV Filters (DirectShow) — 32-bit

assets/                           # Icons (play, pause, stop, skip, fullscreen)
```

- **`guiVidi`** — menangani semua hal terkait Win32 window, kontrol UI (tombol, trackbar, menu), dan event handling (`WindowProc`).
- **`kernelPlayerVidi`** — membungkus API Media Foundation (`IMFPMediaPlayer`) dan DirectShow untuk decode & render video/audio, terpisah total dari logic UI. Juga menangani subtitle via FFmpeg + libass.

Pemisahan ini memungkinkan backend media diganti (misal ke FFmpeg/libVLC) tanpa perlu menyentuh kode GUI sama sekali.

### 📚 Dokumentasi Internal

Untuk penjelasan detail arsitektur internal, lihat [docs/architecture/internal/](docs/architecture/internal/):

| Dokumen | Deskripsi |
|---|---|
| [Arsitektur Khusus Proyek](docs/architecture/internal/project-architecture.md) | Detail dua lapisan, dependency, build system, komponen |
| [Flowchart Internal](docs/architecture/internal/app-flowchart.md) | Alur eksekusi dari WinMain hingga Destroy |

---

## 🔧 Requirements

| Kebutuhan | Minimum | Direkomendasikan |
|---|---|---|
| **OS** | Windows 10 | Windows 11 |
| **Compiler** | Visual Studio 2022 (MSVC) + Windows SDK | Visual Studio 2022 (latest) |
| **CMake** | ≥ 3.15 | ≥ 3.20 |
| **vcpkg** | Latest | Latest |
| **RAM** | 2 GB | 4 GB atau lebih |
| **Codec** | Bawaan Windows (H.264/MP4) | + HEVC Video Extensions / LAV Filters |
| **GPU** | — | Mendukung hardware decoding (DXVA) untuk playback 1080p/4K lebih ringan |

---

## 🛠️ Build

### 1. Install vcpkg

```bash
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
setx VCPKG_ROOT "C:\vcpkg"
```

### 2. Install Dependencies

```bash
C:\vcpkg\vcpkg install
```

### 3. Build

```bash
cmake -B build --preset vcpkg
cmake --build build --config Release
```

Executable akan tersedia di:
```
build/Release/Vidi.exe
```

---

## 📝 Subtitle

Vidi mendukung subtitle dari dalam container video (MKV, MP4, AVI) dengan format:

| Format | Support |
|---|---|
| SRT (SubRip) | ✅ |
| ASS/SSA (Advanced SubStation Alpha) | ✅ |
| Karaoke Lyrics | ✅ |

Subtitle di-decode menggunakan FFmpeg (dynamic loading, tidak perlu install FFmpeg secara global) dan di-render menggunakan libass dengan overlay transparan di atas video.

---

## 🚧 Status Project

Masih dalam tahap pengembangan aktif (`alpha`). Beberapa fitur yang direncanakan ke depan:

- [ ] Playlist / queue video
- [ ] Drag-and-drop file ke window
- [ ] Preview thumbnail saat hover di timeline
- [ ] Audio-only file support (visualizer)

---

## 📄 License

MIT License — see [LICENSE](LICENSE) for details.

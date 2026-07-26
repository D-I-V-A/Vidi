# 🎬 Vidi

Vidi adalah aplikasi pemutar video sederhana berbasis **Windows API (Win32)** untuk antarmuka GUI desktop, dan **Windows Media Foundation (`IMFPMediaPlayer`)** sebagai backend pemutaran video/audio.

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
├── gui/ → namespace guiVidi
│ ├── gui.hh
│ └── gui.cc
└── kernels/ → namespace kernelPlayerVidi
├── ids.hh
├── mfVideoPlayer.hh
└── mfVideoPlayer.cc

```
- **`guiVidi`** — menangani semua hal terkait Win32 window, kontrol UI (tombol, trackbar, menu), dan event handling (`WindowProc`).
- **`kernelPlayerVidi`** — membungkus API Media Foundation (`IMFPMediaPlayer`) untuk proses decode dan render video/audio, terpisah total dari logic UI.

Pemisahan ini memungkinkan backend media diganti (misal ke FFmpeg/libVLC) tanpa perlu menyentuh kode GUI sama sekali.

---


## 🔧 Requirements

- Windows 7 atau lebih baru
- Visual Studio 2019/2022 (MSVC) dengan Windows SDK
- CMake ≥ 3.15

---

## 🛠️ Build

```bash
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

Executable akan tersedia di:

```./build/Release/Vidi.exe```

## 🚧 Status Project

Masih dalam tahap pengembangan aktif (`alpha`). Beberapa fitur yang direncanakan ke depan:
- Playlist / queue video
- Drag-and-drop file ke window
- Subtitle support
- Preview thumbnail saat hover di timeline

---
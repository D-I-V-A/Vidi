# Contributing to Vidi

Terima kasih sudah tertarik untuk berkontribusi di Vidi! Berikut panduan untuk memulai.

---

## 🚀 Development Environment Setup

### Prerequisites

| Tool | Version | Notes |
|---|---|---|
| **Visual Studio 2022** | Latest | Dengan workload "Desktop development with C++" |
| **CMake** | ≥ 3.15 | Sudah ter-bundl dengan VS 2022 |
| **vcpkg** | Latest | Package manager untuk C/C++ |
| **Git** | Latest | Version control |

### Setup

1. **Clone repository:**
   ```bash
   git clone https://github.com/D-I-V-A/Vidi.git
   cd Vidi
   ```

2. **Install vcpkg (kalau belum):**
   ```bash
   git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
   C:\vcpkg\bootstrap-vcpkg.bat
   setx VCPKG_ROOT "C:\vcpkg"
   ```

3. **Install dependencies:**
   ```bash
   C:\vcpkg\vcpkg install libass:x64-windows
   ```

4. **Build:**
   ```bash
   cmake -B build --preset vcpkg
   cmake --build build --config Release
   ```

---

## 🌿 Branching Strategy

```
main          ← production, release branch
  └── development  ← integration branch
        ├── feat/*     ← fitur baru
        ├── fix/*      ← bug fix
        └── docs/*     ← dokumentasi
```

| Branch | Purpose | CI/CD |
|---|---|---|
| `main` | Release, production-ready | Build + Release installer |
| `development` | Integration branch | Clang-format check |
| `feat/*` | Fitur baru | Clang-format check |
| `fix/*` | Bug fix | Clang-format check |
| `docs/*` | Dokumentasi | Clang-format check |

---

## 📝 Commit Message Convention

Format:

```
<type>: <description>
```

### Type

| Type | Keterangan |
|---|---|
| `feat` | Fitur baru |
| `fix` | Bug fix |
| `docs` | Dokumentasi |
| `style` | Format code (clang-format) |
| `refactor` | Refactor tanpa mengubah behavior |
| `ci` | CI/CD changes |
| `chore` | Maintenance tasks |

### Contoh

```
feat: add subtitle overlay rendering
fix: fullscreen render outside app window
docs: update README with dual backend architecture
style: apply clang-format to all source files
ci: fix cmake build by adding vcpkg and libass setup
```

---

## 🎨 Code Style

### Clang-Format

Project ini menggunakan `.clang-format` dengan gaya **Microsoft**. Jalankan sebelum commit:

```bash
clang-format -i <file>
```

Atau format semua file:

```bash
find . -name "*.cc" -o -name "*.hh" | xargs clang-format -i
```

### Konfigurasi `.clang-format`

- `IndentWidth`: 4
- `ColumnLimit`: 120
- `BreakBeforeBraces`: Attach
- `PointerAlignment`: Left
- `SortIncludes`: Never (Windows.h harus di atas)

### Naming Convention

| Element | Convention | Contoh |
|---|---|---|
| Class | PascalCase | `DirectShowPlayer` |
| Method | PascalCase | `OpenFile()`, `SetVolume()` |
| Member variable | `m_` prefix | `m_pPlayer`, `m_hWnd` |
| Constant | UPPER_SNAKE_CASE | `COLOR_MODERN_BG` |
| Namespace | camelCase | `guiVidi`, `kernelPlayerVidi` |

### Pointer/COM Naming

| Prefix | Keterangan | Contoh |
|---|---|---|
| `m_p` | COM interface pointer | `m_pPlayer`, `m_pGraph` |
| `m_h` | Handle | `m_hWnd`, `m_hVideoWnd` |
| `m_b` | Boolean | `m_isPlaying` |
| `g_` | Global/member di GUI | `g_hPlayBtn` |

---

## 🔄 Pull Request Workflow

1. **Buat branch baru** dari `development`:
   ```bash
   git checkout development
   git checkout -b feat/nama-fitur
   ```

2. **Buat perubahan** dan commit:
   ```bash
   git add -A
   git commit -m "feat: deskripsi"
   ```

3. **Push** dan buka PR:
   ```bash
   git push origin feat/nama-fitur
   ```

4. **Buka PR** ke branch `development`.

5. **Tunggu CI pass** (clang-format check).

6. **Merge** setelah review (kalau ada) dan CI green.

---

## 🏗️ Project Structure

```
Vidi/
├── src/                    # Source files
│   ├── main.cc
│   ├── gui/                # GUI layer (guiVidi)
│   └── kernels/            # Backend layer (kernelPlayerVidi)
├── include/                # Header files
│   ├── gui/
│   └── kernels/
├── filters/                # LAV Filters (bundled)
├── assets/                 # Icons
├── docs/                   # Documentation
│   ├── CONTRIBUTING.md
│   └── API.md
├── .github/workflows/      # CI/CD
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json
├── .clang-format
└── LICENSE
```

---

## 🐛 Bug Report

Kalau menemukan bug, buka issue dengan format:

```
**Deskripsi:** [Jelaskan bug-nya]
**Expected:** [Apa yang seharusnya terjadi]
**Actual:** [Apa yang terjadi]
**Steps to reproduce:**
1. ...
2. ...
3. ...
**Environment:** OS, VS version, Windows SDK
```

---

## 💡 Feature Request

Untuk fitur baru, buka issue dengan format:

```
**Deskripsi:** [Jelaskan fitur yang diinginkan]
**Use case:** [Kenapa fitur ini berguna]
**Alternatives:** [Alternatif yang sudah dipertimbangkan]
```

---

## 📄 License

Dengan berkontribusi, kamu setuju bahwa kontribusimu akan di-licensed di bawah [MIT License](LICENSE).

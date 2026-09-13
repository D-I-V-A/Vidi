# Contributing to Vidi

Terima kasih atas minat Anda untuk berkontribusi pada Vidi! Dokumen ini menjelaskan cara memulai, aturan development, dan proses pull request.

---

## 📋 Daftar Isi

- [Persyaratan](#persyaratan)
- [Setup Development](#setup-development)
- [Branching Strategy](#branching-strategy)
- [Commit Convention](#commit-convention)
- [Code Style](#code-style)
- [Pull Request Process](#pull-request-process)
- [Bug Report](#bug-report)
- [Feature Request](#feature-request)

---

## Persyaratan

| Kebutuhan | Minimum | Direkomendasikan |
|---|---|---|
| **OS** | Windows 10 | Windows 11 |
| **Compiler** | Visual Studio 2022 (MSVC) + Windows SDK | Visual Studio 2022 (latest) |
| **CMake** | ≥ 3.15 | ≥ 3.20 |
| **vcpkg** | Latest | Latest |
| **Git** | Latest | Latest |

---

## Setup Development

### 1. Fork & Clone Repository

```bash
# Fork repository di GitHub, lalu clone
git clone https://github.com/USERNAME/Vidi.git
cd Vidi
```

### 2. Install vcpkg

```bash
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
setx VCPKG_ROOT "C:\vcpkg"
```

### 3. Install Dependencies

```bash
C:\vcpkg\vcpkg install
```

### 4. Build

```bash
cmake -B build --preset vcpkg
cmake --build build --config Release
```

### 5. Verify

```bash
# Jalankan executable
build\Release\Vidi.exe
```

---

## Branching Strategy

```
main
├── development          # Branch integrasi
│   ├── feature/xxx      # Fitur baru
│   ├── fix/xxx          # Bug fix
│   └── refactor/xxx     # Refaktorasi
```

| Branch | Deskripsi | Protect |
|---|---|---|
| `main` | Branch production, stabil | ✅ |
| `development` | Branch integrasi untuk testing | ⚠️ |
| `feature/xxx` | Branch untuk fitur baru | ❌ |
| `fix/xxx` | Branch untuk perbaikan bug | ❌ |

### Workflow

1. Buat branch dari `development`
2. Develop fitur/fix
3. Push dan buat PR ke `development`
4. Setelah review, merge ke `development`
5. Ketika stabil, `development` di-merge ke `main`

---

## Commit Convention

Gunakan format **Conventional Commits**:

```
<type>(<scope>): <description>

[optional body]

[optional footer]
```

### Type

| Type | Deskripsi |
|---|---|
| `feat` | Fitur baru |
| `fix` | Perbaikan bug |
| `docs` | Perubahan dokumentasi |
| `style` | Perubahan format (tidak mempengaruhi kode) |
| `refactor` | Refaktorasi (bukan fitur baru/bukan perbaikan bug) |
| `perf` | Optimasi performa |
| `test` | Menambahkan/memperbaiki test |
| `chore` | Maintenance (build, CI, dll) |

### Scope

| Scope | Deskripsi |
|---|---|
| `gui` | GUI layer (window, controls) |
| `kernel` | Kernel layer (player, subtitle) |
| `build` | Build system (CMake) |
| `ci` | CI/CD pipeline |
| `docs` | Dokumentasi |

### Contoh

```bash
# Fitur baru
git commit -m "feat(gui): add playlist support"

# Perbaikan bug
git commit -m "fix(kernel): resolve subtitle sync issue"

# Dokumentasi
git commit -m "docs: update architecture documentation"

# Refaktorasi
git commit -m "refactor(gui): simplify fullscreen toggle"
```

---

## Code Style

### Clang Format

```yaml
# .clang-format
BasedOnStyle: Microsoft
IndentWidth: 4
ColumnLimit: 120
PointerAlignment: Left
SortIncludes: false  # Windows.h ordering sensitivity
```

### Jalankan Format Check

```bash
# Dry run (cek saja, tidak mengubah)
clang-format --dry-run --Werror src/**/*.cc include/**/*.hh

# Format semua file
clang-format -i src/**/*.cc include/**/*.hh
```

### Naming Conventions

| Jenis | Konvensi | Contoh |
|---|---|---|
| Class | PascalCase | `VideoPlayerGUI`, `DirectShowPlayer` |
| Method | PascalCase | `Initialize()`, `OpenFile()`, `Play()` |
| Member Variable | Hungarian + camelCase | `m_player`, `g_hPlayBtn`, `m_isPlaying` |
| Constant | UPPER_SNAKE_CASE | `MAX_SUB_OVERLAYS`, `TIMER_ID` |
| Namespace | camelCase + suffix | `guiVidi`, `kernelPlayerVidi` |
| File | snake_case | `gui_windowproc.cc`, `directShowPlayer.cc` |

### Header Guards

```cpp
// Traditional include guards (not #pragma once)
#ifndef GUI_HH
#define GUI_HH

// ... declarations

#endif // GUI_HH
```

---

## Pull Request Process

### Sebelum Submit PR

1. **Pastikan build berhasil**
   ```bash
   cmake --build build --config Release
   ```

2. **Jalankan clang-format**
   ```bash
   clang-format --dry-run --Werror src/**/*.cc include/**/*.hh
   ```

3. **Update dokumentasi** jika ada perubahan signifikan

4. **Commit dengan format yang benar**
   ```bash
   git commit -m "feat(scope): description"
   ```

### Format PR

```markdown
## Deskripsi
Deskripsi singkat tentang perubahan

## Jenis Perubahan
- [ ] Fitur baru
- [ ] Perbaikan bug
- [ ] Refaktorasi
- [ ] Dokumentasi
- [ ] Lainnya

## Checklist
- [ ] Build berhasil
- [ ] Tidak ada warning
- [ ] Code review selesai
- [ ] Dokumentasi diupdate (jika perlu)

## Screenshot (jika ada perubahan UI)
```

### Review Process

1. PR akan direview oleh maintainer
2. Perbaikan mungkin diminta
3. Setelah approve, PR akan di-merge

---

## Bug Report

### Format Bug Report

```markdown
## Deskripsi
Deskripsi singkat bug

## Steps to Reproduce
1. Buka Vidi
2. Klik '...'
3. Error muncul

## Expected Behavior
Apa yang diharapkan terjadi

## Actual Behavior
Apa yang sebenarnya terjadi

## Environment
- OS: Windows 10/11
- Version: v0.1-alpha
- GPU: (jika relevan)

## Additional Context
Screenshot, logs, atau informasi lain
```

---

## Feature Request

### Format Feature Request

```markdown
## Deskripsi
Deskripsi fitur yang diinginkan

## Use Case
Mengapa fitur ini dibutuhkan?

## Proposed Solution
Solusi yang diusulkan (jika ada)

## Alternatives
Alternatif yang dipertimbangkan

## Additional Context
Screenshot, referensi, atau informasi lain
```

---

## 📁 Struktur Proyek

```
Vidi/
├── src/                    # Source files
│   ├── main.cc            # Entry point
│   ├── gui/               # GUI layer
│   └── kernels/           # Kernel layer
├── include/                # Header files
│   ├── gui/
│   └── kernels/
├── assets/                 # Icons
├── filters/                # LAV Filters
├── docs/                   # Documentation
└── .github/                # CI/CD
```

---

## 📚 Referensi

- [Win32 API Documentation](https://learn.microsoft.com/en-us/windows/win32/)
- [DirectShow Documentation](https://learn.microsoft.com/en-us/windows/win32/directshow)
- [CMake Documentation](https://cmake.org/cmake/help/latest/)
- [Conventional Commits](https://www.conventionalcommits.org/)

---

## 📄 License

Dengan berkontribusi, Anda setuju bahwa kontribusi Anda akan dilisensikan di bawah MIT License.

---

## ❓ Pertanyaan?

Jika ada pertanyaan, buka issue di GitHub atau hubungi maintainer.

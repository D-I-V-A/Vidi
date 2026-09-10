// ============================================================
// Unit Test: LAV Filter Module Caching
// Verifikasi optimasi LoadLibrary/FreeLibrary cycle
// ============================================================

#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <chrono>
#include <string>
#include <vector>
#include <functional>

// ============================================================
// Test Framework (minimal, tanpa dependency)
// ============================================================

static int g_testsPassed = 0;
static int g_testsFailed = 0;

#define TEST_ASSERT(condition, msg)                                         \
    do {                                                                    \
        if (!(condition)) {                                                 \
            printf("  FAIL: %s (line %d)\n", msg, __LINE__);               \
            g_testsFailed++;                                                \
        } else {                                                            \
            printf("  PASS: %s\n", msg);                                    \
            g_testsPassed++;                                                \
        }                                                                   \
    } while (0)

#define TEST_SECTION(name) printf("\n=== %s ===\n", name)

// ============================================================
// Helper: GetExeDir
// ============================================================

static std::wstring GetExeDir() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring full(path);
    size_t pos = full.find_last_of(L"\\/");
    return (pos != std::wstring::npos) ? full.substr(0, pos) : L".";
}

// ============================================================
// Test 1: LoadLibrary caching behavior
// ============================================================

static void TestLoadLibraryCaching() {
    TEST_SECTION("Test 1: LoadLibrary Caching Behavior");

    std::wstring exeDir = GetExeDir();
    std::wstring dllPath = exeDir + L"\\filters\\x64\\LAVSplitter.ax";

    // Load pertama
    auto t1 = std::chrono::high_resolution_clock::now();
    HMODULE h1 = LoadLibraryW(dllPath.c_str());
    auto t2 = std::chrono::high_resolution_clock::now();

    TEST_ASSERT(h1 != nullptr, "LoadLibraryW pertama berhasil");

    // Load kedua (seharusnya return handle sama, sangat cepat)
    auto t3 = std::chrono::high_resolution_clock::now();
    HMODULE h2 = LoadLibraryW(dllPath.c_str());
    auto t4 = std::chrono::high_resolution_clock::now();

    TEST_ASSERT(h2 != nullptr, "LoadLibraryW kedua berhasil");
    TEST_ASSERT(h1 == h2, "LoadLibraryW kedua return handle SAMA (cached)");

    auto loadFirst = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1);
    auto loadSecond = std::chrono::duration_cast<std::chrono::nanoseconds>(t4 - t3);

    printf("  LoadLibraryW pertama: %lld ns\n", (long long)loadFirst.count());
    printf("  LoadLibraryW kedua:  %lld ns\n", (long long)loadSecond.count());

    // FreeLibrary harus dipanggil 2x (karena LoadLibrary 2x)
    FreeLibrary(h1);
    FreeLibrary(h2);
}

// ============================================================
// Test 2: FreeLibrary harus dipanggil sebanyak LoadLibrary
// ============================================================

static void TestFreeLibraryRefCount() {
    TEST_SECTION("Test 2: FreeLibrary Reference Count");

    std::wstring exeDir = GetExeDir();
    std::wstring dllPath = exeDir + L"\\filters\\x64\\LAVVideo.ax";

    // Simulasi: Load 3x, harus Free 3x
    HMODULE h1 = LoadLibraryW(dllPath.c_str());
    HMODULE h2 = LoadLibraryW(dllPath.c_str());
    HMODULE h3 = LoadLibraryW(dllPath.c_str());

    TEST_ASSERT(h1 == h2 && h2 == h3, "3x LoadLibrary return handle SAMA");

    // Free 3x - seharusnya tidak crash
    FreeLibrary(h1);
    FreeLibrary(h2);
    BOOL freed = FreeLibrary(h3);

    TEST_ASSERT(freed, "FreeLibrary ke-3 berhasil (DLL unloaded)");
    printf("  Handle: %p, %p, %p (semua sama)\n", (void*)h1, (void*)h2, (void*)h3);
}

// ============================================================
// Test 3: Benchmark tanpa caching (load/unload cycle)
// ============================================================

static void TestBenchmarkNoCache() {
    TEST_SECTION("Test 3: Benchmark TANPA Caching (FreeLibrary + LoadLibraryW)");

    std::wstring exeDir = GetExeDir();
    std::wstring dllPaths[] = {
        exeDir + L"\\filters\\x64\\LAVSplitter.ax",
        exeDir + L"\\filters\\x64\\LAVVideo.ax",
        exeDir + L"\\filters\\x64\\LAVAudio.ax",
    };

    const int iterations = 50;
    std::vector<double> timesUs;
    timesUs.reserve(iterations);

    for (int i = 0; i < iterations; i++) {
        auto start = std::chrono::high_resolution_clock::now();

        // Simulasi SEBELUM optimasi: load/unload setiap kali
        HMODULE h[3] = {};
        for (int j = 0; j < 3; j++) {
            h[j] = LoadLibraryW(dllPaths[j].c_str());
        }
        for (int j = 0; j < 3; j++) {
            if (h[j]) FreeLibrary(h[j]);
        }

        auto end = std::chrono::high_resolution_clock::now();
        double us = std::chrono::duration<double, std::micro>(end - start).count();
        timesUs.push_back(us);
    }

    double sum = 0;
    for (double t : timesUs) sum += t;
    double avg = sum / timesUs.size();
    double minT = timesUs[0], maxT = timesUs[0];
    for (double t : timesUs) {
        if (t < minT) minT = t;
        if (t > maxT) maxT = t;
    }

    printf("  Iterations: %d\n", iterations);
    printf("  Rata-rata:  %.1f us (%.2f ms)\n", avg, avg / 1000.0);
    printf("  Minimum:    %.1f us\n", minT);
    printf("  Maximum:    %.1f us\n", maxT);
}

// ============================================================
// Test 4: Benchmark dengan caching (load sekali, reuse)
// ============================================================

static void TestBenchmarkWithCache() {
    TEST_SECTION("Test 4: Benchmark DENGAN Caching (reuse handle)");

    std::wstring exeDir = GetExeDir();
    std::wstring dllPaths[] = {
        exeDir + L"\\filters\\x64\\LAVSplitter.ax",
        exeDir + L"\\filters\\x64\\LAVVideo.ax",
        exeDir + L"\\filters\\x64\\LAVAudio.ax",
    };

    // Load sekali di awal
    HMODULE cached[3] = {};
    for (int j = 0; j < 3; j++) {
        cached[j] = LoadLibraryW(dllPaths[j].c_str());
        TEST_ASSERT(cached[j] != nullptr, "Initial load berhasil");
    }

    const int iterations = 50;
    std::vector<double> timesUs;
    timesUs.reserve(iterations);

    for (int i = 0; i < iterations; i++) {
        auto start = std::chrono::high_resolution_clock::now();

        // Simulasi SESUDAH optimasi: reuse cached handle
        HMODULE h[3] = {};
        for (int j = 0; j < 3; j++) {
            h[j] = cached[j]; // Langsung reuse, skip LoadLibraryW
            // Dalam kode sebenarnya: if (!*pOutModule) h = LoadLibraryW(...)
        }

        auto end = std::chrono::high_resolution_clock::now();
        double us = std::chrono::duration<double, std::micro>(end - start).count();
        timesUs.push_back(us);
    }

    double sum = 0;
    for (double t : timesUs) sum += t;
    double avg = sum / timesUs.size();
    double minT = timesUs[0], maxT = timesUs[0];
    for (double t : timesUs) {
        if (t < minT) minT = t;
        if (t > maxT) maxT = t;
    }

    printf("  Iterations: %d\n", iterations);
    printf("  Rata-rata:  %.1f us (%.2f ms)\n", avg, avg / 1000.0);
    printf("  Minimum:    %.1f us\n", minT);
    printf("  Maximum:    %.1f us\n", maxT);

    // Cleanup
    for (int j = 0; j < 3; j++) {
        if (cached[j]) FreeLibrary(cached[j]);
    }
}

// ============================================================
// Test 5: Benchmark GetProcAddress (tetap diperlukan)
// ============================================================

static void TestGetProcAddressOverhead() {
    TEST_SECTION("Test 5: GetProcAddress Overhead");

    std::wstring exeDir = GetExeDir();
    std::wstring dllPath = exeDir + L"\\filters\\x64\\LAVSplitter.ax";

    HMODULE hDll = LoadLibraryW(dllPath.c_str());
    TEST_ASSERT(hDll != nullptr, "Load LAVSplitter berhasil");

    typedef HRESULT(STDAPICALLTYPE * DllGetClassObjectFunc)(REFCLSID, REFIID, LPVOID*);

    const int iterations = 1000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        auto pfn = (DllGetClassObjectFunc)GetProcAddress(hDll, "DllGetClassObject");
        (void)pfn;
    }

    auto end = std::chrono::high_resolution_clock::now();
    double totalUs = std::chrono::duration<double, std::micro>(end - start).count();
    double perCallNs = (totalUs * 1000.0) / iterations;

    printf("  GetProcAddress x%d: %.1f us total, %.1f ns/call\n",
           iterations, totalUs, perCallNs);

    // GetProcAddress harus cepat (< 1us per call)
    TEST_ASSERT(perCallNs < 1000, "GetProcAddress < 1000 ns/call");

    FreeLibrary(hDll);
}

// ============================================================
// Test 6: Simulasi complete flow (tanpa DirectShow)
// ============================================================

static void TestSimulatedOpenFileCycle() {
    TEST_SECTION("Test 6: Simulated OpenFile Cycle (Before vs After)");

    std::wstring exeDir = GetExeDir();
    std::wstring dllPaths[] = {
        exeDir + L"\\filters\\x64\\LAVSplitter.ax",
        exeDir + L"\\filters\\x64\\LAVVideo.ax",
        exeDir + L"\\filters\\x64\\LAVAudio.ax",
    };

    typedef HRESULT(STDAPICALLTYPE * DllGetClassObjectFunc)(REFCLSID, REFIID, LPVOID*);

    const int fileCount = 10;

    // === SEBELUM: Load/Unload setiap file ===
    auto t1 = std::chrono::high_resolution_clock::now();
    for (int f = 0; f < fileCount; f++) {
        HMODULE h[3] = {};
        for (int j = 0; j < 3; j++) {
            h[j] = LoadLibraryW(dllPaths[j].c_str());
            if (h[j]) {
                auto pfn = (DllGetClassObjectFunc)GetProcAddress(h[j], "DllGetClassObject");
                (void)pfn;
            }
        }
        for (int j = 0; j < 3; j++) {
            if (h[j]) FreeLibrary(h[j]);
        }
    }
    auto t2 = std::chrono::high_resolution_clock::now();

    // === SESUDAH: Cache handle ===
    HMODULE cached[3] = {};
    for (int j = 0; j < 3; j++) {
        cached[j] = LoadLibraryW(dllPaths[j].c_str());
    }

    auto t3 = std::chrono::high_resolution_clock::now();
    for (int f = 0; f < fileCount; f++) {
        HMODULE h[3] = {};
        for (int j = 0; j < 3; j++) {
            h[j] = cached[j]; // Reuse cached
            if (h[j]) {
                auto pfn = (DllGetClassObjectFunc)GetProcAddress(h[j], "DllGetClassObject");
                (void)pfn;
            }
        }
        // Tidak FreeLibrary di sini (dipindah ke Shutdown)
    }
    auto t4 = std::chrono::high_resolution_clock::now();

    // Cleanup
    for (int j = 0; j < 3; j++) {
        if (cached[j]) FreeLibrary(cached[j]);
    }

    double beforeUs = std::chrono::duration<double, std::micro>(t2 - t1).count();
    double afterUs = std::chrono::duration<double, std::micro>(t4 - t3).count();
    double savedUs = beforeUs - afterUs;
    double savedPct = (savedUs / beforeUs) * 100.0;

    printf("  %d file opens:\n", fileCount);
    printf("    SEBELUM (load/unload): %.1f us (%.2f ms)\n", beforeUs, beforeUs / 1000.0);
    printf("    SESUDAH (cached):      %.1f us (%.2f ms)\n", afterUs, afterUs / 1000.0);
    printf("    Penghematan:           %.1f us (%.1f%%)\n", savedUs, savedPct);

    TEST_ASSERT(afterUs < beforeUs, "Cached version LEBIH CEPAT");
    TEST_ASSERT(savedPct > 10.0, "Penghematan > 10%");
}

// ============================================================
// Test 7: FreeFilterModules behavior
// ============================================================

static void TestFreeFilterModulesBehavior() {
    TEST_SECTION("Test 7: FreeFilterModules Behavior");

    std::wstring exeDir = GetExeDir();
    std::wstring dllPath = exeDir + L"\\filters\\x64\\LAVSplitter.ax";

    // Load
    HMODULE h = LoadLibraryW(dllPath.c_str());
    TEST_ASSERT(h != nullptr, "Load berhasil");

    // Simulasi: handle di-cache
    HMODULE cached = h;

    // Dalam DestroyGraph: JANGAN FreeLibrary (handle tetap)
    // Dalam FreeFilterModules: FreeLibrary
    // Simulasi:
    //   DestroyGraph: (nothing)
    //   FreeFilterModules: FreeLibrary(cached)

    // Verify handle masih valid setelah "DestroyGraph" (tanpa FreeLibrary)
    HMODULE test = LoadLibraryW(dllPath.c_str());
    TEST_ASSERT(test == cached, "Handle masih valid tanpa FreeLibrary");
    FreeLibrary(test); // undo extra ref

    // Free via FreeFilterModules simulation
    BOOL freed = FreeLibrary(cached);
    TEST_ASSERT(freed, "FreeFilterModules: FreeLibrary berhasil");

    // Setelah Free, LoadLibrary baru harus return baru atau same
    HMODULE after = LoadLibraryW(dllPath.c_str());
    TEST_ASSERT(after != nullptr, "LoadLibrary setelah Free berhasil");
    // Note: bisa sama atau beda, tergantung OS
    FreeLibrary(after);
}

// ============================================================
// Test 8: VSFilter fallback (VSFilter.dll vs xy-VSFilter.dll)
// ============================================================

static void TestVSFilterFallback() {
    TEST_SECTION("Test 8: VSFilter Fallback Behavior");

    std::wstring exeDir = GetExeDir();
    std::wstring vsPath1 = exeDir + L"\\filters\\x64\\VSFilter.dll";
    std::wstring vsPath2 = exeDir + L"\\filters\\x64\\xy-VSFilter.dll";

    // Cek apakah file ada
    DWORD attr1 = GetFileAttributesW(vsPath1.c_str());
    DWORD attr2 = GetFileAttributesW(vsPath2.c_str());

    bool exists1 = (attr1 != INVALID_FILE_ATTRIBUTES);
    bool exists2 = (attr2 != INVALID_FILE_ATTRIBUTES);

    printf("  VSFilter.dll exists:    %s\n", exists1 ? "YES" : "NO");
    printf("  xy-VSFilter.dll exists: %s\n", exists2 ? "YES" : "NO");

    if (exists1) {
        HMODULE h1 = LoadLibraryW(vsPath1.c_str());
        TEST_ASSERT(h1 != nullptr, "VSFilter.dll load berhasil");
        if (h1) FreeLibrary(h1);
    }

    if (exists2) {
        HMODULE h2 = LoadLibraryW(vsPath2.c_str());
        TEST_ASSERT(h2 != nullptr, "xy-VSFilter.dll load berhasil");
        if (h2) FreeLibrary(h2);
    }

    // Test: Jika VSFilter.dll di-cache lalu xy-VSFilter.dll dipanggil
    // handle HARUSNYA berbeda (path berbeda)
    if (exists1 && exists2) {
        HMODULE h1 = LoadLibraryW(vsPath1.c_str());
        HMODULE h2 = LoadLibraryW(vsPath2.c_str());

        // Karena path berbeda, handle mungkin berbeda
        // Tapi untuk test ini, kita pastikan keduanya bisa di-load
        TEST_ASSERT(h1 != nullptr && h2 != nullptr, "Keduanya bisa di-load");

        FreeLibrary(h1);
        FreeLibrary(h2);
    }
}

// ============================================================

int main() {
    printf("============================================================\n");
    printf("  Vidi - LAV Module Cache Test Suite\n");
    printf("============================================================\n");

    TestLoadLibraryCaching();
    TestFreeLibraryRefCount();
    TestBenchmarkNoCache();
    TestBenchmarkWithCache();
    TestGetProcAddressOverhead();
    TestSimulatedOpenFileCycle();
    TestFreeFilterModulesBehavior();
    TestVSFilterFallback();

    printf("\n============================================================\n");
    printf("  Results: %d passed, %d failed\n", g_testsPassed, g_testsFailed);
    printf("============================================================\n");

    return g_testsFailed > 0 ? 1 : 0;
}

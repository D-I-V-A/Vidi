// ============================================================
// Vidi - Video Open Integration Test
// Test LAV module caching dengan video asli
// ============================================================

#include <windows.h>
#include <dshow.h>
#include <cstdio>
#include <cstdint>
#include <chrono>
#include <string>
#include <vector>
#include <algorithm>
#include <random>
#include <fstream>
#include <sstream>
#include <iomanip>

// ============================================================
// CLSID LAV Filters (dari directShowPlayer.cc)
// ============================================================

static const CLSID CLSID_LAVSplitterSource = {
    0xB98D13E7, 0x55DB, 0x4385, {0xA3, 0x3D, 0x09, 0xFD, 0x1B, 0xA2, 0x63, 0x38}};
static const CLSID CLSID_LAVVideo = {
    0xEE30215D, 0x164F, 0x4A92, {0xA4, 0xEB, 0x9D, 0x4C, 0x13, 0x39, 0x0F, 0x9F}};
static const CLSID CLSID_LAVAudio = {
    0xE8E73B6B, 0x4CB3, 0x44A4, {0xBE, 0x99, 0x4F, 0x7B, 0xCB, 0x96, 0xE4, 0x91}};

// ============================================================
// Test Framework
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
// Helpers
// ============================================================

static std::wstring GetExeDir() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring full(path);
    size_t pos = full.find_last_of(L"\\/");
    return (pos != std::wstring::npos) ? full.substr(0, pos) : L".";
}

static std::string WStrToStr(const std::wstring& wstr) {
    if (wstr.empty())
        return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &result[0], size, nullptr, nullptr);
    return result;
}

// ============================================================
// Env File Parser (env.dev)
// ============================================================

struct EnvConfig {
    std::wstring videoDir;
};

static EnvConfig LoadEnvDev() {
    EnvConfig config;
    std::wstring exeDir = GetExeDir();
    std::wstring envPath = exeDir + L"\\..\\..\\tests\\env.dev";

    // Coba beberapa lokasi
    std::vector<std::wstring> candidates = {
        envPath,                                              // build/bin/../../tests/env.dev
        exeDir + L"\\..\\tests\\env.dev",                     // build/tests/../tests/env.dev
        exeDir + L"\\tests\\env.dev",                         // tests/env.dev (flat)
        GetExeDir().substr(0, GetExeDir().find_last_of(L"\\")) + L"\\tests\\env.dev", // project root/tests/env.dev
    };

    std::ifstream file;
    for (auto& path : candidates) {
        // Konversi wstring ke char path untuk ifstream
        int sz = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string charPath(sz, 0);
        WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, &charPath[0], sz, nullptr, nullptr);
        charPath.resize(strlen(charPath.c_str()));

        file.open(charPath);
        if (file.is_open()) {
            printf("  Config: %s\n", charPath.c_str());
            break;
        }
    }

    if (!file.is_open()) {
        printf("  WARNING: env.dev not found, using default path\n");
        config.videoDir = L"D:\\Anime\\[KS] Sword Art Online BD 720P";
        return config;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines dan comments
        if (line.empty() || line[0] == '#')
            continue;

        // Parse KEY=VALUE
        size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;

        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        // Trim whitespace
        auto trim = [](std::string& s) {
            s.erase(0, s.find_first_not_of(" \t\r\n"));
            s.erase(s.find_last_not_of(" \t\r\n") + 1);
        };
        trim(key);
        trim(value);

        if (key == "VIDI_TEST_VIDEO_DIR") {
            // Convert UTF-8 to wstring
            int wsize = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
            config.videoDir.resize(wsize);
            MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, &config.videoDir[0], wsize);
            config.videoDir.resize(wcslen(config.videoDir.c_str()));
        }
    }

    if (config.videoDir.empty()) {
        printf("  WARNING: VIDI_TEST_VIDEO_DIR not set, using default\n");
        config.videoDir = L"D:\\Anime\\[KS] Sword Art Online BD 720P";
    }

    return config;
}

// ============================================================
// Video Scanner
// ============================================================

struct VideoFile {
    std::wstring path;
    std::wstring name;
    DWORD sizeMB;
};

static std::vector<VideoFile> ScanVideoFolder(const std::wstring& dir) {
    std::vector<VideoFile> videos;
    std::wstring searchDir = dir + L"\\*";

    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchDir.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE)
        return videos;

    do {
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;

        std::wstring name = findData.cFileName;
        // Filter: .mkv, .mp4, .avi
        std::wstring ext = name;
        std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
        if (ext.find(L".mkv") == std::wstring::npos &&
            ext.find(L".mp4") == std::wstring::npos &&
            ext.find(L".avi") == std::wstring::npos)
            continue;

        VideoFile vf;
        vf.path = dir + L"\\" + name;
        vf.name = name;
        vf.sizeMB = (findData.nFileSizeHigh * (MAXDWORD + 1) + findData.nFileSizeLow) / (1024 * 1024);
        videos.push_back(vf);
    } while (FindNextFileW(hFind, &findData));

    FindClose(hFind);

    // Sort by name
    std::sort(videos.begin(), videos.end(), [](const VideoFile& a, const VideoFile& b) {
        return a.name < b.name;
    });

    return videos;
}

// ============================================================
// DirectShow Helper: Load Filter
// ============================================================

typedef HRESULT(STDAPICALLTYPE * DllGetClassObjectFunc)(REFCLSID, REFIID, LPVOID*);

static IBaseFilter* LoadLavFilter(const wchar_t* dllPath, REFCLSID clsid) {
    HMODULE hDll = LoadLibraryW(dllPath);
    if (!hDll)
        return nullptr;

    DllGetClassObjectFunc pDllGetClassObject =
        (DllGetClassObjectFunc)GetProcAddress(hDll, "DllGetClassObject");
    if (!pDllGetClassObject) {
        FreeLibrary(hDll);
        return nullptr;
    }

    IClassFactory* pFactory = nullptr;
    HRESULT hr = pDllGetClassObject(clsid, IID_IClassFactory, (void**)&pFactory);
    if (FAILED(hr) || !pFactory) {
        FreeLibrary(hDll);
        return nullptr;
    }

    IBaseFilter* pFilter = nullptr;
    hr = pFactory->CreateInstance(nullptr, IID_IBaseFilter, (void**)&pFilter);
    pFactory->Release();

    if (FAILED(hr) || !pFilter) {
        FreeLibrary(hDll);
        return nullptr;
    }

    return pFilter;
}

// ============================================================
// Timing Struct
// ============================================================

struct TimingResult {
    double dllLoadMs;
    double graphBuildMs;
    double fileOpenMs;
    double totalMs;
    std::wstring videoName;
};

// Global: simpan cold load time dari Test 1
static double g_coldLoadTimeMs = 0.0;

// ============================================================
// Test 1: LoadLibrary Caching (Unit Test)
// ============================================================

static void TestLoadLibraryCaching() {
    TEST_SECTION("Test 1: LoadLibrary Caching Behavior");

    std::wstring exeDir = GetExeDir();
    std::wstring dllPath = exeDir + L"\\filters\\x64\\LAVSplitter.ax";

    // Load pertama (cold)
    auto t1 = std::chrono::high_resolution_clock::now();
    HMODULE h1 = LoadLibraryW(dllPath.c_str());
    auto t2 = std::chrono::high_resolution_clock::now();

    TEST_ASSERT(h1 != nullptr, "LoadLibraryW pertama berhasil");

    // Load kedua (warm)
    auto t3 = std::chrono::high_resolution_clock::now();
    HMODULE h2 = LoadLibraryW(dllPath.c_str());
    auto t4 = std::chrono::high_resolution_clock::now();

    TEST_ASSERT(h2 != nullptr, "LoadLibraryW kedua berhasil");
    TEST_ASSERT(h1 == h2, "Handle SAMA (cached)");

    auto loadFirst = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();
    auto loadSecond = std::chrono::duration_cast<std::chrono::nanoseconds>(t4 - t3).count();

    // Simpan cold load time untuk comparison (1 DLL)
    g_coldLoadTimeMs = loadFirst / 1e6;

    printf("  Cold load: %lld ns (%.2f ms)\n", loadFirst, loadFirst / 1e6);
    printf("  Warm load: %lld ns (%.3f ms)\n", loadSecond, loadSecond / 1e6);

    FreeLibrary(h1);
    FreeLibrary(h2);
}

// ============================================================
// Test 2: GetProcAddress Overhead
// ============================================================

static void TestGetProcAddressOverhead() {
    TEST_SECTION("Test 2: GetProcAddress Overhead");

    std::wstring exeDir = GetExeDir();
    std::wstring dllPath = exeDir + L"\\filters\\x64\\LAVSplitter.ax";

    HMODULE hDll = LoadLibraryW(dllPath.c_str());
    TEST_ASSERT(hDll != nullptr, "Load LAVSplitter");

    const int iterations = 1000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        auto pfn = (DllGetClassObjectFunc)GetProcAddress(hDll, "DllGetClassObject");
        (void)pfn;
    }

    auto end = std::chrono::high_resolution_clock::now();
    double totalNs = std::chrono::duration<double, std::nano>(end - start).count();
    double perCallNs = totalNs / iterations;

    printf("  GetProcAddress x%d: %.1f ns/call\n", iterations, perCallNs);
    TEST_ASSERT(perCallNs < 1000, "GetProcAddress < 1000 ns/call");

    FreeLibrary(hDll);
}

// ============================================================
// Test 3: Single Video Open
// ============================================================

static TimingResult TestSingleVideoOpen(const VideoFile& video, bool cached) {
    TimingResult result = {};
    result.videoName = video.name;

    std::wstring exeDir = GetExeDir();
    std::wstring splitterPath = exeDir + L"\\filters\\x64\\LAVSplitter.ax";
    std::wstring videoPath = video.path;

    auto totalStart = std::chrono::high_resolution_clock::now();

    // === DLL Load ===
    auto t1 = std::chrono::high_resolution_clock::now();
    HMODULE hSplitter = LoadLibraryW(splitterPath.c_str());
    HMODULE hVideo = LoadLibraryW((exeDir + L"\\filters\\x64\\LAVVideo.ax").c_str());
    HMODULE hAudio = LoadLibraryW((exeDir + L"\\filters\\x64\\LAVAudio.ax").c_str());
    auto t2 = std::chrono::high_resolution_clock::now();

    result.dllLoadMs = std::chrono::duration<double, std::milli>(t2 - t1).count();

    if (!hSplitter || !hVideo || !hAudio) {
        printf("  ERROR: Failed to load LAV DLLs\n");
        if (hSplitter) FreeLibrary(hSplitter);
        if (hVideo) FreeLibrary(hVideo);
        if (hAudio) FreeLibrary(hAudio);
        return result;
    }

    // === COM Init ===
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // === Build Graph ===
    auto t3 = std::chrono::high_resolution_clock::now();

    IGraphBuilder* pGraph = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_IGraphBuilder, (void**)&pGraph);
    if (FAILED(hr) || !pGraph) {
        printf("  ERROR: CreateFilterGraph failed\n");
        CoUninitialize();
        FreeLibrary(hSplitter);
        FreeLibrary(hVideo);
        FreeLibrary(hAudio);
        return result;
    }

    // Load splitter filter
    IBaseFilter* pSplitter = LoadLavFilter(splitterPath.c_str(), CLSID_LAVSplitterSource);
    if (pSplitter) {
        pGraph->AddFilter(pSplitter, L"LAV Splitter Source");
    }

    // Load video decoder
    IBaseFilter* pVideo = LoadLavFilter((exeDir + L"\\filters\\x64\\LAVVideo.ax").c_str(), CLSID_LAVVideo);
    if (pVideo) {
        pGraph->AddFilter(pVideo, L"LAV Video Decoder");
    }

    // Load audio decoder
    IBaseFilter* pAudio = LoadLavFilter((exeDir + L"\\filters\\x64\\LAVAudio.ax").c_str(), CLSID_LAVAudio);
    if (pAudio) {
        pGraph->AddFilter(pAudio, L"LAV Audio Decoder");
    }

    auto t4 = std::chrono::high_resolution_clock::now();
    result.graphBuildMs = std::chrono::duration<double, std::milli>(t4 - t3).count();

    // === File Open ===
    auto t5 = std::chrono::high_resolution_clock::now();

    if (pSplitter) {
        IFileSourceFilter* pFileSource = nullptr;
        hr = pSplitter->QueryInterface(IID_IFileSourceFilter, (void**)&pFileSource);
        if (SUCCEEDED(hr) && pFileSource) {
            hr = pFileSource->Load(videoPath.c_str(), nullptr);
            pFileSource->Release();
        }
    }

    auto t6 = std::chrono::high_resolution_clock::now();
    result.fileOpenMs = std::chrono::duration<double, std::milli>(t6 - t5).count();

    auto totalEnd = std::chrono::high_resolution_clock::now();
    result.totalMs = std::chrono::duration<double, std::milli>(totalEnd - totalStart).count();

    // === Cleanup: Release COM objects, but DON'T FreeLibrary (cached) ===
    if (pAudio) pAudio->Release();
    if (pVideo) pVideo->Release();
    if (pSplitter) pSplitter->Release();
    if (pGraph) pGraph->Release();

    CoUninitialize();

    // Cached: keep DLLs loaded
    if (!cached) {
        FreeLibrary(hSplitter);
        FreeLibrary(hVideo);
        FreeLibrary(hAudio);
    }

    return result;
}

// ============================================================
// Test 4: Benchmark Multiple Opens
// ============================================================

struct BenchmarkResult {
    int openCount;
    std::vector<TimingResult> timings;
    double totalMs;
    double avgMs;
};

static BenchmarkResult RunBenchmark(const std::vector<VideoFile>& allVideos, int openCount) {
    BenchmarkResult bench = {};
    bench.openCount = openCount;

    // Pick random videos
    std::vector<VideoFile> selected;
    std::vector<int> indices(allVideos.size());
    for (int i = 0; i < (int)allVideos.size(); i++)
        indices[i] = i;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(indices.begin(), indices.end(), gen);

    for (int i = 0; i < openCount && i < (int)allVideos.size(); i++) {
        selected.push_back(allVideos[indices[i]]);
    }

    // Load DLLs sekali di awal (cached)
    std::wstring exeDir = GetExeDir();
    HMODULE hSplitter = LoadLibraryW((exeDir + L"\\filters\\x64\\LAVSplitter.ax").c_str());
    HMODULE hVideo = LoadLibraryW((exeDir + L"\\filters\\x64\\LAVVideo.ax").c_str());
    HMODULE hAudio = LoadLibraryW((exeDir + L"\\filters\\x64\\LAVAudio.ax").c_str());

    bench.totalMs = 0;

    for (int i = 0; i < openCount; i++) {
        auto result = TestSingleVideoOpen(selected[i], true);
        bench.timings.push_back(result);
        bench.totalMs += result.totalMs;
    }

    bench.avgMs = bench.totalMs / openCount;

    // Cleanup cached DLLs
    if (hSplitter) FreeLibrary(hSplitter);
    if (hVideo) FreeLibrary(hVideo);
    if (hAudio) FreeLibrary(hAudio);

    return bench;
}

// ============================================================
// Main
// ============================================================

int main() {
    printf("============================================================\n");
    printf("  Vidi - Video Open Integration Test\n");
    printf("============================================================\n");

    // === Load Config ===
    TEST_SECTION("Loading Configuration");
    EnvConfig config = LoadEnvDev();
    printf("  VIDI_TEST_VIDEO_DIR = %ls\n", config.videoDir.c_str());

    // === Scan Videos ===
    TEST_SECTION("Scanning Video Folder");
    auto videos = ScanVideoFolder(config.videoDir);
    printf("  Found %zu video files\n", videos.size());

    if (videos.empty()) {
        printf("\n  ERROR: No video files found in %ls\n", config.videoDir.c_str());
        printf("  Please check VIDI_TEST_VIDEO_DIR in env.dev\n");
        return 1;
    }

    // Tampilkan beberapa video
    int showCount = (videos.size() < 5) ? (int)videos.size() : 5;
    for (int i = 0; i < showCount; i++) {
        printf("    [%d] %ls (%lu MB)\n", i + 1, videos[i].name.c_str(), videos[i].sizeMB);
    }
    if (videos.size() > 5) {
        printf("    ... and %zu more\n", videos.size() - 5);
    }

    // === Unit Tests ===
    TestLoadLibraryCaching();
    TestGetProcAddressOverhead();

    // === Single Video Test ===
    TEST_SECTION("Single Video Open Test");
    {
        // Pick first video
        auto& video = videos[0];
        printf("  Video: %ls (%lu MB)\n", video.name.c_str(), video.sizeMB);

        // 5 iterasi
        std::vector<TimingResult> results;
        for (int i = 0; i < 5; i++) {
            bool cached = (i > 0); // First run cold, rest warm
            auto result = TestSingleVideoOpen(video, cached);
            results.push_back(result);

            printf("  Run %d (%s): DLL=%.1fms Graph=%.1fms Open=%.1fms Total=%.1fms\n",
                   i + 1, cached ? "warm " : "cold",
                   result.dllLoadMs, result.graphBuildMs,
                   result.fileOpenMs, result.totalMs);
        }

        double coldTotal = results[0].totalMs;
        double warmAvg = 0;
        for (int i = 1; i < 5; i++)
            warmAvg += results[i].totalMs;
        warmAvg /= 4.0;

        double saved = coldTotal - warmAvg;
        double savedPct = (saved / coldTotal) * 100.0;

        printf("\n  Cold cache avg: %.1f ms\n", coldTotal);
        printf("  Warm cache avg: %.1f ms\n", warmAvg);
        printf("  Time saved:     %.1f ms per open (%.1f%%)\n", saved, savedPct);
        TEST_ASSERT(warmAvg < coldTotal, "Warm cache LEBIH CEPAT dari cold cache");
    }

    // === Multi-Video Benchmark ===
    TEST_ASSERT(videos.size() >= 5, "Minimal 5 video untuk benchmark");
    if (videos.size() < 5) {
        printf("\n  Skipping multi-video benchmark (need at least 5 videos)\n");
    } else {
        int testCounts[] = {5, 10, 15, 20};
        std::vector<BenchmarkResult> allResults;

        for (int count : testCounts) {
            if (count > (int)videos.size())
                continue;

            char section[64];
            sprintf_s(section, "Benchmark: %d Open Video", count);
            TEST_SECTION(section);

            auto bench = RunBenchmark(videos, count);

            // Tampilkan hasil per video
            for (int i = 0; i < (int)bench.timings.size(); i++) {
                printf("  [%2d] %ls → %.1fms\n",
                       i + 1, bench.timings[i].videoName.c_str(), bench.timings[i].totalMs);
            }

            printf("\n  Total:   %.1f ms\n", bench.totalMs);
            printf("  Average: %.1f ms per open\n", bench.avgMs);

            allResults.push_back(bench);
        }

        // === Comparison: Cached vs Non-Cached (Theoretical) ===
        if (!allResults.empty()) {
            TEST_SECTION("Comparison: Cached vs Non-Cached (Theoretical)");

            // Gunakan cold load time dari Test 1 (3 DLLs = g_coldLoadTimeMs * 3)
            double coldLoadTimeMs = g_coldLoadTimeMs * 3.0;

            printf("  Cold LoadLibraryW time: %.1f ms (3 DLLs)\n", coldLoadTimeMs);
            printf("\n");
            printf("  | %-12s | %-12s | %-12s | %-12s |\n", "Open Count", "Cached", "Non-Cached", "Saved");
            printf("  | %-12s | %-12s | %-12s | %-12s |\n", "------------", "------------", "------------", "------------");

            for (auto& bench : allResults) {
                // Non-cached = cached time + (coldLoadTime * openCount)
                double nonCachedMs = bench.totalMs + (coldLoadTimeMs * bench.openCount);
                double savedMs = nonCachedMs - bench.totalMs;
                double savedPct = (savedMs / nonCachedMs) * 100.0;

                printf("  | %-12d | %8.1f ms | %8.1f ms | %8.1f ms |\n",
                       bench.openCount, bench.totalMs, nonCachedMs, savedMs);
            }
            printf("\n");
        }
    }

    // === Summary ===
    printf("\n============================================================\n");
    printf("  Results: %d passed, %d failed\n", g_testsPassed, g_testsFailed);
    printf("============================================================\n");

    return g_testsFailed > 0 ? 1 : 0;
}

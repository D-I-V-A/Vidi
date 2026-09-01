#include "../../include/kernels/subtitleReader.hh"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cstdarg>
#include <cmath>

namespace kernelPlayerVidi {

// ============================================================
// FFmpeg constants
// ============================================================

#ifndef AV_TIME_BASE
#define AV_TIME_BASE 1000000
#endif

#ifndef AV_NOPTS_VALUE
#define AV_NOPTS_VALUE ((int64_t)UINT64_C(0x8000000000000000))
#endif

// ============================================================
// AVPacket layout for LAV-patched FFmpeg (avcodec-lav-62)
// ============================================================

struct AVPacketRaw {
    void* buf;
    int64_t pts;
    int64_t dts;
    uint8_t* data;
    int size;
    int stream_index;
    int flags;
    int64_t duration;
    int64_t pos;
    char _pad[256];
};

// ============================================================
// Helpers
// ============================================================

static void VSubLog(const wchar_t* fmt, ...) {
    wchar_t buf[512];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf_s(buf, _TRUNCATE, fmt, ap);
    va_end(ap);
    wcscat_s(buf, L"\n");
    OutputDebugStringW(buf);

    wchar_t lp[MAX_PATH];
    if (GetTempPathW(MAX_PATH, lp)) {
        wcscat_s(lp, L"vidi_debug.log");
        FILE* f = nullptr;
        if (_wfopen_s(&f, lp, L"a") == 0 && f) {
            SYSTEMTIME st;
            GetLocalTime(&st);
            fwprintf(f, L"[%02u:%02u:%02u.%03u] %s", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, buf);
            fclose(f);
        }
    }
}

static std::wstring Utf8ToWide(const char* utf8) {
    if (!utf8 || !*utf8)
        return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    if (len <= 0)
        return L"";
    std::wstring w(len - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &w[0], len);
    return w;
}

static std::string WideToUtf8(const wchar_t* wide) {
    if (!wide || !*wide)
        return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0)
        return "";
    std::string s(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, &s[0], len, nullptr, nullptr);
    return s;
}

static std::wstring GetExeDir() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring full(path);
    size_t pos = full.find_last_of(L"\\/");
    return (pos != std::wstring::npos) ? full.substr(0, pos) : L".";
}

static bool LooksLikeSubtitleText(const uint8_t* data, int size) {
    if (!data || size < 4)
        return false;

    // Reject H.264/HEVC NAL start codes
    if (data[0] == 0 && data[1] == 0 && (data[2] == 1 || (data[2] == 0 && data[3] == 1)))
        return false;

    // Reject packets with too many null bytes
    int nullCount = 0;
    for (int i = 0; i < size && i < 32; ++i) {
        if (data[i] == 0)
            nullCount++;
    }
    if (nullCount > 2)
        return false;

    // Strict UTF-8 check — reject binary video/audio data
    int printable = 0;
    int checked = 0;
    int i = 0;
    while (i < size && i < 256) {
        uint8_t c = data[i];
        if (c >= 0x20 && c < 0x7F) {
            printable++;
            checked++;
            i++;
        } else if (c == '\n' || c == '\r' || c == '\t') {
            printable++;
            checked++;
            i++;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 >= size || (data[i + 1] & 0xC0) != 0x80)
                return false;
            printable += 2;
            checked += 2;
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 >= size || (data[i + 1] & 0xC0) != 0x80 || (data[i + 2] & 0xC0) != 0x80)
                return false;
            printable += 3;
            checked += 3;
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 >= size || (data[i + 1] & 0xC0) != 0x80 || (data[i + 2] & 0xC0) != 0x80 ||
                (data[i + 3] & 0xC0) != 0x80)
                return false;
            printable += 4;
            checked += 4;
            i += 4;
        } else {
            return false; // invalid UTF-8 byte
        }
    }

    if (checked < 4)
        return false;
    return (printable > checked * 7 / 10);
}

static std::wstring StripASSTags(const char* text) {
    if (!text)
        return L"";

    std::string s(text);
    std::string out;
    out.reserve(s.size());

    bool inTag = false;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '{')
            inTag = true;
        else if (s[i] == '}') {
            inTag = false;
            continue;
        } else if (!inTag)
            out += s[i];
    }

    std::wstring result = Utf8ToWide(out.c_str());
    std::wstring::size_type pos = 0;
    while ((pos = result.find(L"\\N", pos)) != std::wstring::npos) {
        result.replace(pos, 2, L"\n");
        pos += 1;
    }
    pos = 0;
    while ((pos = result.find(L"\\n", pos)) != std::wstring::npos) {
        result.replace(pos, 2, L"\n");
        pos += 1;
    }
    return result;
}

// Parse \pos(x,y) dari text ASS — return true jika ditemukan
static bool ExtractPosTag(const std::string& text, double& outX, double& outY) {
    // Cari \pos( atau \pos ( di dalam {...} tags
    size_t pos = 0;
    while (pos < text.size()) {
        size_t tagStart = text.find("\\pos(", pos);
        size_t prefixLen = 5; // panjang "\pos("
        if (tagStart == std::string::npos) {
            tagStart = text.find("\\pos (", pos);
            prefixLen = 6;
            if (tagStart == std::string::npos)
                return false;
        }
        size_t argsStart = tagStart + prefixLen;
        size_t parenEnd = text.find(')', argsStart);
        if (parenEnd == std::string::npos) {
            pos = argsStart;
            continue;
        }
        std::string args = text.substr(argsStart, parenEnd - argsStart);
        // Coba tanpa spasi dulu (kasus paling umum)
        if (sscanf_s(args.c_str(), "%lf,%lf", &outX, &outY) == 2)
            return true;
        // Fallback: handle spasi di sekitar koma
        if (sscanf_s(args.c_str(), "%lf , %lf", &outX, &outY) == 2)
            return true;
        pos = parenEnd + 1;
    }
    return false;
}

static int ExtractAnTag(const std::string& text) {
    size_t pos = text.find("\\an");
    if (pos == std::string::npos)
        return 0;
    pos += 3;
    if (pos < text.size() && text[pos] >= '1' && text[pos] <= '9')
        return text[pos] - '0';
    return 0;
}

static int ExtractFsTag(const std::string& text) {
    size_t pos = 0;
    while (pos < text.size()) {
        size_t p = text.find("\\fs", pos);
        if (p == std::string::npos)
            return 0;
        p += 3;
        // check bukan \fscx \fscy \fsp
        if (p < text.size() && text[p] != 'c' && text[p] != 'p' && text[p] >= '0' && text[p] <= '9')
            return atoi(text.c_str() + p);
        pos = p + 1;
    }
    return 0;
}

// Parse ASS dialogue — 2 format:
// 1. Full: "Dialogue: Layer,Start,End,Style,Name,ML,MR,MV,Effect,Text"
// 2. MKV:  "ReadOrder,Layer,Style,Name,ML,MR,MV,Effect,Text" (timestamps dari PTS)
// Penting: text BISA mengandung koma (dalam \pos, \move, dll)
static bool ParseASSDialogue(const std::string& line, double ptsSec, double& outStartSec, double& outEndSec,
                             std::wstring& outText, double& outPosX, double& outPosY, int& outAlignment,
                             int& outFontSize) {
    bool isFullFormat = (line.compare(0, 9, "Dialogue:") == 0);
    int textAfterComma = isFullFormat ? 9 : 8; // text starts after Nth comma
    // Cari posisi koma ke-N
    size_t pos = isFullFormat ? 10 : 0;
    for (int i = 0; i < textAfterComma; ++i) {
        pos = line.find(',', pos);
        if (pos == std::string::npos)
            return false;
        pos++; // skip comma
    }
    // pos sekarang menunjuk awal text
    if (pos >= line.size())
        return false;

    std::string textRaw = line.substr(pos);

    if (isFullFormat) {
        // Full ASS: parse Start dan End
        size_t p1 = 10;
        size_t c1 = line.find(',', p1);
        if (c1 == std::string::npos)
            return false;
        size_t c2 = line.find(',', c1 + 1);
        if (c2 == std::string::npos)
            return false;
        size_t c3 = line.find(',', c2 + 1);
        if (c3 == std::string::npos)
            return false;

        std::string startStr = line.substr(c1 + 1, c2 - c1 - 1);
        std::string endStr = line.substr(c2 + 1, c3 - c2 - 1);

        auto parseTime = [](const std::string& s) -> double {
            int h = 0, m = 0;
            double sec = 0;
            if (sscanf_s(s.c_str(), "%d:%d:%lf", &h, &m, &sec) >= 2)
                return h * 3600.0 + m * 60.0 + sec;
            return -1;
        };

        outStartSec = parseTime(startStr);
        outEndSec = parseTime(endStr);
        if (outStartSec < 0 || outEndSec < 0)
            return false;
    } else {
        // MKV: timestamps dari packet PTS
        outStartSec = ptsSec;
        outEndSec = ptsSec + 5.0; // default, di-update oleh index sorting
    }

    // Extract \pos(x,y) SEBELUM strip tags
    outPosX = -1;
    outPosY = -1;
    bool posFound = ExtractPosTag(textRaw, outPosX, outPosY);
    // Debug: log semua entry yang mengandung \pos(
    {
        size_t hasPos = textRaw.find("\\pos(");
        if (hasPos == std::string::npos)
            hasPos = textRaw.find("\\pos (");
        if (hasPos != std::string::npos) {
            if (posFound) {
                VSubLog(L"[VIDI] Sub: \\pos OK (%.0f,%.0f) raw=[%hs]", outPosX, outPosY, textRaw.c_str());
            } else {
                VSubLog(L"[VIDI] Sub: \\pos FAILED raw=[%hs]", textRaw.c_str());
            }
        }
    }
    // Extract \an alignment SEBELUM strip tags
    outAlignment = ExtractAnTag(textRaw);
    outFontSize = ExtractFsTag(textRaw);
    // Debug: log entries dengan font size != 0
    if (outFontSize > 0) {
        VSubLog(L"[VIDI] Sub: \\fs%d raw=[%hs]", outFontSize, textRaw.c_str());
    }
    outText = StripASSTags(textRaw.c_str());
    return !outText.empty();
}

static double DetectBestTimebase(int64_t maxPts) {
    if (maxPts <= 0)
        return 1000.0;
    // MKV always uses nanoseconds for timestamps
    // Verify: if maxPts / 1000000000 gives a reasonable time (1s - 14400s = 4 hours), use it
    double t1e9 = (double)maxPts / 1000000000.0;
    if (t1e9 >= 1.0 && t1e9 <= 14400.0) {
        VSubLog(L"[VIDI] Sub: maxPts=%lld, using nanoseconds, maxTime=%.1fs", maxPts, t1e9);
        return 1000000000.0;
    }
    // Fallback: try other common timebases
    double candidates[] = {1.0, 100.0, 1000.0, 10000.0, 100000.0, 1000000.0, 10000000.0, 100000000.0};
    double bestDivisor = 1000000000.0;
    double bestScore = 1e18;
    for (double d : candidates) {
        double t = (double)maxPts / d;
        if (t >= 10.0 && t <= 14400.0) {
            double score = fabs(t - 1200.0);
            if (score < bestScore) {
                bestScore = score;
                bestDivisor = d;
            }
        }
    }
    VSubLog(L"[VIDI] Sub: maxPts=%lld, fallback divisor=%.0f, maxTime=%.1fs", maxPts, bestDivisor,
            (double)maxPts / bestDivisor);
    return bestDivisor;
}

// ============================================================
// Load FFmpeg DLLs
// ============================================================

bool SubtitleReader::LoadFFmpegDlls() {
    if (m_dllsLoaded)
        return true;

    std::wstring dir = GetExeDir();
    m_hAvFormatDll = LoadLibraryW((dir + L"\\filters\\x64\\avformat-lav-62.dll").c_str());
    m_hAvCodecDll = LoadLibraryW((dir + L"\\filters\\x64\\avcodec-lav-62.dll").c_str());
    m_hAvUtilDll = LoadLibraryW((dir + L"\\filters\\x64\\avutil-lav-60.dll").c_str());

    if (!m_hAvFormatDll || !m_hAvCodecDll || !m_hAvUtilDll) {
        VSubLog(L"[VIDI] Sub: gagal load FFmpeg DLL (avformat=%p avcodec=%p avutil=%p)", m_hAvFormatDll, m_hAvCodecDll,
                m_hAvUtilDll);
        return false;
    }

#define RES(mod, name) m_ff.name = reinterpret_cast<fn_##name>(GetProcAddress(mod, #name))

    RES(m_hAvFormatDll, avformat_open_input);
    RES(m_hAvFormatDll, avformat_find_stream_info);
    RES(m_hAvFormatDll, avformat_close_input);
    RES(m_hAvFormatDll, av_find_best_stream);
    RES(m_hAvFormatDll, av_read_frame);

    RES(m_hAvCodecDll, avcodec_descriptor_name);
    RES(m_hAvCodecDll, avcodec_alloc_context3);
    RES(m_hAvCodecDll, avcodec_free_context);
    RES(m_hAvCodecDll, avcodec_parameters_to_context);
    RES(m_hAvCodecDll, avcodec_open2);
    RES(m_hAvCodecDll, avcodec_send_packet);
    RES(m_hAvCodecDll, avcodec_receive_subtitle);
    RES(m_hAvCodecDll, avcodec_find_decoder);

    m_ff.av_packet_alloc = reinterpret_cast<fn_av_packet_alloc>(GetProcAddress(m_hAvCodecDll, "av_packet_alloc"));
    if (!m_ff.av_packet_alloc)
        m_ff.av_packet_alloc = reinterpret_cast<fn_av_packet_alloc>(GetProcAddress(m_hAvFormatDll, "av_packet_alloc"));

    m_ff.av_packet_free = reinterpret_cast<fn_av_packet_free>(GetProcAddress(m_hAvCodecDll, "av_packet_free"));
    if (!m_ff.av_packet_free)
        m_ff.av_packet_free = reinterpret_cast<fn_av_packet_free>(GetProcAddress(m_hAvFormatDll, "av_packet_free"));

    m_ff.av_packet_unref = reinterpret_cast<fn_av_packet_unref>(GetProcAddress(m_hAvCodecDll, "av_packet_unref"));
    if (!m_ff.av_packet_unref)
        m_ff.av_packet_unref = reinterpret_cast<fn_av_packet_unref>(GetProcAddress(m_hAvFormatDll, "av_packet_unref"));

    RES(m_hAvUtilDll, avsubtitle_free);

#undef RES

    if (!m_ff.avformat_open_input || !m_ff.avformat_close_input || !m_ff.av_read_frame || !m_ff.av_packet_alloc) {
        VSubLog(L"[VIDI] Sub: fungsi FFmpeg kritis tidak ditemukan");
        return false;
    }

    m_dllsLoaded = true;
    VSubLog(L"[VIDI] Sub: FFmpeg loaded OK");
    return true;
}

void SubtitleReader::FreeFFmpegDlls() {
    if (m_hAvFormatDll) {
        FreeLibrary(m_hAvFormatDll);
        m_hAvFormatDll = nullptr;
    }
    if (m_hAvCodecDll) {
        FreeLibrary(m_hAvCodecDll);
        m_hAvCodecDll = nullptr;
    }
    if (m_hAvUtilDll) {
        FreeLibrary(m_hAvUtilDll);
        m_hAvUtilDll = nullptr;
    }
    m_dllsLoaded = false;
    memset(&m_ff, 0, sizeof(m_ff));
}

// ============================================================
// Constructor / Destructor
// ============================================================

SubtitleReader::SubtitleReader()
    : m_hAvFormatDll(nullptr),
      m_hAvCodecDll(nullptr),
      m_hAvUtilDll(nullptr),
      m_ff{},
      m_fmtCtx(nullptr),
      m_codecCtx(nullptr),
      m_subtitleStreamIndex(-1),
      m_subtitleCodecId(0),
      m_timeBaseDen(1),
      m_timeBaseNum(1000),
      m_dllsLoaded(false),
      m_fileOpen(false) {}

SubtitleReader::~SubtitleReader() {
    Close();
}

// ============================================================
// FreeFile / Close
// ============================================================

void SubtitleReader::FreeFile() {
    if (m_codecCtx && m_ff.avcodec_free_context) {
        m_ff.avcodec_free_context(&m_codecCtx);
        m_codecCtx = nullptr;
    }
    if (m_fmtCtx && m_ff.avformat_close_input) {
        m_ff.avformat_close_input(&m_fmtCtx);
        m_fmtCtx = nullptr;
    }
    m_subtitleStreamIndex = -1;
    m_subtitleCodecId = 0;
    m_fileOpen = false;
    m_subtitleIndex.clear();
}

void SubtitleReader::Close() {
    FreeFile();
    FreeFFmpegDlls();
}

// ============================================================
// Open — single pass detect + index
// ============================================================

bool SubtitleReader::Open(const wchar_t* videoPath) {
    FreeFile();
    if (!LoadFFmpegDlls())
        return false;

    std::string utf8Path = WideToUtf8(videoPath);
    VSubLog(L"[VIDI] Sub: opening %s", videoPath);

    int ret = m_ff.avformat_open_input(&m_fmtCtx, utf8Path.c_str(), nullptr, nullptr);
    if (ret < 0) {
        VSubLog(L"[VIDI] Sub: avformat_open_input failed (err=%d)", ret);
        return false;
    }

    ret = m_ff.avformat_find_stream_info(m_fmtCtx, nullptr);
    if (ret < 0) {
        VSubLog(L"[VIDI] Sub: avformat_find_stream_info failed (err=%d)", ret);
        FreeFile();
        return false;
    }

    VSubLog(L"[VIDI] Sub: detect + index in single pass...");
    DetectAndBuildInOnePass();

    // Default PlayRes for 720p anime BD (ASS \pos coordinates use this)
    m_playResX = 1280;
    m_playResY = 720;

    if (m_subtitleIndex.empty()) {
        VSubLog(L"[VIDI] Sub: no subtitle entries found");
    } else {
        VSubLog(L"[VIDI] Sub: %d entries, first at %.1fs, last at %.1fs", (int)m_subtitleIndex.size(),
                m_subtitleIndex.front().startSeconds, m_subtitleIndex.back().startSeconds);
    }

    m_fileOpen = true;
    return true;
}

// ============================================================
// DetectAndBuildInOnePass — deteksi + index dalam satu pass
// Tidak perlu av_seek_frame, tidak ada masalah stream position
// ============================================================

void SubtitleReader::DetectAndBuildInOnePass() {
    if (!m_fmtCtx)
        return;

    static const int MAX_STREAMS = 256;
    int textCounts[MAX_STREAMS] = {};
    int totalPackets[MAX_STREAMS] = {};

    AVPacketRaw* pkt = reinterpret_cast<AVPacketRaw*>(m_ff.av_packet_alloc());
    if (!pkt)
        return;

    struct RawEntry {
        int64_t pts;
        int64_t duration;
        int streamIndex;
        std::string data;
    };
    std::vector<RawEntry> allEntries;
    allEntries.reserve(4000);

    int scanned = 0;
    while (scanned < 200000 && m_ff.av_read_frame(m_fmtCtx, reinterpret_cast<AVPacket*>(pkt)) >= 0) {
        int si = pkt->stream_index;
        if (si >= 0 && si < MAX_STREAMS) {
            totalPackets[si]++;
            if (pkt->data && pkt->size > 4 && LooksLikeSubtitleText(pkt->data, pkt->size)) {
                textCounts[si]++;
            }
        }

        if (pkt->data && pkt->size > 0) {
            std::string rawText(reinterpret_cast<char*>(pkt->data), pkt->size);
            while (!rawText.empty() && rawText.back() == '\0')
                rawText.pop_back();
            if (!rawText.empty()) {
                allEntries.push_back({pkt->pts, pkt->duration, si, std::move(rawText)});
            }
        }

        m_ff.av_packet_unref(reinterpret_cast<AVPacket*>(pkt));
        scanned++;
    }

    VSubLog(L"[VIDI] Sub: scanned %d packets total", scanned);

    int bestStream = -1;
    int bestCount = 0;
    for (int i = 0; i < MAX_STREAMS; ++i) {
        if (totalPackets[i] > 0 && textCounts[i] > 0) {
            VSubLog(L"[VIDI] Sub: stream %d -> %d/%d text packets", i, textCounts[i], totalPackets[i]);
            if (textCounts[i] > bestCount && textCounts[i] >= totalPackets[i] / 3) {
                bestCount = textCounts[i];
                bestStream = i;
            }
        }
    }

    m_ff.av_packet_free(reinterpret_cast<AVPacket**>(&pkt));

    if (bestStream < 0) {
        VSubLog(L"[VIDI] Sub: no subtitle stream detected");
        return;
    }

    m_subtitleStreamIndex = bestStream;
    VSubLog(L"[VIDI] Sub: detected stream %d (%d text packets)", bestStream, bestCount);

    std::vector<RawEntry> subEntries;
    subEntries.reserve(bestCount);
    for (auto& e : allEntries) {
        if (e.streamIndex == bestStream) {
            subEntries.push_back(std::move(e));
        }
    }
    allEntries.clear();

    if (subEntries.empty())
        return;

    int64_t maxPts = 0;
    for (auto& e : subEntries) {
        if (e.pts != AV_NOPTS_VALUE && e.pts > maxPts)
            maxPts = e.pts;
    }
    double bestDivisor = DetectBestTimebase(maxPts);

    // DEBUG: tampilkan isi 5 packet pertama
    VSubLog(L"[VIDI] Sub: %d sub packets, showing first 5:", (int)subEntries.size());
    for (int i = 0; i < 5 && i < (int)subEntries.size(); ++i) {
        std::string preview = subEntries[i].data.substr(0, 120);
        double durSec = (subEntries[i].duration > 0) ? (double)subEntries[i].duration / bestDivisor : 0;
        VSubLog(L"[VIDI] Sub:   [%d] pts=%lld dur=%.2f data=[%hs]", i, subEntries[i].pts, durSec, preview.c_str());
    }

    m_subtitleIndex.clear();
    int skipped = 0;
    int withDuration = 0;
    for (auto& raw : subEntries) {
        double ptsSec = 0.0;
        if (raw.pts != AV_NOPTS_VALUE && raw.pts != 0)
            ptsSec = (double)raw.pts / bestDivisor;

        double durSec = 0.0;
        if (raw.duration > 0)
            durSec = (double)raw.duration / bestDivisor;

        std::wstring text;
        double posX = -1, posY = -1;
        int alignment = 0;
        int fontSize = 0;
        double startSec = ptsSec, endSec = ptsSec + durSec;

        if (ParseASSDialogue(raw.data, ptsSec, startSec, endSec, text, posX, posY, alignment, fontSize)) {
            if (durSec > 0)
                withDuration++;
        } else {
            skipped++;
            continue;
        }

        if (text.empty())
            continue;

        // Smart duration heuristics — when MKV doesn't provide duration
        if (durSec <= 0 || endSec <= startSec) {
            double textLen = (double)text.length();
            bool hasPos = (posX >= 0 && posY >= 0);

            // Base duration by text length: ~15 chars/sec reading speed
            double readDur = textLen / 15.0;
            if (hasPos) {
                // Signs/labels: shorter, 1.5-4s
                endSec = startSec + max(1.5, min(4.0, readDur));
            } else {
                // Dialogue: 2-7s depending on length
                endSec = startSec + max(2.0, min(7.0, readDur));
            }
        }

        SubtitleEntry entry;
        entry.startSeconds = startSec;
        entry.endSeconds = endSec;
        entry.text = text;
        entry.posX = posX;
        entry.posY = posY;
        entry.alignment = alignment;
        entry.fontSize = fontSize;
        m_subtitleIndex.push_back(entry);
    }

    std::sort(m_subtitleIndex.begin(), m_subtitleIndex.end(),
              [](const SubtitleEntry& a, const SubtitleEntry& b) { return a.startSeconds < b.startSeconds; });

    // Post-sort: refine end times using next entry's start when gap is reasonable
    for (size_t i = 0; i + 1 < m_subtitleIndex.size(); ++i) {
        auto& cur = m_subtitleIndex[i];
        const auto& next = m_subtitleIndex[i + 1];
        double gap = next.startSeconds - cur.startSeconds;
        double curDur = cur.endSeconds - cur.startSeconds;

        // Only use next.start if:
        // 1. Gap is reasonable (0.5s to 10s) — not too short (overlap), not too long (separate scenes)
        // 2. Current heuristic duration is longer than the gap
        if (gap > 0.3 && gap < 10.0 && curDur > gap) {
            cur.endSeconds = next.startSeconds - 0.05;
        }
        // Cap any entry that's still absurdly long
        if (cur.endSeconds - cur.startSeconds > 8.0)
            cur.endSeconds = cur.startSeconds + 7.0;
    }

    if (!m_subtitleIndex.empty() && m_subtitleIndex.back().endSeconds <= m_subtitleIndex.back().startSeconds)
        m_subtitleIndex.back().endSeconds = m_subtitleIndex.back().startSeconds + 3.0;

    // Deduplikasi: hapus entry yang punya posisi + teks sama (karaoke highlight/base duplikat, clip animation duplikat)
    {
        int beforeDedup = (int)m_subtitleIndex.size();
        std::vector<SubtitleEntry> deduped;
        deduped.reserve(m_subtitleIndex.size());
        for (auto& e : m_subtitleIndex) {
            bool isDup = false;
            if (e.posX >= 0 && e.posY >= 0 && !e.text.empty()) {
                for (const auto& d : deduped) {
                    if (d.posX >= 0 && d.posY >= 0 && d.text == e.text) {
                        double dx = fabs(d.posX - e.posX);
                        double dy = fabs(d.posY - e.posY);
                        if (dx < 2.0 && dy < 2.0) {
                            isDup = true;
                            break;
                        }
                    }
                }
            }
            if (!isDup)
                deduped.push_back(std::move(e));
        }
        VSubLog(L"[VIDI] Sub: dedup %d -> %d entries", beforeDedup, (int)deduped.size());
        m_subtitleIndex = std::move(deduped);
    }

    VSubLog(L"[VIDI] Sub: %d entries built (%d with MKV duration), %d skipped", (int)m_subtitleIndex.size(),
            withDuration, skipped);
}

// ============================================================
// GetSubtitleAt — binary search
// ============================================================

std::wstring SubtitleReader::GetSubtitleAt(double timeSeconds) {
    if (m_subtitleIndex.empty())
        return L"";

    int lo = 0, hi = (int)m_subtitleIndex.size() - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        const auto& e = m_subtitleIndex[mid];
        if (timeSeconds < e.startSeconds) {
            hi = mid - 1;
        } else if (timeSeconds > e.endSeconds) {
            lo = mid + 1;
        } else {
            return e.text;
        }
    }
    return L"";
}

// ============================================================
// GetActiveSubtitles — return ALL entries active at timeSeconds
// For multi-line ASS (karaoke, signs, positioned text)
// ============================================================

void SubtitleReader::GetActiveSubtitles(double timeSeconds, std::vector<SubtitleEntry>& out) {
    out.clear();
    if (m_subtitleIndex.empty())
        return;

    // Binary search to find first entry that could be active
    int lo = 0, hi = (int)m_subtitleIndex.size() - 1;
    int startIdx = 0;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (m_subtitleIndex[mid].endSeconds < timeSeconds) {
            lo = mid + 1;
        } else if (m_subtitleIndex[mid].startSeconds > timeSeconds) {
            hi = mid - 1;
        } else {
            startIdx = mid;
            break;
        }
    }
    if (lo > hi)
        startIdx = lo;

    // Scan forward from startIdx to collect all active entries
    for (int i = startIdx; i < (int)m_subtitleIndex.size(); ++i) {
        const auto& e = m_subtitleIndex[i];
        if (e.startSeconds > timeSeconds)
            break;
        if (e.endSeconds >= timeSeconds)
            out.push_back(e);
    }
}

// ============================================================
// GetSubtitleStreamCount / GetSubtitleStreams / ReadSubtitles
// ============================================================

int SubtitleReader::GetSubtitleStreamCount() const {
    if (!m_fileOpen)
        return 0;
    return (m_subtitleStreamIndex >= 0) ? 1 : 0;
}

std::vector<SubtitleInfo> SubtitleReader::GetSubtitleStreams() const {
    std::vector<SubtitleInfo> result;
    if (!m_fileOpen || m_subtitleStreamIndex < 0)
        return result;

    SubtitleInfo info;
    info.streamIndex = m_subtitleStreamIndex;
    info.language = "";
    info.title = "";
    info.codecId = m_subtitleCodecId;
    info.isTextBased = true;
    result.push_back(info);
    return result;
}

std::vector<SubtitleReader::TimedText> SubtitleReader::ReadSubtitles(double timeStart, double timeEnd) {
    std::vector<TimedText> result;
    for (const auto& e : m_subtitleIndex) {
        if (e.endSeconds < timeStart)
            continue;
        if (e.startSeconds > timeEnd)
            break;
        TimedText tt;
        tt.startSeconds = e.startSeconds;
        tt.endSeconds = e.endSeconds;
        tt.text = e.text;
        result.push_back(tt);
    }
    return result;
}

} // namespace kernelPlayerVidi

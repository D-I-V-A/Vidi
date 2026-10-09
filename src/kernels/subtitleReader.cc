#include "../../include/kernels/subtitleReader.hh"

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cstdarg>
#include <cmath>
#include <vector>
#include <string>
#include <cstdint>

namespace kernelPlayerVidi {

// ============================================================
// Font helpers
// ============================================================

static std::string DecodeNameString(const uint8_t* strPtr, uint16_t length, uint16_t encodingID) {
    std::string result;
    if (!strPtr || length == 0)
        return result;

    if (encodingID == 1) { // UTF-16BE
        for (int j = 0; j + 1 < length; j += 2) {
            char c = static_cast<char>(strPtr[j + 1]);
            if (c >= 32 && c < 127)
                result += c;
        }
    } else {
        for (int j = 0; j < length; ++j) {
            char c = static_cast<char>(strPtr[j]);
            if (c >= 32 && c < 127)
                result += c;
        }
    }
    return result;
}

static std::vector<std::string> ExtractTtfFontNames(const uint8_t* data, int size) {
    std::vector<std::string> names;
    if (!data || size < 12)
        return names;

    const uint16_t numTables =
        static_cast<uint16_t>((static_cast<uint16_t>(data[4]) << 8) | static_cast<uint16_t>(data[5]));

    const uint8_t* nameTable = nullptr;
    int nameTableLen = 0;

    for (int i = 0; i < numTables; ++i) {
        const int off = 12 + i * 16;
        if (off + 16 > size)
            break;
        if (std::memcmp(data + off, "name", 4) != 0)
            continue;

        const int nameTableOffset = (static_cast<int>(data[off + 8]) << 24) | (static_cast<int>(data[off + 9]) << 16) |
                                    (static_cast<int>(data[off + 10]) << 8) | static_cast<int>(data[off + 11]);
        nameTableLen = (static_cast<int>(data[off + 12]) << 24) | (static_cast<int>(data[off + 13]) << 16) |
                       (static_cast<int>(data[off + 14]) << 8) | static_cast<int>(data[off + 15]);

        if (nameTableOffset >= 0 && nameTableLen > 0 && nameTableOffset <= size &&
            nameTableLen <= size - nameTableOffset) {
            nameTable = data + nameTableOffset;
        }
        break;
    }

    if (!nameTable || nameTableLen < 6)
        return names;

    const uint16_t nameCount =
        static_cast<uint16_t>((static_cast<uint16_t>(nameTable[2]) << 8) | static_cast<uint16_t>(nameTable[3]));
    const uint16_t stringOffset =
        static_cast<uint16_t>((static_cast<uint16_t>(nameTable[4]) << 8) | static_cast<uint16_t>(nameTable[5]));

    if (stringOffset >= nameTableLen)
        return names;

    const uint8_t* stringStorage = nameTable + stringOffset;

    auto tryAddName = [&names](const std::string& name) {
        if (name.empty())
            return;
        for (const auto& existing : names) {
            if (existing == name)
                return;
        }
        names.push_back(name);
    };

    const int targetIDs[] = {4, 1, 6, 2};

    for (int targetID : targetIDs) {
        for (int i = 0; i < nameCount; ++i) {
            const int recOff = 6 + i * 12;
            if (recOff + 12 > nameTableLen)
                break;

            const uint16_t platformID = static_cast<uint16_t>((static_cast<uint16_t>(nameTable[recOff + 0]) << 8) |
                                                              static_cast<uint16_t>(nameTable[recOff + 1]));
            const uint16_t encodingID = static_cast<uint16_t>((static_cast<uint16_t>(nameTable[recOff + 2]) << 8) |
                                                              static_cast<uint16_t>(nameTable[recOff + 3]));
            const uint16_t nameID = static_cast<uint16_t>((static_cast<uint16_t>(nameTable[recOff + 6]) << 8) |
                                                          static_cast<uint16_t>(nameTable[recOff + 7]));
            const uint16_t strLength = static_cast<uint16_t>((static_cast<uint16_t>(nameTable[recOff + 8]) << 8) |
                                                             static_cast<uint16_t>(nameTable[recOff + 9]));
            const uint16_t strOffset = static_cast<uint16_t>((static_cast<uint16_t>(nameTable[recOff + 10]) << 8) |
                                                             static_cast<uint16_t>(nameTable[recOff + 11]));

            if (nameID != targetID)
                continue;
            if (platformID != 1 && platformID != 3)
                continue;
            if (static_cast<int>(stringOffset) + static_cast<int>(strOffset) + static_cast<int>(strLength) >
                nameTableLen)
                continue;

            const uint8_t* strPtr = stringStorage + strOffset;
            const std::string decoded = DecodeNameString(strPtr, strLength, encodingID);
            tryAddName(decoded);
        }
    }

    return names;
}

// ============================================================
// FFmpeg constants
// ============================================================

#ifndef AV_TIME_BASE
#define AV_TIME_BASE 1000000
#endif

#ifndef AV_NOPTS_VALUE
#define AV_NOPTS_VALUE ((int64_t)UINT64_C(0x8000000000000000))
#endif

struct AVPacketRawLayout {
    void* buf;
    int64_t pts;
    int64_t dts;
    uint8_t* data;
    int size;
    int stream_index;
    int flags;
    void* side_data;
    int side_data_elems;
    int side_data_padding;
    int64_t duration;
    int64_t pos;
    void* opaque;
    void* opaque_ref;
    int time_base_num;
    int time_base_den;
    char _pad[192];
};

// ============================================================
// Logging
// ============================================================

static void VSubLog(const wchar_t* fmt, ...) {
    wchar_t buf[1024] = {};
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf_s(buf, _countof(buf), _TRUNCATE, fmt, ap);
    va_end(ap);

    OutputDebugStringW(buf);
    OutputDebugStringW(L"\n");

    wchar_t tempPath[MAX_PATH] = {};
    if (!GetTempPathW(MAX_PATH, tempPath))
        return;
    if (wcscat_s(tempPath, MAX_PATH, L"vidi_debug.log") != 0)
        return;

    FILE* f = nullptr;
    if (_wfopen_s(&f, tempPath, L"a") != 0 || !f)
        return;

    SYSTEMTIME st;
    GetLocalTime(&st);
    fwprintf(f, L"[%02u:%02u:%02u.%03u] %s\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, buf);
    fclose(f);
}

// ============================================================
// UTF-8 / Wide helpers
// ============================================================

static std::wstring Utf8ToWide(const char* utf8) {
    if (!utf8 || !*utf8)
        return L"";
    const int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    if (len <= 0)
        return L"";
    std::wstring result(static_cast<size_t>(len), L'\0');
    const int written = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, result.data(), len);
    if (written <= 0)
        return L"";
    if (!result.empty() && result.back() == L'\0')
        result.pop_back();
    return result;
}

static std::string WideToUtf8(const wchar_t* wide) {
    if (!wide || !*wide)
        return "";
    const int len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0)
        return "";
    std::string result(static_cast<size_t>(len), '\0');
    const int written = WideCharToMultiByte(CP_UTF8, 0, wide, -1, result.data(), len, nullptr, nullptr);
    if (written <= 0)
        return "";
    if (!result.empty() && result.back() == '\0')
        result.pop_back();
    return result;
}

static std::wstring GetExeDir() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring full(path);
    const size_t pos = full.find_last_of(L"\\/");
    if (pos != std::wstring::npos)
        return full.substr(0, pos);
    return L".";
}

// ============================================================
// Detect subtitle-like packet
// ============================================================

static bool LooksLikeSubtitleText(const uint8_t* data, int size) {
    if (!data || size < 4)
        return false;
    if (data[0] == 0 && data[1] == 0 && (data[2] == 1 || (data[2] == 0 && data[3] == 1)))
        return false;

    int nullCount = 0;
    for (int i = 0; i < size && i < 32; ++i) {
        if (data[i] == 0)
            ++nullCount;
    }
    if (nullCount > 2)
        return false;

    int printable = 0, checked = 0;
    int i = 0;
    while (i < size && i < 256) {
        const uint8_t c = data[i];
        if (c >= 0x20 && c < 0x7F) {
            ++printable;
            ++checked;
            ++i;
        } else if (c == '\n' || c == '\r' || c == '\t') {
            ++printable;
            ++checked;
            ++i;
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
            return false;
        }
    }
    if (checked < 4)
        return false;
    return printable > checked * 7 / 10;
}

// ============================================================
// [FIX SRT/WEBVTT] Convert non-ASS subtitle packet to ASS dialogue
// ============================================================

static bool IsAssCodec(int codecId) {
    return codecId == VIDI_AV_CODEC_ID_ASS || codecId == VIDI_AV_CODEC_ID_SSA;
}

static bool IsBitmapSubtitleCodec(int codecId) {
    return codecId == VIDI_AV_CODEC_ID_DVD_SUBTITLE || codecId == VIDI_AV_CODEC_ID_DVB_SUBTITLE ||
           codecId == VIDI_AV_CODEC_ID_HDMV_PGS_SUBTITLE || codecId == VIDI_AV_CODEC_ID_XSUB ||
           codecId == VIDI_AV_CODEC_ID_DVB_TELETEXT;
}

static bool IsTextBasedSubtitleCodec(int codecId) {
    return codecId == VIDI_AV_CODEC_ID_TEXT || codecId == VIDI_AV_CODEC_ID_SRT || codecId == VIDI_AV_CODEC_ID_SUBRIP ||
           codecId == VIDI_AV_CODEC_ID_WEBVTT || codecId == VIDI_AV_CODEC_ID_MOV_TEXT ||
           codecId == VIDI_AV_CODEC_ID_MICRODVD || codecId == VIDI_AV_CODEC_ID_JACOSUB ||
           codecId == VIDI_AV_CODEC_ID_SAMI || codecId == VIDI_AV_CODEC_ID_REALTEXT ||
           codecId == VIDI_AV_CODEC_ID_SUBVIEWER || codecId == VIDI_AV_CODEC_ID_SUBVIEWER1 ||
           codecId == VIDI_AV_CODEC_ID_VPLAYER || codecId == VIDI_AV_CODEC_ID_PJS || codecId == VIDI_AV_CODEC_ID_MPL2 ||
           codecId == VIDI_AV_CODEC_ID_HDMV_TEXT_SUBTITLE || codecId == VIDI_AV_CODEC_ID_TTML;
}

// Extract raw text dari packet SRT/WebVTT (buang index & timing line)
static std::string ExtractSubtitleText(const char* data, int size, int codecId) {
    std::string str(data, static_cast<size_t>(size));
    // Trim trailing NUL
    while (!str.empty() && str.back() == '\0')
        str.pop_back();

    // WebVTT bisa punya "WEBVTT" header + cue identifier + timing
    // SRT punya "index\n00:00:00,000 --> 00:00:00,000\n"
    size_t arrowPos = str.find("-->");
    if (arrowPos != std::string::npos) {
        size_t lineEnd = str.find('\n', arrowPos);
        if (lineEnd != std::string::npos) {
            str = str.substr(lineEnd + 1);
        } else {
            str.clear();
        }
    }

    // Buang SRT index line di awal (angka + newline)
    if (codecId == VIDI_AV_CODEC_ID_SRT || codecId == VIDI_AV_CODEC_ID_SUBRIP) {
        size_t p = 0;
        while (p < str.size() && (str[p] == ' ' || str[p] == '\t'))
            p++;
        size_t digitsStart = p;
        while (p < str.size() && str[p] >= '0' && str[p] <= '9')
            p++;
        if (p > digitsStart && p < str.size() && (str[p] == '\r' || str[p] == '\n')) {
            while (p < str.size() && (str[p] == '\r' || str[p] == '\n'))
                p++;
            str = str.substr(p);
        }
    }
    return str;
}

// Convert multi-line text to ASS (\N line-break)
static std::string TextToAssFormat(const std::string& text) {
    std::string result;
    std::string line;
    for (char c : text) {
        if (c == '\r')
            continue;
        if (c == '\n') {
            if (!line.empty()) {
                if (!result.empty())
                    result += "\\N";
                result += line;
                line.clear();
            }
        } else {
            line += c;
        }
    }
    if (!line.empty()) {
        if (!result.empty())
            result += "\\N";
        result += line;
    }
    return result;
}

// Format timestamp ASS: H:MM:SS.CC
static std::string FormatAssTimestamp(long long ms) {
    if (ms < 0)
        ms = 0;
    long long h = ms / 3600000;
    ms %= 3600000;
    long long m = ms / 60000;
    ms %= 60000;
    long long s = ms / 1000;
    long long cs = (ms % 1000) / 10;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%lld:%02lld:%02lld.%02lld", h, m, s, cs);
    return std::string(buf);
}

// Build ASS Dialogue line sesuai Format di defaultHeader:
// Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
static std::string BuildAssDialogue(const std::string& text, long long startMs, long long durationMs) {
    std::string line = "0,";
    line += FormatAssTimestamp(startMs);
    line += ",";
    line += FormatAssTimestamp(startMs + durationMs);
    line += ",Default,,0,0,0,,";
    line += text;
    return line;
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
      m_dllsLoaded(false),
      m_fileOpen(false) {}

SubtitleReader::~SubtitleReader() {
    Close();
}

// ============================================================
// Load FFmpeg DLLs
// ============================================================

bool SubtitleReader::LoadFFmpegDlls() {
    if (m_dllsLoaded)
        return true;

    const std::wstring dir = GetExeDir();
    m_hAvFormatDll = LoadLibraryW((dir + L"\\filters\\x64\\avformat-lav-62.dll").c_str());
    m_hAvCodecDll = LoadLibraryW((dir + L"\\filters\\x64\\avcodec-lav-62.dll").c_str());
    m_hAvUtilDll = LoadLibraryW((dir + L"\\filters\\x64\\avutil-lav-60.dll").c_str());

    if (!m_hAvFormatDll || !m_hAvCodecDll || !m_hAvUtilDll) {
        VSubLog(L"[VIDI] Sub: gagal load FFmpeg DLL (format=%p codec=%p util=%p)", m_hAvFormatDll, m_hAvCodecDll,
                m_hAvUtilDll);
        return false;
    }

#define RES(mod, name) m_ff.name = reinterpret_cast<fn_##name>(GetProcAddress(mod, #name))

    RES(m_hAvFormatDll, avformat_open_input);
    RES(m_hAvFormatDll, avformat_find_stream_info);
    RES(m_hAvFormatDll, avformat_close_input);
    RES(m_hAvFormatDll, av_find_best_stream);
    RES(m_hAvFormatDll, av_read_frame);
    RES(m_hAvFormatDll, av_seek_frame);

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

    m_ff.av_dict_get = reinterpret_cast<fn_av_dict_get>(GetProcAddress(m_hAvUtilDll, "av_dict_get"));
    if (!m_ff.av_dict_get)
        m_ff.av_dict_get = reinterpret_cast<fn_av_dict_get>(GetProcAddress(m_hAvFormatDll, "av_dict_get"));

#undef RES

    if (!m_ff.avformat_open_input || !m_ff.avformat_find_stream_info || !m_ff.avformat_close_input ||
        !m_ff.av_read_frame || !m_ff.av_packet_alloc || !m_ff.av_packet_free || !m_ff.av_packet_unref) {
        VSubLog(L"[VIDI] Sub: fungsi FFmpeg kritis tidak ditemukan");
        return false;
    }

    m_dllsLoaded = true;
    VSubLog(L"[VIDI] Sub: FFmpeg DLL loaded");
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
    std::memset(&m_ff, 0, sizeof(m_ff));
}

void SubtitleReader::FreeFile() {
    m_loaded.store(false);
    if (m_codecCtx && m_ff.avcodec_free_context) {
        m_ff.avcodec_free_context(&m_codecCtx);
        m_codecCtx = nullptr;
    }
    if (m_fmtCtx && m_ff.avformat_close_input) {
        m_ff.avformat_close_input(&m_fmtCtx);
        m_fmtCtx = nullptr;
    }
    m_assRenderer.Shutdown();
    m_subtitleStreamIndex = -1;
    m_subtitleCodecId = 0;
    m_fileOpen = false;
}

void SubtitleReader::Close() {
    FreeFile();
}

void SubtitleReader::FullShutdown() {
    FreeFile();
    FreeFFmpegDlls();
}

// ============================================================
// Open subtitle
// ============================================================

bool SubtitleReader::Open(const wchar_t* videoPath) {
    FreeFile();

    // [FIX] Reset cancel flag setiap kali membuka file baru
    m_cancelRequested.store(false);

    if (!videoPath || !*videoPath)
        return false;
    if (!LoadFFmpegDlls())
        return false;

    if (!m_assRenderer.Initialize()) {
        VSubLog(L"[VIDI] Sub: libass initialization failed");
        return false;
    }

    const std::string utf8Path = WideToUtf8(videoPath);
    VSubLog(L"[VIDI] Sub: opening subtitle source: %s", videoPath);

    int ret = m_ff.avformat_open_input(&m_fmtCtx, utf8Path.c_str(), nullptr, nullptr);
    if (ret < 0) {
        VSubLog(L"[VIDI] Sub: avformat_open_input failed: %d", ret);
        FreeFile();
        return false;
    }

    ret = m_ff.avformat_find_stream_info(m_fmtCtx, nullptr);
    if (ret < 0) {
        VSubLog(L"[VIDI] Sub: avformat_find_stream_info failed");
        FreeFile();
        return false;
    }

    // --------------------------------------------------------
    // Find subtitle stream
    // --------------------------------------------------------
    static const int MAX_STREAMS = 256;
    bool isAttachment[MAX_STREAMS] = {};
    int bestStream = -1;

    if (m_ff.av_find_best_stream) {
        bestStream = m_ff.av_find_best_stream(m_fmtCtx, AVMEDIA_TYPE_SUBTITLE, -1, -1, nullptr, 0);
    }

    // --------------------------------------------------------
    // Fallback detection via packet scan
    // --------------------------------------------------------
    if (bestStream < 0) {
        AVPacketRawLayout* pktFb = reinterpret_cast<AVPacketRawLayout*>(m_ff.av_packet_alloc());
        if (!pktFb) {
            FreeFile();
            return false;
        }

        int textCounts[MAX_STREAMS] = {};
        int totalPackets[MAX_STREAMS] = {};
        int scanned = 0;

        while (scanned < 5000 && m_ff.av_read_frame(m_fmtCtx, reinterpret_cast<AVPacket*>(pktFb)) >= 0) {
            // [FIX] Check cancel
            if (m_cancelRequested.load(std::memory_order_relaxed)) {
                m_ff.av_packet_unref(reinterpret_cast<AVPacket*>(pktFb));
                break;
            }
            const int si = pktFb->stream_index;
            if (si >= 0 && si < MAX_STREAMS) {
                ++totalPackets[si];
                if (pktFb->data && pktFb->size > 4 && LooksLikeSubtitleText(pktFb->data, pktFb->size)) {
                    ++textCounts[si];
                }
            }
            m_ff.av_packet_unref(reinterpret_cast<AVPacket*>(pktFb));
            ++scanned;
        }

        if (m_ff.av_seek_frame)
            m_ff.av_seek_frame(m_fmtCtx, -1, 0, 0);

        for (int i = 0; i < MAX_STREAMS; ++i) {
            if (textCounts[i] <= 0)
                continue;
            if (textCounts[i] < totalPackets[i] / 3)
                continue;
            if (bestStream < 0 || textCounts[i] > textCounts[bestStream])
                bestStream = i;
        }

        m_ff.av_packet_free(reinterpret_cast<AVPacket**>(&pktFb));
    }

    if (bestStream < 0) {
        VSubLog(L"[VIDI] Sub: subtitle stream not found");
        FreeFile();
        return false;
    }

    m_subtitleStreamIndex = bestStream;
    VSubLog(L"[VIDI] Sub: selected stream %d", bestStream);

    // --------------------------------------------------------
    // Detect attachment streams
    // --------------------------------------------------------
    {
        auto fmtRaw = reinterpret_cast<AVFormatContextCompat*>(m_fmtCtx);
        if (fmtRaw) {
            for (unsigned int si = 0; si < fmtRaw->nb_streams && si < (unsigned)MAX_STREAMS; ++si) {
                auto streamRaw = reinterpret_cast<AVStreamCompat*>(fmtRaw->streams[si]);
                if (!streamRaw || !streamRaw->codecpar)
                    continue;
                if (streamRaw->codecpar->codec_type == AVMEDIA_TYPE_ATTACHMENT)
                    isAttachment[si] = true;
            }
        }
    }

    // --------------------------------------------------------
    // [FIX] Set codec ID & tentukan mode (ASS vs convert)
    // --------------------------------------------------------
    {
        auto fmtRaw = reinterpret_cast<AVFormatContextCompat*>(m_fmtCtx);
        if (fmtRaw && bestStream >= 0 && bestStream < (int)fmtRaw->nb_streams && fmtRaw->streams) {
            auto streamRaw = reinterpret_cast<AVStreamCompat*>(fmtRaw->streams[bestStream]);
            if (streamRaw && streamRaw->codecpar) {
                m_subtitleCodecId = streamRaw->codecpar->codec_id;
                VSubLog(L"[VIDI] Sub: codec_id=0x%X", m_subtitleCodecId);
            }
        }
    }

    const bool codecIsAss = IsAssCodec(m_subtitleCodecId);
    const bool codecIsBitmap = IsBitmapSubtitleCodec(m_subtitleCodecId);
    const bool codecIsTextConvert = !codecIsAss && IsTextBasedSubtitleCodec(m_subtitleCodecId);

    if (codecIsBitmap) {
        VSubLog(L"[VIDI] Sub: bitmap subtitle codec 0x%X tidak didukung oleh custom renderer, skip", m_subtitleCodecId);
        // Bitmap subtitle butuh renderer khusus (PGS/VobSub). Custom libass tidak bisa handle.
        // Biarkan stream tetap terdeteksi, tapi tidak ada packet yang diproses.
    } else if (codecIsTextConvert) {
        VSubLog(L"[VIDI] Sub: text-based codec 0x%X -> convert to ASS dialogue", m_subtitleCodecId);
    }

    // --------------------------------------------------------
    // Load ASS codec private (hanya jika ASS/SSA)
    // --------------------------------------------------------
    if (codecIsAss) {
        auto fmtRaw = reinterpret_cast<AVFormatContextCompat*>(m_fmtCtx);
        if (fmtRaw && bestStream >= 0 && bestStream < (int)fmtRaw->nb_streams && fmtRaw->streams) {
            auto streamRaw = reinterpret_cast<AVStreamCompat*>(fmtRaw->streams[bestStream]);
            if (streamRaw && streamRaw->codecpar) {
                auto parRaw = streamRaw->codecpar;
                if (parRaw->extradata && parRaw->extradata_size > 0) {
                    if (!m_assRenderer.LoadTrackFromMemory(reinterpret_cast<const char*>(parRaw->extradata),
                                                           parRaw->extradata_size)) {
                        VSubLog(L"[VIDI] Sub: failed to load ASS codec private");
                    } else {
                        VSubLog(L"[VIDI] Sub: loaded ASS codec private (%d bytes)", parRaw->extradata_size);
                    }
                } else {
                    // Default ASS header
                    const char* defaultHeader = "[Script Info]\r\n"
                                                "ScriptType: v4.00+\r\n"
                                                "PlayResX: 1280\r\n"
                                                "PlayResY: 720\r\n"
                                                "WrapStyle: 0\r\n"
                                                "\r\n"
                                                "[V4+ Styles]\r\n"
                                                "Format: Name, Fontname, Fontsize, "
                                                "PrimaryColour, SecondaryColour, "
                                                "OutlineColour, BackColour, "
                                                "Bold, Italic, Underline, StrikeOut, "
                                                "ScaleX, ScaleY, Spacing, Angle, "
                                                "BorderStyle, Outline, Shadow, "
                                                "Alignment, MarginL, MarginR, MarginV, Encoding\r\n"
                                                "Style: "
                                                "Default,Arial,48,"
                                                "&H00FFFFFF,&H000000FF,"
                                                "&H00000000,&H64000000,"
                                                "-1,0,0,0,100,100,0,0,"
                                                "1,2,2,2,10,10,10,1\r\n"
                                                "\r\n"
                                                "[Events]\r\n"
                                                "Format: Layer, Start, End, Style, "
                                                "Name, MarginL, MarginR, MarginV, "
                                                "Effect, Text\r\n";
                    m_assRenderer.LoadTrackFromMemory(defaultHeader, static_cast<int>(std::strlen(defaultHeader)));
                    VSubLog(L"[VIDI] Sub: using default ASS header (ASS without codec private)");
                }
            }
        }
    } else {
        // [FIX] Non-ASS codec: selalu pakai default ASS header (untuk Format: Line)
        const char* defaultHeader = "[Script Info]\r\n"
                                    "ScriptType: v4.00+\r\n"
                                    "PlayResX: 1280\r\n"
                                    "PlayResY: 720\r\n"
                                    "WrapStyle: 0\r\n"
                                    "\r\n"
                                    "[V4+ Styles]\r\n"
                                    "Format: Name, Fontname, Fontsize, "
                                    "PrimaryColour, SecondaryColour, "
                                    "OutlineColour, BackColour, "
                                    "Bold, Italic, Underline, StrikeOut, "
                                    "ScaleX, ScaleY, Spacing, Angle, "
                                    "BorderStyle, Outline, Shadow, "
                                    "Alignment, MarginL, MarginR, MarginV, Encoding\r\n"
                                    "Style: "
                                    "Default,Arial,48,"
                                    "&H00FFFFFF,&H000000FF,"
                                    "&H00000000,&H64000000,"
                                    "-1,0,0,0,100,100,0,0,"
                                    "1,2,2,2,10,10,10,1\r\n"
                                    "\r\n"
                                    "[Events]\r\n"
                                    "Format: Layer, Start, End, Style, "
                                    "Name, MarginL, MarginR, MarginV, "
                                    "Effect, Text\r\n";
        m_assRenderer.LoadTrackFromMemory(defaultHeader, static_cast<int>(std::strlen(defaultHeader)));
        VSubLog(L"[VIDI] Sub: using default ASS header (text codec)");
    }

    // --------------------------------------------------------
    // Reset reader ke awal subtitle stream
    // --------------------------------------------------------
    if (m_ff.av_seek_frame) {
        m_ff.av_seek_frame(m_fmtCtx, m_subtitleStreamIndex, 0, 0);
    }

    AVRational subTimeBase = {1, 1000000000};
    {
        auto fmtRaw = reinterpret_cast<AVFormatContextCompat*>(m_fmtCtx);
        if (fmtRaw && bestStream >= 0 && bestStream < (int)fmtRaw->nb_streams && fmtRaw->streams) {
            auto streamRaw = reinterpret_cast<AVStreamCompat*>(fmtRaw->streams[bestStream]);
            if (streamRaw)
                subTimeBase = streamRaw->time_base;
        }
    }

    double tbVal = 1e-9;
    if (subTimeBase.den != 0)
        tbVal = static_cast<double>(subTimeBase.num) / static_cast<double>(subTimeBase.den);

    VSubLog(L"[VIDI] Sub: time_base = %d/%d", subTimeBase.num, subTimeBase.den);

    // --------------------------------------------------------
    // Read subtitle packets
    // --------------------------------------------------------
    AVPacketRawLayout* pkt = reinterpret_cast<AVPacketRawLayout*>(m_ff.av_packet_alloc());
    if (!pkt) {
        VSubLog(L"[VIDI] Sub: AVPacket allocation failed");
        FreeFile();
        return false;
    }

    int fed = 0, fontsFed = 0, scanned = 0;
    long long firstSubtitleMs = -1, lastSubtitleMs = -1;

    while (m_ff.av_read_frame(m_fmtCtx, reinterpret_cast<AVPacket*>(pkt)) >= 0) {
        // [FIX] Check cancel flag
        if (m_cancelRequested.load(std::memory_order_relaxed)) {
            m_ff.av_packet_unref(reinterpret_cast<AVPacket*>(pkt));
            VSubLog(L"[VIDI] Sub: cancel requested, aborting scan");
            break;
        }

        const int si = pkt->stream_index;

        if (si != bestStream && (si < 0 || si >= MAX_STREAMS || !isAttachment[si])) {
            m_ff.av_packet_unref(reinterpret_cast<AVPacket*>(pkt));
            ++scanned;
            continue;
        }

        if (pkt->data && pkt->size > 0) {
            // =================================================
            // Subtitle packet
            // =================================================
            if (si == bestStream && !codecIsBitmap) {
                int dataSize = pkt->size;
                while (dataSize > 0 && pkt->data[dataSize - 1] == '\0')
                    --dataSize;

                if (dataSize > 0) {
                    long long timecodeMs = 0;
                    if (pkt->pts != AV_NOPTS_VALUE) {
                        const double ptsMs = static_cast<double>(pkt->pts) * tbVal * 1000.0;
                        if (ptsMs > -9.22e18 && ptsMs < 9.22e18)
                            timecodeMs = static_cast<long long>(std::llround(ptsMs));
                    }
                    if (timecodeMs < 0)
                        timecodeMs = 0;

                    long long durationMs = 5000;
                    if (pkt->duration > 0) {
                        const double duration = static_cast<double>(pkt->duration) * tbVal * 1000.0;
                        if (duration > 0.0 && duration < 9.22e18)
                            durationMs = static_cast<long long>(std::llround(duration));
                    }
                    // [FIX] Hapus clamp 30s — biarkan durasi natural
                    // (fallback hanya jika durasi tidak masuk akal)
                    if (durationMs <= 0)
                        durationMs = 5000;
                    if (durationMs > 3600000) // 1 jam = fallback
                        durationMs = 5000;

                    if (firstSubtitleMs < 0)
                        firstSubtitleMs = timecodeMs;
                    if (timecodeMs > lastSubtitleMs)
                        lastSubtitleMs = timecodeMs;

                    if (fed < 20) {
                        int previewSize = dataSize;
                        if (previewSize > 100)
                            previewSize = 100;
                        std::string preview(reinterpret_cast<char*>(pkt->data), previewSize);
                        VSubLog(L"[VIDI] Sub: packet[%d] start=%lldms duration=%lldms text='%hs'", fed, timecodeMs,
                                durationMs, preview.c_str());
                    }
                    if (fed > 0 && fed % 500 == 0) {
                        VSubLog(L"[VIDI] Sub: progress events=%d last=%lldms scanned=%d", fed, timecodeMs, scanned);
                    }

                    // =========================================
                    // [FIX] Feed libass: ASS langsung, lainnya convert
                    // =========================================
                    if (codecIsAss) {
                        if (m_assRenderer.ProcessChunk(reinterpret_cast<const char*>(pkt->data), dataSize, timecodeMs,
                                                       durationMs)) {
                            ++fed;
                        }
                    } else if (codecIsTextConvert) {
                        std::string text =
                            ExtractSubtitleText(reinterpret_cast<const char*>(pkt->data), dataSize, m_subtitleCodecId);
                        std::string assText = TextToAssFormat(text);
                        if (!assText.empty()) {
                            std::string assLine = BuildAssDialogue(assText, timecodeMs, durationMs);
                            if (m_assRenderer.ProcessChunk(assLine.c_str(), static_cast<int>(assLine.size()),
                                                           timecodeMs, durationMs)) {
                                ++fed;
                            }
                        }
                    }
                }
            }
            // =================================================
            // Font attachment
            // =================================================
            else if (si >= 0 && si < MAX_STREAMS && isAttachment[si] && fontsFed < 100) {
                const uint8_t* fontData = pkt->data;
                const int fontSize = pkt->size;
                std::vector<std::string> fontNames = ExtractTtfFontNames(fontData, fontSize);
                if (fontNames.empty())
                    fontNames.push_back("ATTACHMENT_" + std::to_string(si));

                for (const auto& fontName : fontNames)
                    m_assRenderer.AddFont(fontName.c_str(), reinterpret_cast<const char*>(fontData), fontSize);

                ++fontsFed;
            }
        }

        m_ff.av_packet_unref(reinterpret_cast<AVPacket*>(pkt));
        ++scanned;
    }

    m_ff.av_packet_free(reinterpret_cast<AVPacket**>(&pkt));

    VSubLog(L"[VIDI] Sub: scan complete, packets=%d, events=%d, fonts=%d, first=%lldms, last=%lldms", scanned, fed,
            fontsFed, firstSubtitleMs, lastSubtitleMs);

    // --------------------------------------------------------
    // Load attachment fonts from codecpar extradata
    // --------------------------------------------------------
    {
        auto fmtRaw = reinterpret_cast<AVFormatContextCompat*>(m_fmtCtx);
        if (fmtRaw) {
            int extraFonts = 0;
            for (unsigned int si = 0; si < fmtRaw->nb_streams; ++si) {
                if (static_cast<int>(si) == bestStream)
                    continue;
                auto streamRaw = reinterpret_cast<AVStreamCompat*>(fmtRaw->streams[si]);
                if (!streamRaw || !streamRaw->codecpar)
                    continue;
                if (streamRaw->codecpar->codec_type != AVMEDIA_TYPE_ATTACHMENT)
                    continue;
                if (!streamRaw->codecpar->extradata || streamRaw->codecpar->extradata_size <= 0)
                    continue;

                const uint8_t* data = streamRaw->codecpar->extradata;
                const int size = streamRaw->codecpar->extradata_size;
                std::vector<std::string> fontNames = ExtractTtfFontNames(data, size);
                if (fontNames.empty())
                    fontNames.push_back("ATTACHMENT_" + std::to_string(si));
                for (const auto& fontName : fontNames)
                    m_assRenderer.AddFont(fontName.c_str(), reinterpret_cast<const char*>(data), size);
                ++extraFonts;
            }
            VSubLog(L"[VIDI] Sub: %d attachment fonts loaded from extradata", extraFonts);
        }
    }

    // --------------------------------------------------------
    // Diagnostics
    // --------------------------------------------------------
    ASS_Track* track = m_assRenderer.GetTrack();
    if (track) {
        VSubLog(L"[VIDI] Sub: ASS track events=%d styles=%d", track->n_events, track->n_styles);
    }

    int playResX = 1280, playResY = 720;
    if (track) {
        if (track->PlayResX > 0)
            playResX = track->PlayResX;
        if (track->PlayResY > 0)
            playResY = track->PlayResY;
    }

    m_assRenderer.SetFrameSize(playResX, playResY);
    m_assRenderer.SetStorageSize(playResX, playResY);
    m_assRenderer.SetFonts(L"Yu Gothic", L"Yu Gothic");
    m_assRenderer.SetCheckReadorder(true);

    VSubLog(L"[VIDI] Sub: PlayRes=%dx%d", playResX, playResY);

    m_loaded.store(true);
    m_fileOpen = true;

    VSubLog(L"[VIDI] Sub: subtitle system READY");
    return true;
}

// ============================================================
// Query
// ============================================================

int SubtitleReader::GetSubtitleStreamCount() const {
    if (!m_fileOpen)
        return 0;
    return m_subtitleStreamIndex >= 0 ? 1 : 0;
}

std::vector<SubtitleInfo> SubtitleReader::GetSubtitleStreams() const {
    std::vector<SubtitleInfo> result;
    if (!m_fileOpen || m_subtitleStreamIndex < 0)
        return result;

    SubtitleInfo info{};
    info.streamIndex = m_subtitleStreamIndex;
    info.language = "";
    info.title = "";
    info.codecId = m_subtitleCodecId;
    info.isTextBased = !IsBitmapSubtitleCodec(m_subtitleCodecId);
    result.push_back(info);
    return result;
}

// ============================================================
// RenderFrame
// ============================================================

RenderResult SubtitleReader::RenderFrame(double timeSeconds) {
    RenderResult result;
    result.changed = false;
    if (!m_loaded.load())
        return result;
    if (timeSeconds < 0.0)
        timeSeconds = 0.0;

    const double timeMsDouble = timeSeconds * 1000.0;
    if (timeMsDouble > 9.22e18 || timeMsDouble < -9.22e18)
        return result;

    const long long timeMs = static_cast<long long>(std::llround(timeMsDouble));
    const RenderResult assResult = m_assRenderer.RenderFrame(timeMs);

    result.changed = assResult.changed;
    result.bitmaps.reserve(assResult.bitmaps.size());
    for (auto& img : assResult.bitmaps) {
        RenderedBitmap rb;
        rb.x = img.x;
        rb.y = img.y;
        rb.width = img.width;
        rb.height = img.height;
        rb.color = img.color;
        rb.bitmap = std::move(img.bitmap);
        result.bitmaps.push_back(std::move(rb));
    }
    return result;
}

} // namespace kernelPlayerVidi
#include "../../include/kernels/subtitleReader.hh"

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cstdarg>
#include <cmath>
#include <vector>
#include <string>

namespace kernelPlayerVidi {

// ============================================================
// Font helpers
// ============================================================

static std::string DecodeNameString(const uint8_t* strPtr, uint16_t length, uint16_t encodingID) {
    std::string result;

    if (!strPtr || length == 0)
        return result;

    if (encodingID == 1) {
        // UTF-16BE
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

    // Prefer:
    // 4 = Full font name
    // 1 = Family
    // 6 = PostScript name
    // 2 = Subfamily
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

            if (platformID != 1 && platformID != 3) {
                continue;
            }

            if (static_cast<int>(stringOffset) + static_cast<int>(strOffset) + static_cast<int>(strLength) >
                nameTableLen) {
                continue;
            }

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

// ============================================================
// AVPacket raw layout
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

    if (!GetTempPathW(MAX_PATH, tempPath)) {
        return;
    }

    if (wcscat_s(tempPath, MAX_PATH, L"vidi_debug.log") != 0) {
        return;
    }

    FILE* f = nullptr;

    if (_wfopen_s(&f, tempPath, L"a") != 0 || !f) {
        return;
    }

    SYSTEMTIME st;
    GetLocalTime(&st);

    fwprintf(f, L"[%02u:%02u:%02u.%03u] %s\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, buf);

    fclose(f);
}

// ============================================================
// UTF-8 -> Wide
// ============================================================

static std::wstring Utf8ToWide(const char* utf8) {
    if (!utf8 || !*utf8)
        return L"";

    const int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);

    if (len <= 0)
        return L"";

    std::wstring result(static_cast<size_t>(len - 1), L'\0');

    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, result.data(), len);

    return result;
}

// ============================================================
// Wide -> UTF-8
// ============================================================

static std::string WideToUtf8(const wchar_t* wide) {
    if (!wide || !*wide)
        return "";

    const int len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);

    if (len <= 0)
        return "";

    std::string result(static_cast<size_t>(len - 1), '\0');

    WideCharToMultiByte(CP_UTF8, 0, wide, -1, result.data(), len, nullptr, nullptr);

    return result;
}

// ============================================================
// EXE directory
// ============================================================

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

    // NAL start code
    if (data[0] == 0 && data[1] == 0 && (data[2] == 1 || (data[2] == 0 && data[3] == 1))) {
        return false;
    }

    int nullCount = 0;

    for (int i = 0; i < size && i < 32; ++i) {
        if (data[i] == 0)
            ++nullCount;
    }

    if (nullCount > 2)
        return false;

    int printable = 0;
    int checked = 0;

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

            if (i + 1 >= size || (data[i + 1] & 0xC0) != 0x80) {
                return false;
            }

            printable += 2;
            checked += 2;
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {

            if (i + 2 >= size || (data[i + 1] & 0xC0) != 0x80 || (data[i + 2] & 0xC0) != 0x80) {
                return false;
            }

            printable += 3;
            checked += 3;
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {

            if (i + 3 >= size || (data[i + 1] & 0xC0) != 0x80 || (data[i + 2] & 0xC0) != 0x80 ||
                (data[i + 3] & 0xC0) != 0x80) {
                return false;
            }

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
// Constructor
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

// ============================================================
// Destructor
// ============================================================

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
        VSubLog(L"[VIDI] Sub: gagal load FFmpeg DLL "
                L"(format=%p codec=%p util=%p)",
                m_hAvFormatDll, m_hAvCodecDll, m_hAvUtilDll);

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

    if (!m_ff.av_packet_alloc) {

        m_ff.av_packet_alloc = reinterpret_cast<fn_av_packet_alloc>(GetProcAddress(m_hAvFormatDll, "av_packet_alloc"));
    }

    m_ff.av_packet_free = reinterpret_cast<fn_av_packet_free>(GetProcAddress(m_hAvCodecDll, "av_packet_free"));

    if (!m_ff.av_packet_free) {

        m_ff.av_packet_free = reinterpret_cast<fn_av_packet_free>(GetProcAddress(m_hAvFormatDll, "av_packet_free"));
    }

    m_ff.av_packet_unref = reinterpret_cast<fn_av_packet_unref>(GetProcAddress(m_hAvCodecDll, "av_packet_unref"));

    if (!m_ff.av_packet_unref) {

        m_ff.av_packet_unref = reinterpret_cast<fn_av_packet_unref>(GetProcAddress(m_hAvFormatDll, "av_packet_unref"));
    }

    RES(m_hAvUtilDll, avsubtitle_free);

    m_ff.av_dict_get = reinterpret_cast<fn_av_dict_get>(GetProcAddress(m_hAvUtilDll, "av_dict_get"));

    if (!m_ff.av_dict_get) {

        m_ff.av_dict_get = reinterpret_cast<fn_av_dict_get>(GetProcAddress(m_hAvFormatDll, "av_dict_get"));
    }

#undef RES

    if (!m_ff.avformat_open_input || !m_ff.avformat_find_stream_info || !m_ff.avformat_close_input ||
        !m_ff.av_read_frame || !m_ff.av_packet_alloc || !m_ff.av_packet_free || !m_ff.av_packet_unref) {
        VSubLog(L"[VIDI] Sub: fungsi FFmpeg kritis "
                L"tidak ditemukan");

        return false;
    }

    m_dllsLoaded = true;

    VSubLog(L"[VIDI] Sub: FFmpeg DLL loaded");

    return true;
}

// ============================================================
// Free FFmpeg DLLs
// ============================================================

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

// ============================================================
// Free file
// ============================================================

void SubtitleReader::FreeFile() {
    m_loaded = false;

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

// ============================================================
// Close
// ============================================================

void SubtitleReader::Close() {
    FreeFile();
}

// ============================================================
// Full shutdown
// ============================================================

void SubtitleReader::FullShutdown() {
    FreeFile();
    FreeFFmpegDlls();
}

// ============================================================
// Open subtitle
// ============================================================

bool SubtitleReader::Open(const wchar_t* videoPath) {
    FreeFile();

    if (!videoPath || !*videoPath)
        return false;

    if (!LoadFFmpegDlls())
        return false;

    // --------------------------------------------------------
    // Init libass
    // --------------------------------------------------------

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
    // Fallback detection
    // --------------------------------------------------------

    if (bestStream < 0) {

        AVPacketRaw* pktFb = reinterpret_cast<AVPacketRaw*>(m_ff.av_packet_alloc());

        if (!pktFb) {

            FreeFile();

            return false;
        }

        int textCounts[MAX_STREAMS] = {};
        int totalPackets[MAX_STREAMS] = {};

        int scanned = 0;

        while (scanned < 5000 && m_ff.av_read_frame(m_fmtCtx, reinterpret_cast<AVPacket*>(pktFb)) >= 0) {
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

        if (m_ff.av_seek_frame) {

            m_ff.av_seek_frame(m_fmtCtx, -1, 0, 0);
        }

        for (int i = 0; i < MAX_STREAMS; ++i) {
            if (textCounts[i] <= 0)
                continue;

            if (textCounts[i] < totalPackets[i] / 3) {
                continue;
            }

            if (bestStream < 0 || textCounts[i] > textCounts[bestStream]) {
                bestStream = i;
            }
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

            for (unsigned int si = 0; si < fmtRaw->nb_streams && si < MAX_STREAMS; ++si) {
                auto streamRaw = reinterpret_cast<AVStreamCompat*>(fmtRaw->streams[si]);

                if (!streamRaw || !streamRaw->codecpar) {
                    continue;
                }

                if (streamRaw->codecpar->codec_type == AVMEDIA_TYPE_ATTACHMENT) {
                    isAttachment[si] = true;
                }
            }
        }
    }

    // --------------------------------------------------------
    // Load ASS codec private
    // --------------------------------------------------------

    {
        auto fmtRaw = reinterpret_cast<AVFormatContextCompat*>(m_fmtCtx);

        if (fmtRaw && bestStream >= 0 && bestStream < static_cast<int>(fmtRaw->nb_streams) && fmtRaw->streams) {
            auto streamRaw = reinterpret_cast<AVStreamCompat*>(fmtRaw->streams[bestStream]);

            if (streamRaw && streamRaw->codecpar) {
                auto parRaw = streamRaw->codecpar;

                if (parRaw->extradata && parRaw->extradata_size > 0) {
                    if (!m_assRenderer.LoadTrackFromMemory(reinterpret_cast<const char*>(parRaw->extradata),
                                                           parRaw->extradata_size)) {
                        VSubLog(L"[VIDI] Sub: failed to load "
                                L"ASS codec private");
                    } else {
                        VSubLog(L"[VIDI] Sub: loaded ASS "
                                L"codec private (%d bytes)",
                                parRaw->extradata_size);
                    }
                } else {

                    // ------------------------------------------------
                    // Default ASS header
                    // ------------------------------------------------

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

                    VSubLog(L"[VIDI] Sub: using default ASS header");
                }
            }
        }
    }

    // --------------------------------------------------------
    // Subtitle time base
    // --------------------------------------------------------

    if (m_ff.av_seek_frame) {

        m_ff.av_seek_frame(m_fmtCtx, -1, 0, 0);
    }

    AVRational subTimeBase = {1, 1000000000};

    {
        auto fmtRaw = reinterpret_cast<AVFormatContextCompat*>(m_fmtCtx);

        if (fmtRaw && bestStream >= 0 && bestStream < static_cast<int>(fmtRaw->nb_streams) && fmtRaw->streams) {
            auto streamRaw = reinterpret_cast<AVStreamCompat*>(fmtRaw->streams[bestStream]);

            if (streamRaw)
                subTimeBase = streamRaw->time_base;
        }
    }

    double tbVal = 1e-9;

    if (subTimeBase.den != 0) {

        tbVal = static_cast<double>(subTimeBase.num) / static_cast<double>(subTimeBase.den);
    }

    VSubLog(L"[VIDI] Sub: time_base = %d/%d", subTimeBase.num, subTimeBase.den);

    // --------------------------------------------------------
    // Read subtitle packets
    // --------------------------------------------------------

    AVPacketRaw* pkt = reinterpret_cast<AVPacketRaw*>(m_ff.av_packet_alloc());

    if (!pkt) {

        VSubLog(L"[VIDI] Sub: AVPacket allocation failed");

        FreeFile();

        return false;
    }

    int fed = 0;
    int fontsFed = 0;
    int scanned = 0;

    while (scanned < 100000 && m_ff.av_read_frame(m_fmtCtx, reinterpret_cast<AVPacket*>(pkt)) >= 0) {
        const int si = pkt->stream_index;

        // ----------------------------------------------------
        // Ignore unrelated streams
        // ----------------------------------------------------

        if (si != bestStream && (si < 0 || si >= MAX_STREAMS || !isAttachment[si])) {
            m_ff.av_packet_unref(reinterpret_cast<AVPacket*>(pkt));

            ++scanned;
            continue;
        }

        if (pkt->data && pkt->size > 0) {
            // =================================================
            // Subtitle packet
            // =================================================

            if (si == bestStream) {

                int dataSize = pkt->size;

                // Remove trailing NUL
                while (dataSize > 0 && pkt->data[dataSize - 1] == '\0') {
                    --dataSize;
                }

                if (dataSize > 0) {

                    // ------------------------------------------------
                    // Start time
                    // ------------------------------------------------

                    long long timecodeMs = 0;

                    if (pkt->pts != AV_NOPTS_VALUE) {

                        const double ptsMs = static_cast<double>(pkt->pts) * tbVal * 1000.0;

                        if (ptsMs > -9.22e18 && ptsMs < 9.22e18) {
                            timecodeMs = static_cast<long long>(std::llround(ptsMs));
                        }
                    }

                    if (timecodeMs < 0)
                        timecodeMs = 0;

                    // ------------------------------------------------
                    // IMPORTANT:
                    //
                    // Pakai duration asli packet.
                    //
                    // JANGAN:
                    //
                    // nextPTS - currentPTS
                    //
                    // karena ASS event boleh overlap.
                    // ------------------------------------------------

                    long long durationMs = 5000;

                    if (pkt->duration > 0) {

                        const double duration = static_cast<double>(pkt->duration) * tbVal * 1000.0;

                        if (duration > 0 && duration < 9.22e18) {
                            durationMs = static_cast<long long>(std::llround(duration));
                        }
                    }

                    // ------------------------------------------------
                    // Fallback
                    // ------------------------------------------------

                    if (durationMs <= 0)
                        durationMs = 5000;

                    // ------------------------------------------------
                    // Minimum duration
                    // ------------------------------------------------

                    if (durationMs < 1)
                        durationMs = 1;

                    // ------------------------------------------------
                    // Maximum safety duration
                    // ------------------------------------------------

                    if (durationMs > 30000)
                        durationMs = 30000;

                    // ------------------------------------------------
                    // Debug
                    // ------------------------------------------------

                    if (fed < 20) {

                        int previewSize = dataSize;

                        if (previewSize > 100)
                            previewSize = 100;

                        std::string preview(reinterpret_cast<char*>(pkt->data), previewSize);
                        VSubLog(L"[VIDI] Sub: packet[%d] "
                                L"start=%lldms "
                                L"duration=%lldms "
                                L"text='%hs'",
                                fed, timecodeMs, durationMs, preview.c_str());
                    }

                    // ------------------------------------------------
                    // Feed libass
                    //
                    // HANYA SEKALI saat Open().
                    // ------------------------------------------------

                    if (m_assRenderer.ProcessChunk(reinterpret_cast<const char*>(pkt->data), dataSize, timecodeMs,
                                                   durationMs)) {
                        ++fed;
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

                if (fontNames.empty()) {

                    fontNames.push_back("ATTACHMENT_" + std::to_string(si));
                }

                for (const auto& fontName : fontNames) {
                    m_assRenderer.AddFont(fontName.c_str(), reinterpret_cast<const char*>(fontData), fontSize);
                }

                ++fontsFed;

                if (fontsFed <= 10) {

                    std::string names;

                    for (size_t i = 0; i < fontNames.size(); ++i) {
                        if (i != 0)
                            names += ", ";

                        names += "'";
                        names += fontNames[i];
                        names += "'";
                    }

                    VSubLog(L"[VIDI] Sub: font attachment "
                            L"[%hs] loaded",
                            names.c_str());
                }
            }
        }

        m_ff.av_packet_unref(reinterpret_cast<AVPacket*>(pkt));

        ++scanned;
    }

    m_ff.av_packet_free(reinterpret_cast<AVPacket**>(&pkt));

    VSubLog(L"[VIDI] Sub: fed %d subtitle packets", fed);

    // --------------------------------------------------------
    // Load attachment fonts from codecpar extradata
    // --------------------------------------------------------

    {
        auto fmtRaw = reinterpret_cast<AVFormatContextCompat*>(m_fmtCtx);

        if (fmtRaw) {

            int extraFonts = 0;

            for (unsigned int si = 0; si < fmtRaw->nb_streams; ++si) {
                if (static_cast<int>(si) == bestStream) {
                    continue;
                }

                auto streamRaw = reinterpret_cast<AVStreamCompat*>(fmtRaw->streams[si]);

                if (!streamRaw || !streamRaw->codecpar) {
                    continue;
                }

                if (streamRaw->codecpar->codec_type != AVMEDIA_TYPE_ATTACHMENT) {
                    continue;
                }

                if (!streamRaw->codecpar->extradata || streamRaw->codecpar->extradata_size <= 0) {
                    continue;
                }

                const uint8_t* data = streamRaw->codecpar->extradata;

                const int size = streamRaw->codecpar->extradata_size;

                std::vector<std::string> fontNames = ExtractTtfFontNames(data, size);

                if (fontNames.empty()) {

                    fontNames.push_back("ATTACHMENT_" + std::to_string(si));
                }

                for (const auto& fontName : fontNames) {
                    m_assRenderer.AddFont(fontName.c_str(), reinterpret_cast<const char*>(data), size);
                }

                ++extraFonts;
            }

            VSubLog(L"[VIDI] Sub: %d attachment fonts "
                    L"loaded from extradata",
                    extraFonts);
        }
    }

    // --------------------------------------------------------
    // Track diagnostics
    // --------------------------------------------------------

    ASS_Track* track = m_assRenderer.GetTrack();

    if (track) {

        VSubLog(L"[VIDI] Sub: ASS track "
                L"events=%d styles=%d",
                track->n_events, track->n_styles);

        if (track->n_styles > 0 && track->styles) {
            for (int i = 0; i < track->n_styles; ++i) {
                ASS_Style& style = track->styles[i];

                VSubLog(L"[VIDI] Sub: style[%d] "
                        L"name='%hs' "
                        L"font='%hs' "
                        L"align=%d "
                        L"marginV=%d",
                        i, style.Name ? style.Name : "?", style.FontName ? style.FontName : "?", style.Alignment,
                        static_cast<int>(style.MarginV));
            }
        }
    }

    // --------------------------------------------------------
    // PlayRes
    // --------------------------------------------------------

    int playResX = 1280;
    int playResY = 720;

    if (track) {

        if (track->PlayResX > 0)
            playResX = track->PlayResX;

        if (track->PlayResY > 0)
            playResY = track->PlayResY;
    }

    // --------------------------------------------------------
    // Initial libass frame/storage size
    // --------------------------------------------------------

    m_assRenderer.SetFrameSize(playResX, playResY);

    m_assRenderer.SetStorageSize(playResX, playResY);

    // --------------------------------------------------------
    // Font fallback
    // --------------------------------------------------------

    m_assRenderer.SetFonts(L"Yu Gothic", L"Yu Gothic");

    // --------------------------------------------------------
    // Let libass process ReadOrder normally.
    // --------------------------------------------------------

    m_assRenderer.SetCheckReadorder(true);

    VSubLog(L"[VIDI] Sub: PlayRes=%dx%d", playResX, playResY);

    // --------------------------------------------------------
    // Ready
    // --------------------------------------------------------

    m_loaded = true;
    m_fileOpen = true;

    VSubLog(L"[VIDI] Sub: subtitle system READY");

    return true;
}

// ============================================================
// GetSubtitleStreamCount
// ============================================================

int SubtitleReader::GetSubtitleStreamCount() const {
    if (!m_fileOpen)
        return 0;

    return m_subtitleStreamIndex >= 0 ? 1 : 0;
}

// ============================================================
// GetSubtitleStreams
// ============================================================

std::vector<SubtitleInfo> SubtitleReader::GetSubtitleStreams() const {
    std::vector<SubtitleInfo> result;

    if (!m_fileOpen || m_subtitleStreamIndex < 0) {
        return result;
    }

    SubtitleInfo info{};

    info.streamIndex = m_subtitleStreamIndex;

    info.language = "";
    info.title = "";

    info.codecId = m_subtitleCodecId;

    info.isTextBased = true;

    result.push_back(info);

    return result;
}

// ============================================================
// RenderFrame
//
// Seek / rewind / fast-forward:
//
// TIDAK:
// - membuka file lagi
// - membaca packet lagi
// - ProcessChunk lagi
// - membuat ASS track baru
//
// Hanya render berdasarkan time baru.
// ============================================================

RenderResult SubtitleReader::RenderFrame(double timeSeconds) {
    RenderResult result;

    result.changed = false;

    if (!m_loaded)
        return result;

    if (timeSeconds < 0.0)
        timeSeconds = 0.0;

    const double timeMsDouble = timeSeconds * 1000.0;

    if (timeMsDouble > 9.22e18 || timeMsDouble < -9.22e18) {
        return result;
    }

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
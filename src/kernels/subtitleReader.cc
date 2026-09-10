#include "../../include/kernels/subtitleReader.hh"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cstdarg>
#include <cmath>
#include <cstring>
#include <vector>
#include <map>

namespace kernelPlayerVidi {

static std::string DecodeNameString(const uint8_t* strPtr, uint16_t length, uint16_t encodingID) {
    std::string result;
    if (encodingID == 1) {
        for (int j = 0; j + 1 < length; j += 2) {
            char c = (char)strPtr[j + 1];
            if (c >= 32 && c < 127)
                result += c;
        }
    } else {
        for (int j = 0; j < length; j++) {
            char c = (char)strPtr[j];
            if (c >= 32 && c < 127)
                result += c;
        }
    }
    return result;
}

static std::vector<std::string> ExtractTtfFontNames(const uint8_t* data, int size) {
    std::vector<std::string> names;
    if (size < 12)
        return names;
    uint16_t numTables = (data[4] << 8) | data[5];
    const uint8_t* nameTable = nullptr;
    int nameTableLen = 0;
    for (int i = 0; i < numTables; i++) {
        int off = 12 + i * 16;
        if (off + 16 > size)
            break;
        if (memcmp(data + off, "name", 4) == 0) {
            nameTableLen = (data[off + 12] << 24) | (data[off + 13] << 16) | (data[off + 14] << 8) | data[off + 15];
            int nameTableOffset =
                (data[off + 8] << 24) | (data[off + 9] << 16) | (data[off + 10] << 8) | data[off + 11];
            if (nameTableOffset + nameTableLen <= size)
                nameTable = data + nameTableOffset;
            break;
        }
    }
    if (!nameTable || nameTableLen < 6)
        return names;
    uint16_t nameCount = (nameTable[2] << 8) | nameTable[3];
    uint16_t stringOffset = (nameTable[4] << 8) | nameTable[5];
    const uint8_t* stringStorage = nameTable + stringOffset;

    auto tryAddName = [&](const std::string& n) {
        if (!n.empty()) {
            for (auto& existing : names)
                if (existing == n)
                    return;
            names.push_back(n);
        }
    };

    int targetIDs[] = {4, 1, 6, 2};
    for (int tid : targetIDs) {
        for (int i = 0; i < nameCount; i++) {
            int recOff = 6 + i * 12;
            if (recOff + 12 > nameTableLen)
                break;
            uint16_t platformID = (nameTable[recOff + 0] << 8) | nameTable[recOff + 1];
            uint16_t encodingID = (nameTable[recOff + 2] << 8) | nameTable[recOff + 3];
            uint16_t nameID = (nameTable[recOff + 6] << 8) | nameTable[recOff + 7];
            uint16_t strLength = (nameTable[recOff + 8] << 8) | nameTable[recOff + 9];
            uint16_t strOffset = (nameTable[recOff + 10] << 8) | nameTable[recOff + 11];
            if (nameID != tid)
                continue;
            if (platformID != 3 && platformID != 1)
                continue;
            const uint8_t* strPtr = stringStorage + strOffset;
            if (strPtr + strLength > data + size || strLength == 0)
                continue;
            std::string decoded = DecodeNameString(strPtr, strLength, encodingID);
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
      m_dllsLoaded(false),
      m_fileOpen(false) {}

SubtitleReader::~SubtitleReader() {
    Close();
}

// ============================================================
// FreeFile / Close
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

void SubtitleReader::Close() {
    FreeFile();
}

void SubtitleReader::FullShutdown() {
    FreeFile();
    FreeFFmpegDlls();
}

// ============================================================
// Deteksi karaoke dari raw event text
// ============================================================

static long long ExtractKaraokeDuration(const char* data, int size) {
    if (!data || size < 4)
        return 0;
    std::string s(data, size);
    long long totalCs = 0;
    size_t pos = 0;
    while ((pos = s.find("\\k", pos)) != std::string::npos) {
        // Skip \kf, \ko, \kt — ambil angka setelah 'k'
        size_t numStart = pos + 2;
        if (numStart < s.size()) {
            char c = s[numStart];
            if (c == 'f' || c == 'o' || c == 't')
                numStart++;
        }
        // Baca angka sampai '}' atau non-digit
        size_t numEnd = numStart;
        while (numEnd < s.size() && (isdigit(s[numEnd]) || s[numEnd] == '.'))
            numEnd++;
        if (numEnd > numStart) {
            try {
                double val = std::stod(s.substr(numStart, numEnd - numStart));
                totalCs += (long long)(val + 0.5);
            } catch (...) {
            }
        }
        pos = numEnd;
    }
    return totalCs * 10; // centiseconds → milliseconds
}

static bool HasKaraokeTag(const char* data, int size) {
    if (!data || size < 4)
        return false;
    std::string s(data, size);
    return s.find("{\\k") != std::string::npos || s.find("{\\kf") != std::string::npos ||
           s.find("{\\ko") != std::string::npos;
}

// ============================================================
// Open — single pass detect + index
// ============================================================

bool SubtitleReader::Open(const wchar_t* videoPath) {
    FreeFile();
    if (!LoadFFmpegDlls())
        return false;

    // 1. Init libass
    if (!m_assRenderer.Initialize()) {
        VSubLog(L"[VIDI] Sub: libass init failed");
        return false;
    }

    std::string utf8Path = WideToUtf8(videoPath);
    VSubLog(L"[VIDI] Sub: opening %s", videoPath);

    int ret = m_ff.avformat_open_input(&m_fmtCtx, utf8Path.c_str(), nullptr, nullptr);
    if (ret < 0) {
        VSubLog(L"[VIDI] Sub: avformat_open_input failed (err=%d)", ret);
        return false;
    }

    ret = m_ff.avformat_find_stream_info(m_fmtCtx, nullptr);
    if (ret < 0) {
        VSubLog(L"[VIDI] Sub: avformat_find_stream_info failed");
        FreeFile();
        return false;
    }

    // 2. Cari subtitle stream terbaik
    static const int MAX_STREAMS = 256;
    bool isAttachment[MAX_STREAMS] = {};

    int bestStream = -1;
    if (m_ff.av_find_best_stream) {
        bestStream = m_ff.av_find_best_stream(m_fmtCtx, AVMEDIA_TYPE_SUBTITLE, -1, -1, nullptr, 0);
    }

    // Fallback: kalau av_find_best_stream gagal, scan max 5000 packet
    if (bestStream < 0) {
        AVPacketRaw* pktFb = reinterpret_cast<AVPacketRaw*>(m_ff.av_packet_alloc());
        int textCounts[MAX_STREAMS] = {};
        int totalPackets[MAX_STREAMS] = {};
        int subPacketFed = 0;
        while (subPacketFed < 5000 && m_ff.av_read_frame(m_fmtCtx, reinterpret_cast<AVPacket*>(pktFb)) >= 0) {
            int si = pktFb->stream_index;
            if (si >= 0 && si < MAX_STREAMS) {
                totalPackets[si]++;
                if (pktFb->data && pktFb->size > 4 && LooksLikeSubtitleText(pktFb->data, pktFb->size))
                    textCounts[si]++;
            }
            m_ff.av_packet_unref(reinterpret_cast<AVPacket*>(pktFb));
            subPacketFed++;
        }
        m_ff.av_seek_frame(m_fmtCtx, -1, 0, 0);
        for (int i = 0; i < MAX_STREAMS; ++i) {
            if (textCounts[i] > 0 && textCounts[i] >= totalPackets[i] / 3) {
                if (bestStream < 0 || textCounts[i] > textCounts[bestStream])
                    bestStream = i;
            }
        }
        m_ff.av_packet_free(reinterpret_cast<AVPacket**>(&pktFb));
    }

    if (bestStream < 0) {
        VSubLog(L"[VIDI] Sub: no subtitle stream detected");
        return false;
    }

    m_subtitleStreamIndex = bestStream;
    VSubLog(L"[VIDI] Sub: stream %d (detected via av_find_best_stream)", bestStream);

    // Detect attachment streams via codec_type
    {
        auto fmtRaw2 = reinterpret_cast<AVFormatContextCompat*>(m_fmtCtx);
        for (unsigned int si = 0; si < fmtRaw2->nb_streams && si < MAX_STREAMS; ++si) {
            auto sRaw2 = reinterpret_cast<AVStreamCompat*>(fmtRaw2->streams[si]);
            if (sRaw2 && sRaw2->codecpar && sRaw2->codecpar->codec_type == AVMEDIA_TYPE_ATTACHMENT)
                isAttachment[si] = true;
        }
        int attCount = 0;
        for (int i = 0; i < MAX_STREAMS; i++)
            if (isAttachment[i])
                attCount++;
        VSubLog(L"[VIDI] Sub: found %d attachment streams", attCount);
    }

    // 3. Feed codec_private ke libass
    //    MKV subtitle stream punya extradata = codec_private (ASS header)
    {
        auto fmtRaw = reinterpret_cast<AVFormatContextCompat*>(m_fmtCtx);
        if (bestStream >= 0 && bestStream < (int)fmtRaw->nb_streams && fmtRaw->streams) {
            auto streamRaw = reinterpret_cast<AVStreamCompat*>(fmtRaw->streams[bestStream]);
            if (streamRaw && streamRaw->codecpar) {
                auto parRaw = streamRaw->codecpar;
                if (parRaw->extradata && parRaw->extradata_size > 0) {
                    m_assRenderer.LoadTrackFromMemory(reinterpret_cast<const char*>(parRaw->extradata),
                                                      parRaw->extradata_size);
                    VSubLog(L"[VIDI] Sub: loaded codec_private (%d bytes) into libass", parRaw->extradata_size);
                } else {
                    VSubLog(L"[VIDI] Sub: codec_private kosong, pakai default header");
                    const char* defaultHeader =
                        "[Script Info]\r\n"
                        "ScriptType: v4.00+\r\n"
                        "PlayResX: 1280\r\n"
                        "PlayResY: 720\r\n"
                        "WrapStyle: 0\r\n"
                        "\r\n"
                        "[V4+ Styles]\r\n"
                        "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, "
                        "Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, "
                        "Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\r\n"
                        "Style: "
                        "Default,Arial,48,&H00FFFFFF,&H000000FF,&H00000000,&H64000000,-1,0,0,0,100,100,0,0,1,2,2,2,10,"
                        "10,10,1\r\n"
                        "\r\n"
                        "[Events]\r\n"
                        "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\r\n";
                    m_assRenderer.LoadTrackFromMemory(defaultHeader, (int)strlen(defaultHeader));
                }
            }
        }
    }

    // 4. Feed subtitle packets ke libass — hanya baca stream subtitle + attachment
    if (m_ff.av_seek_frame)
        m_ff.av_seek_frame(m_fmtCtx, -1, 0, 0);

    AVRational subTimeBase = {1, 1000000000};
    {
        auto fmtRawTb = reinterpret_cast<AVFormatContextCompat*>(m_fmtCtx);
        if (bestStream >= 0 && bestStream < (int)fmtRawTb->nb_streams && fmtRawTb->streams) {
            auto sRawTb = reinterpret_cast<AVStreamCompat*>(fmtRawTb->streams[bestStream]);
            if (sRawTb)
                subTimeBase = sRawTb->time_base;
        }
    }
    VSubLog(L"[VIDI] Sub: time_base = %d/%d", subTimeBase.num, subTimeBase.den);
    if (m_ff.av_seek_frame)
        m_ff.av_seek_frame(m_fmtCtx, -1, 0, 0);

    double tbVal = (subTimeBase.den != 0) ? (double)subTimeBase.num / subTimeBase.den : 1e-9;

    // Kumpulkan semua subtitle event dulu untuk hitung durasi dari gap antar event
    struct SubEvent {
        std::vector<char> data;
        long long timecodeMs;
        bool isKaraoke;
    };
    std::vector<SubEvent> events;
    events.reserve(10000);

    AVPacketRaw* pkt = reinterpret_cast<AVPacketRaw*>(m_ff.av_packet_alloc());
    int fed = 0;
    int fontsFed = 0;
    int subPacketsRead = 0;
    while (subPacketsRead < 100000 && m_ff.av_read_frame(m_fmtCtx, reinterpret_cast<AVPacket*>(pkt)) >= 0) {
        int si = pkt->stream_index;
        if (si != bestStream && (si < 0 || si >= MAX_STREAMS || !isAttachment[si])) {
            m_ff.av_packet_unref(reinterpret_cast<AVPacket*>(pkt));
            continue;
        }
        subPacketsRead++;
        if (pkt->data && pkt->size > 0) {
            if (si == bestStream) {
                int dataSize = pkt->size;
                while (dataSize > 0 && pkt->data[dataSize - 1] == '\0')
                    dataSize--;
                if (dataSize > 0) {
                    long long timecodeMs = 0;
                    if (pkt->pts != AV_NOPTS_VALUE && pkt->pts != 0)
                        timecodeMs = (long long)(pkt->pts * tbVal * 1000.0);
                    SubEvent ev;
                    ev.data.assign(reinterpret_cast<char*>(pkt->data), reinterpret_cast<char*>(pkt->data) + dataSize);
                    ev.timecodeMs = timecodeMs;
                    ev.isKaraoke = HasKaraokeTag(ev.data.data(), static_cast<int>(ev.data.size()));
                    events.push_back(std::move(ev));
                }
            } else if (si >= 0 && si < MAX_STREAMS && isAttachment[si] && fontsFed < 100) {
                const uint8_t* d = reinterpret_cast<const uint8_t*>(pkt->data);
                std::vector<std::string> fontNames = ExtractTtfFontNames(d, pkt->size);
                if (fontNames.empty())
                    fontNames.push_back("ATTACHMENT_" + std::to_string(si));
                for (auto& fn : fontNames) {
                    m_assRenderer.AddFont(fn.c_str(), reinterpret_cast<const char*>(pkt->data), pkt->size);
                }
                fontsFed++;
                if (fontsFed <= 5) {
                    std::string nameList;
                    for (size_t ni = 0; ni < fontNames.size(); ni++) {
                        if (ni > 0)
                            nameList += ", ";
                        nameList += "'" + fontNames[ni] + "'";
                    }
                    VSubLog(L"[VIDI] Sub: loaded font [%hs] (%d bytes, stream %d)", nameList.c_str(), pkt->size, si);
                }
            }
        }
        m_ff.av_packet_unref(reinterpret_cast<AVPacket*>(pkt));
    }
    m_ff.av_packet_free(reinterpret_cast<AVPacket**>(&pkt));

    // Sort by timecode
    std::sort(events.begin(), events.end(),
              [](const SubEvent& a, const SubEvent& b) { return a.timecodeMs < b.timecodeMs; });

    // Build map: timecode karaoke → durasi dari \k tags
    std::map<long long, long long> karaokeDurMap;
    for (size_t i = 0; i < events.size(); i++) {
        if (events[i].isKaraoke) {
            long long dur = ExtractKaraokeDuration(events[i].data.data(), static_cast<int>(events[i].data.size()));
            if (dur > 0)
                karaokeDurMap[events[i].timecodeMs] = dur;
        }
    }

    // Diagnostic: log 10 event pertama untuk debug
    int diagCount = (events.size() < 10) ? (int)events.size() : 10;
    for (int i = 0; i < diagCount; i++) {
        std::string preview(events[i].data.data(), (events[i].data.size() > 80) ? 80 : events[i].data.size());
        VSubLog(L"[VIDI] Sub: diag[%d] t=%lld karaoke=%d data='%hs'", i, events[i].timecodeMs, events[i].isKaraoke,
                preview.c_str());
    }
    VSubLog(L"[VIDI] Sub: karaokeDurMap has %d entries", (int)karaokeDurMap.size());
    for (auto& [k, v] : karaokeDurMap) {
        VSubLog(L"[VIDI] Sub:   karaDur t=%lld dur=%lld", k, v);
    }

    // Feed ke libass dengan durasi berbeda untuk karaoke vs translate
    // Cap duration agar subtitle tidak stay terlalu lama
    const long long MAX_DUR_MS = 7000;
    const long long MIN_DUR_MS = 500;
    int karaokeCount = 0;
    int translateCount = 0;
    int inheritedCount = 0;
    for (size_t i = 0; i < events.size(); i++) {
        long long durMs = 3000;
        if (events[i].isKaraoke) {
            // Karaoke: durasi dari \k tags, minimum gap ke event berikutnya
            karaokeCount++;
            long long karaokeDurMs =
                ExtractKaraokeDuration(events[i].data.data(), static_cast<int>(events[i].data.size()));
            long long gapMs = 3000;
            if (i + 1 < events.size()) {
                gapMs = events[i + 1].timecodeMs - events[i].timecodeMs;
                if (gapMs <= 0)
                    gapMs = 1000;
            }
            durMs = (karaokeDurMs > gapMs) ? karaokeDurMs : gapMs;
            if (durMs > MAX_DUR_MS)
                durMs = MAX_DUR_MS;
            if (durMs < MIN_DUR_MS)
                durMs = MIN_DUR_MS;
        } else {
            // Translate: cek apakah ada karaoke event dalam window ±500ms
            translateCount++;
            bool inherited = false;
            for (auto& [kTime, kDur] : karaokeDurMap) {
                long long diff = events[i].timecodeMs - kTime;
                if (diff >= -500 && diff <= 500) {
                    durMs = kDur;
                    inherited = true;
                    inheritedCount++;
                    break;
                }
            }
            if (inherited) {
                if (durMs > MAX_DUR_MS)
                    durMs = MAX_DUR_MS;
                if (durMs < MIN_DUR_MS)
                    durMs = MIN_DUR_MS;
            }
            if (!inherited) {
                // Gap ke event berikutnya dengan timecode LEBIH BESAR
                for (size_t j = i + 1; j < events.size(); j++) {
                    if (events[j].timecodeMs > events[i].timecodeMs) {
                        durMs = events[j].timecodeMs - events[i].timecodeMs;
                        break;
                    }
                }
                if (durMs <= 0)
                    durMs = 3000;
                if (durMs > MAX_DUR_MS)
                    durMs = MAX_DUR_MS;
                if (durMs < MIN_DUR_MS)
                    durMs = MIN_DUR_MS;
            }
        }
        m_assRenderer.ProcessChunk(events[i].data.data(), static_cast<int>(events[i].data.size()), events[i].timecodeMs,
                                   durMs);
        fed++;
    }
    VSubLog(L"[VIDI] Sub: fed %d events (karaoke=%d, translate=%d, inherited=%d)", fed, karaokeCount, translateCount,
            inheritedCount);

    // Load fonts from attachment streams via codecpar->extradata
    {
        auto fmtRaw3 = reinterpret_cast<AVFormatContextCompat*>(m_fmtCtx);
        fontsFed = 0;
        for (unsigned int si = 0; si < fmtRaw3->nb_streams; ++si) {
            if ((int)si == bestStream)
                continue;
            auto sRaw3 = reinterpret_cast<AVStreamCompat*>(fmtRaw3->streams[si]);
            if (!sRaw3 || !sRaw3->codecpar)
                continue;
            if (sRaw3->codecpar->codec_type != AVMEDIA_TYPE_ATTACHMENT)
                continue;
            if (!sRaw3->codecpar->extradata || sRaw3->codecpar->extradata_size <= 0)
                continue;

            const uint8_t* d = sRaw3->codecpar->extradata;
            int dsize = sRaw3->codecpar->extradata_size;
            std::vector<std::string> fontNames = ExtractTtfFontNames(d, dsize);
            if (fontNames.empty())
                fontNames.push_back("ATTACHMENT_" + std::to_string(si));
            for (auto& fn : fontNames) {
                m_assRenderer.AddFont(fn.c_str(), reinterpret_cast<const char*>(d), dsize);
            }
            fontsFed++;
        }
        VSubLog(L"[VIDI] Sub: %d font attachments loaded from extradata", fontsFed);
    }

    // Diagnostic: cek isi track
    if (m_assRenderer.GetTrack()) {
        VSubLog(L"[VIDI] Sub: track n_events=%d n_styles=%d", m_assRenderer.GetTrack()->n_events,
                m_assRenderer.GetTrack()->n_styles);
        if (m_assRenderer.GetTrack()->n_styles > 0 && m_assRenderer.GetTrack()->styles) {
            auto& s = m_assRenderer.GetTrack()->styles[0];
            VSubLog(L"[VIDI] Sub: style[0] FontName='%hs' FontSize=%.0f", s.FontName ? s.FontName : "?", s.FontSize);
            // Log semua unique font names yang dipakai
            for (int si = 0; si < m_assRenderer.GetTrack()->n_styles; si++) {
                auto& st = m_assRenderer.GetTrack()->styles[si];
                VSubLog(L"[VIDI] Sub:   style[%d] Name='%hs' Font='%hs' Align=%d MarginV=%d", si,
                        st.Name ? st.Name : "?", st.FontName ? st.FontName : "?", st.Alignment, (int)st.MarginV);
            }
        }
    }

    // Diagnostic: hitung unique style names + Effect fields
    if (m_assRenderer.GetTrack() && m_assRenderer.GetTrack()->n_events > 0) {
        std::map<std::string, int> styleUsage;
        std::map<std::string, int> effectUsage;
        for (int i = 0; i < m_assRenderer.GetTrack()->n_events; i++) {
            auto& ev = m_assRenderer.GetTrack()->events[i];
            if (ev.Style >= 0 && ev.Style < m_assRenderer.GetTrack()->n_styles) {
                const char* name = m_assRenderer.GetTrack()->styles[ev.Style].Name;
                if (name)
                    styleUsage[name]++;
            }
            if (ev.Effect && ev.Effect[0])
                effectUsage[ev.Effect]++;
        }
        VSubLog(L"[VIDI] Sub: unique style usage (%d styles):", (int)styleUsage.size());
        for (auto& [name, count] : styleUsage) {
            VSubLog(L"[VIDI] Sub:   '%hs' = %d events", name.c_str(), count);
        }
        VSubLog(L"[VIDI] Sub: unique effect usage (%d effects):", (int)effectUsage.size());
        for (auto& [name, count] : effectUsage) {
            VSubLog(L"[VIDI] Sub:   '%hs' = %d events", name.c_str(), count);
        }
    }

    // 7. Set frame size dari PlayRes yang terdeteksi di ASS header
    VSubLog(L"[VIDI] Sub: setting frame size...");
    int playResX = 1280, playResY = 720; // default
    if (m_assRenderer.GetTrack()) {
        if (m_assRenderer.GetTrack()->PlayResX > 0)
            playResX = m_assRenderer.GetTrack()->PlayResX;
        if (m_assRenderer.GetTrack()->PlayResY > 0)
            playResY = m_assRenderer.GetTrack()->PlayResY;
    }
    m_assRenderer.SetFrameSize(playResX, playResY);
    m_assRenderer.SetStorageSize(playResX, playResY);
    VSubLog(L"[VIDI] Sub: setting fonts...");
    m_assRenderer.SetFonts(L"Arial", L"Arial");
    VSubLog(L"[VIDI] Sub: setting check readorder...");
    m_assRenderer.SetCheckReadorder(false);
    if (m_assRenderer.GetTrack()) {
        for (int i = 0; i < m_assRenderer.GetTrack()->n_styles; i++) {
            m_assRenderer.GetTrack()->styles[i].PrimaryColour = 0x00FFFFFF;
        }
    }
    VSubLog(L"[VIDI] Sub: PlayRes %dx%d", playResX, playResY);

    m_loaded = true;
    m_fileOpen = true;
    return true;
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

std::vector<SubtitleReader::RenderedBitmap> SubtitleReader::RenderFrame(double timeSeconds) {
    std::vector<RenderedBitmap> result;
    if (!m_loaded)
        return result;

    long long timeMs = (long long)(timeSeconds * 1000.0);
    auto assImages = m_assRenderer.RenderFrame(timeMs);

    for (auto& img : assImages) {
        RenderedBitmap rb;
        rb.x = img.x;
        rb.y = img.y;
        rb.width = img.width;
        rb.height = img.height;
        rb.color = img.color;
        rb.bitmap = std::move(img.bitmap);
        result.push_back(std::move(rb));
    }
    static double lastLogTime = -2.0;
    if (timeSeconds - lastLogTime >= 2.0) {
        VSubLog(L"[VIDI] Sub: RenderFrame(%.1fs -> %lld ms) => %d bitmaps", timeSeconds, timeMs, (int)result.size());
        lastLogTime = timeSeconds;
    }
    return result;
}
} // namespace kernelPlayerVidi

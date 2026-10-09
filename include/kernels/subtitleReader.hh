#ifndef SUBTITLE_READER_HH
#define SUBTITLE_READER_HH

#include <windows.h>
#include <string>
#include <vector>
#include <atomic>

#include "ffmpeg_dynload.hh"
#include "assRenderer.hh"

namespace kernelPlayerVidi {

// MEDIATYPE_Subtitle tidak dideklarasikan di strmif.h SDK
static const GUID GUID_MediaTypeSubtitle = {
    0x736c6774, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
static const CLSID CLSID_LAVSplitter = {0x171252A0, 0x8820, 0x4AFE, {0x9D, 0xF8, 0x5C, 0x92, 0xB2, 0xD6, 0x6B, 0x04}};
// "LAV Splitter Source" = file-source yang implement IFileSourceFilter
static const CLSID CLSID_LAVSplitterSource = {
    0xB98D13E7, 0x55DB, 0x4385, {0xA3, 0x3D, 0x09, 0xFD, 0x1B, 0xA2, 0x63, 0x38}};
static const CLSID CLSID_LAVVideo = {0xEE30215D, 0x164F, 0x4A92, {0xA4, 0xEB, 0x9D, 0x4C, 0x13, 0x39, 0x0F, 0x9F}};
static const CLSID CLSID_LAVAudio = {0xE8E73B6B, 0x4CB3, 0x44A4, {0xBE, 0x99, 0x4F, 0x7B, 0xCB, 0x96, 0xE4, 0x91}};
// VSFilter / DirectVobSub (xy-VSFilter juga memakai CLSID ini)
static const CLSID CLSID_DirectVobSub = {0x93a22e7a, 0x1291, 0x45c5, {0xba, 0x6f, 0x6b, 0x54, 0x29, 0xeb, 0x7a, 0x53}};
// [FIX ANTI-HIJAU] VMR-7 (Video Mixing Renderer). Tanpa ini Intelligent Connect
// bisa jatuh ke legacy Video Renderer yang frame idle-nya berupa GRADIENT HIJAU +
// logo blur khas quartz.dll -- muncul saat maximize/repaint tanpa frame baru.
static const CLSID CLSID_VMR7 = {0x87A59784, 0x25CF, 0x4A13, {0x9B, 0xBE, 0x0D, 0xE8, 0x85, 0x58, 0xFF, 0xC5}};

struct SubtitleInfo {
    int streamIndex;
    std::string language;
    std::string title;
    int codecId;
    bool isTextBased;
};

struct SubtitlePacketData {
    std::vector<uint8_t> data;
    long long ptsMs = 0;
    long long durationMs = 0;
    bool hasValidDuration = false;
};

class SubtitleReader {
  private:
    HMODULE m_hAvFormatDll;
    HMODULE m_hAvCodecDll;
    HMODULE m_hAvUtilDll;

    FFmpegFuncs m_ff;

    AVFormatContext* m_fmtCtx;
    AVCodecContext* m_codecCtx;

    int m_subtitleStreamIndex;
    int m_subtitleCodecId;

    bool m_dllsLoaded;
    bool m_fileOpen;

    AssRenderer m_assRenderer;

    std::atomic<bool> m_loaded{false};
    // [FIX] Flag untuk membatalkan proses scan subtitle secara graceful
    std::atomic<bool> m_cancelRequested{false};

    bool LoadFFmpegDlls();
    void FreeFFmpegDlls();
    void FreeFile();

  public:
    SubtitleReader();
    ~SubtitleReader();

    bool Open(const wchar_t* videoPath);

    void Close();
    void FullShutdown();

    // [FIX] Minta thread subtitle berhenti tanpa TerminateThread
    void RequestCancel() {
        m_cancelRequested.store(true);
    }

    bool IsLoaded() const {
        return m_loaded;
    }

    bool IsOpen() const {
        return m_fileOpen;
    }

    int GetSubtitleStreamCount() const;

    std::vector<SubtitleInfo> GetSubtitleStreams() const;

    AssRenderer& GetAssRenderer() {
        return m_assRenderer;
    }

    RenderResult RenderFrame(double timeSeconds);
};

} // namespace kernelPlayerVidi

#endif // SUBTITLE_READER_HH
#ifndef SUBTITLE_READER_HH
#define SUBTITLE_READER_HH

#include <windows.h>
#include <string>
#include <vector>
#include <atomic>
#include "ffmpeg_dynload.hh"
#include "assRenderer.hh"

namespace kernelPlayerVidi {

struct SubtitleInfo {
    int streamIndex;
    std::string language;
    std::string title;
    int codecId;
    bool isTextBased;
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

    bool LoadFFmpegDlls();
    void FreeFFmpegDlls();
    void FreeFile();

  public:
    SubtitleReader();
    ~SubtitleReader();

    bool Open(const wchar_t* videoPath);
    void Close();
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

    // Untuk gui.cc render
    struct RenderedBitmap {
        int x, y, width, height;
        uint32_t color;
        std::vector<uint8_t> bitmap;
    };
    std::vector<RenderedBitmap> RenderFrame(double timeSeconds);
};

} // namespace kernelPlayerVidi
#endif
#ifndef SUBTITLE_READER_HH
#define SUBTITLE_READER_HH

#include <windows.h>
#include <string>
#include <vector>
#include "ffmpeg_dynload.hh"

namespace kernelPlayerVidi {

struct SubtitleInfo {
    int streamIndex;
    std::string language;
    std::string title;
    int codecId;
    bool isTextBased;
};

struct SubtitleEntry {
    double startSeconds;
    double endSeconds;
    std::wstring text;
    double posX = -1; // from \pos(x,y), -1 = not specified
    double posY = -1;
    int alignment = 0; // \an tag: 1-9 (0 = tidak ada)
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
    int m_timeBaseDen;
    int m_timeBaseNum;

    bool m_dllsLoaded;
    bool m_fileOpen;
    double m_playResX = 0;
    double m_playResY = 0;

    bool LoadFFmpegDlls();
    void FreeFFmpegDlls();
    void FreeFile();
    void DetectAndBuildInOnePass();
    void BuildSubtitleIndex();

  public:
    SubtitleReader();
    ~SubtitleReader();

    bool Open(const wchar_t* videoPath);
    void Close();

    bool IsOpen() const {
        return m_fileOpen;
    }
    int GetSubtitleStreamCount() const;
    std::vector<SubtitleInfo> GetSubtitleStreams() const;

    std::vector<SubtitleEntry> m_subtitleIndex;
    std::wstring GetSubtitleAt(double timeSeconds);
    std::vector<SubtitleEntry> GetActiveSubtitles(double timeSeconds);
    double GetPlayResX() const {
        return m_playResX;
    }
    double GetPlayResY() const {
        return m_playResY;
    }

    struct TimedText {
        double startSeconds;
        double endSeconds;
        std::wstring text;
    };

    std::vector<TimedText> ReadSubtitles(double timeStart, double timeEnd);
};

} // namespace kernelPlayerVidi

#endif // SUBTITLE_READER_HH

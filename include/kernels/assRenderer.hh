#ifndef ASS_RENDERER_HH
#define ASS_RENDERER_HH

#include <windows.h>
#include <string>
#include <vector>
#include <ass/ass.h>

namespace kernelPlayerVidi {

struct RenderedBitmap {
    int x;
    int y;
    int width;
    int height;
    uint32_t color; // ASS_RGBA
    std::vector<uint8_t> bitmap; // alpha-only 8-bit
};

class AssRenderer {
  private:
    ASS_Library* m_library;
    ASS_Renderer* m_renderer;
    ASS_Track* m_track;
    int m_frameWidth;
    int m_frameHeight;
    int m_storageWidth;
    int m_storageHeight;
    bool m_initialized;

  public:
    AssRenderer();
    ~AssRenderer();

    bool Initialize();
    void Shutdown();

    bool SetFrameSize(int width, int height);
    bool SetStorageSize(int width, int height);
    bool SetFonts(const std::wstring& defaultFont = L"",
                  const std::wstring& defaultFamily = L"");

    bool LoadTrackFromMemory(const char* data, int size);
    bool ProcessChunk(const char* data, int size,
                      long long timecodeMs, long long durationMs);
    void FlushEvents();
    void SetCheckReadorder(bool check);

    void AddFont(const char* name, const char* data, int dataSize);
    void ClearFonts();

    std::vector<RenderedBitmap> RenderFrame(long long timeMs);
    ASS_Track* GetTrack() { return m_track; }
    ASS_Library* GetLibrary() { return m_library; }
};

} // namespace kernelPlayerVidi

#endif // ASS_RENDERER_HH

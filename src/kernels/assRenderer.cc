#include "../../include/kernels/assRenderer.hh"

#include <cstdio>
#include <cstdarg>
#include <string>

namespace kernelPlayerVidi {

// ============================================================
// LIBASS LOG CALLBACK
// ============================================================

static void AssLogCallback(int level, const char* fmt, va_list args, void* data) {
    if (level > 8)
        return;

    char buf[1024] = {};

    vsnprintf(buf, sizeof(buf), fmt, args);

    wchar_t wbuf[1024] = {};

    int converted = MultiByteToWideChar(CP_UTF8, 0, buf, -1, wbuf, static_cast<int>(std::size(wbuf)));

    if (converted <= 0) {
        OutputDebugStringW(L"[libass] ");
        OutputDebugStringA(buf);
        OutputDebugStringW(L"\n");
        return;
    }

    OutputDebugStringW(L"[libass] ");
    OutputDebugStringW(wbuf);
    OutputDebugStringW(L"\n");
}

// ============================================================
// UTF-8 CONVERSION
// ============================================================

static std::string WideToUtf8String(const std::wstring& text) {
    if (text.empty())
        return {};

    int required = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);

    if (required <= 0)
        return {};

    std::string result(static_cast<size_t>(required), '\0');

    int written = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, result.data(), required, nullptr, nullptr);

    if (written <= 0)
        return {};

    if (!result.empty() && result.back() == '\0')
        result.pop_back();

    return result;
}

// ============================================================
// CONSTRUCTOR
// ============================================================

AssRenderer::AssRenderer()
    : m_library(nullptr),
      m_renderer(nullptr),
      m_track(nullptr),
      m_frameWidth(0),
      m_frameHeight(0),
      m_storageWidth(0),
      m_storageHeight(0),
      m_initialized(false) {}

// ============================================================
// DESTRUCTOR
// ============================================================

AssRenderer::~AssRenderer() {
    Shutdown();
}

// ============================================================
// INITIALIZE LIBASS
// ============================================================

bool AssRenderer::Initialize() {

    if (m_initialized)
        return true;

    m_library = ass_library_init();

    if (!m_library)
        return false;

    ass_set_message_cb(m_library, AssLogCallback, nullptr);

    // Izinkan libass mengambil font dari attachment.
    ass_set_extract_fonts(m_library, 1);

    // Font Windows.
    ass_set_fonts_dir(m_library, "C:\\Windows\\Fonts");

    m_renderer = ass_renderer_init(m_library);

    if (!m_renderer) {

        ass_library_done(m_library);

        m_library = nullptr;

        return false;
    }

    // Gunakan shaping kompleks supaya:
    // - Unicode
    // - Japanese
    // - Arabic
    // - kombinasi karakter
    // - karaoke ASS
    // dapat diproses dengan benar.
    ass_set_shaper(m_renderer, ASS_SHAPING_COMPLEX);

    m_initialized = true;

    return true;
}

// ============================================================
// SHUTDOWN
// ============================================================

void AssRenderer::Shutdown() {

    if (m_track) {

        ass_free_track(m_track);

        m_track = nullptr;
    }

    if (m_renderer) {

        ass_renderer_done(m_renderer);

        m_renderer = nullptr;
    }

    if (m_library) {

        ass_clear_fonts(m_library);

        ass_library_done(m_library);

        m_library = nullptr;
    }

    m_frameWidth = 0;
    m_frameHeight = 0;

    m_storageWidth = 0;
    m_storageHeight = 0;

    m_initialized = false;
}

// ============================================================
// SET FRAME SIZE
// ============================================================

bool AssRenderer::SetFrameSize(int width, int height) {

    if (!m_renderer)
        return false;

    if (width <= 0 || height <= 0)
        return false;

    m_frameWidth = width;
    m_frameHeight = height;

    ass_set_frame_size(m_renderer, width, height);

    return true;
}

// ============================================================
// SET STORAGE SIZE
// ============================================================

bool AssRenderer::SetStorageSize(int width, int height) {

    if (!m_renderer)
        return false;

    if (width <= 0 || height <= 0)
        return false;

    m_storageWidth = width;
    m_storageHeight = height;

    ass_set_storage_size(m_renderer, width, height);

    return true;
}

// ============================================================
// SET DEFAULT FONTS
// ============================================================

bool AssRenderer::SetFonts(const std::wstring& defaultFont, const std::wstring& defaultFamily) {

    if (!m_renderer)
        return false;

    std::string utf8Default = WideToUtf8String(defaultFont);

    std::string utf8Family = WideToUtf8String(defaultFamily);

    ass_set_fonts(m_renderer,

                  utf8Default.empty() ? nullptr : utf8Default.c_str(),

                  utf8Family.empty() ? nullptr : utf8Family.c_str(),

                  ASS_FONTPROVIDER_DIRECTWRITE,

                  nullptr,

                  0);

    return true;
}

// ============================================================
// LOAD ASS TRACK / CODEC PRIVATE
// ============================================================

bool AssRenderer::LoadTrackFromMemory(const char* data, int size) {

    if (!m_library)
        return false;

    if (!data || size <= 0)
        return false;

    if (m_track) {

        ass_free_track(m_track);

        m_track = nullptr;
    }

    m_track = ass_new_track(m_library);

    if (!m_track)
        return false;

    ass_process_codec_private(m_track, data, size);

    return true;
}

// ============================================================
// PROCESS ASS EVENT
// ============================================================

bool AssRenderer::ProcessChunk(const char* data, int size, long long timecodeMs, long long durationMs) {

    if (!m_track)
        return false;

    if (!data || size <= 0)
        return false;

    if (durationMs <= 0)
        return false;

    ass_process_chunk(m_track, data, size, timecodeMs, durationMs);

    return true;
}

// ============================================================
// FLUSH EVENTS
// ============================================================

void AssRenderer::FlushEvents() {

    if (m_track)
        ass_flush_events(m_track);
}

// ============================================================
// READ ORDER CHECK
// ============================================================

void AssRenderer::SetCheckReadorder(bool check) {

    if (!m_track)
        return;

    ass_set_check_readorder(m_track, check ? 1 : 0);
}

// ============================================================
// ADD FONT ATTACHMENT
// ============================================================

void AssRenderer::AddFont(const char* name, const char* data, int dataSize) {

    if (!m_library)
        return;

    if (!name || !data || dataSize <= 0)
        return;

    ass_add_font(m_library, name, data, dataSize);
}

// ============================================================
// CLEAR FONTS
// ============================================================

void AssRenderer::ClearFonts() {

    if (m_library)
        ass_clear_fonts(m_library);
}

// ============================================================
// RENDER FRAME
// ============================================================

RenderResult AssRenderer::RenderFrame(long long timeMs) {

    RenderResult result;

    result.changed = false;

    if (!m_renderer || !m_track)
        return result;

    int detectChange = 0;

    ASS_Image* img = ass_render_frame(m_renderer, m_track, timeMs, &detectChange);

    result.changed = (detectChange != 0);

    // ========================================================
    // CONVERT ASS_IMAGE -> RenderedBitmap
    // ========================================================

    while (img) {

        if (img->w > 0 && img->h > 0 && img->bitmap) {

            RenderedBitmap rb;

            rb.x = img->dst_x;
            rb.y = img->dst_y;

            rb.width = img->w;
            rb.height = img->h;

            rb.color = img->color;

            // ------------------------------------------------
            // IMPORTANT:
            //
            // ASS_Image::stride TIDAK selalu sama dengan width.
            //
            // Jadi copy per baris.
            // ------------------------------------------------

            const size_t rowBytes = static_cast<size_t>(img->w);

            const size_t totalBytes = rowBytes * static_cast<size_t>(img->h);

            rb.bitmap.reserve(totalBytes);

            for (int y = 0; y < img->h; ++y) {

                const uint8_t* row = img->bitmap + static_cast<size_t>(y) * static_cast<size_t>(img->stride);

                rb.bitmap.insert(rb.bitmap.end(), row, row + rowBytes);
            }

            result.bitmaps.push_back(std::move(rb));
        }

        img = img->next;
    }

    return result;
}

} // namespace kernelPlayerVidi
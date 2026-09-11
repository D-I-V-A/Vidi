#include "../../include/kernels/assRenderer.hh"
#include <cstdio>
#include <cstdarg>

namespace kernelPlayerVidi {

static void AssLogCallback(int level, const char* fmt, va_list args, void* data) {
    if (level > 8)
        return;
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    wchar_t wbuf[1024];
    MultiByteToWideChar(CP_UTF8, 0, buf, -1, wbuf, 1024);
    OutputDebugStringW(L"[libass] ");
    OutputDebugStringW(wbuf);
    OutputDebugStringW(L"\n");
}

AssRenderer::AssRenderer()
    : m_library(nullptr),
      m_renderer(nullptr),
      m_track(nullptr),
      m_frameWidth(0),
      m_frameHeight(0),
      m_storageWidth(0),
      m_storageHeight(0),
      m_initialized(false) {}

AssRenderer::~AssRenderer() {
    Shutdown();
}

bool AssRenderer::Initialize() {
    if (m_initialized)
        return true;

    m_library = ass_library_init();
    if (!m_library)
        return false;

    ass_set_message_cb(m_library, AssLogCallback, nullptr);
    ass_set_extract_fonts(m_library, 1);
    ass_set_fonts_dir(m_library, "C:\\Windows\\Fonts");

    m_renderer = ass_renderer_init(m_library);
    if (!m_renderer) {
        ass_library_done(m_library);
        m_library = nullptr;
        return false;
    }

    ass_set_shaper(m_renderer, ASS_SHAPING_COMPLEX);

    m_initialized = true;
    return true;
}

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
    m_initialized = false;
}

bool AssRenderer::SetFrameSize(int width, int height) {
    if (!m_renderer)
        return false;
    m_frameWidth = width;
    m_frameHeight = height;
    ass_set_frame_size(m_renderer, width, height);
    return true;
}

bool AssRenderer::SetStorageSize(int width, int height) {
    if (!m_renderer)
        return false;
    m_storageWidth = width;
    m_storageHeight = height;
    ass_set_storage_size(m_renderer, width, height);
    return true;
}

bool AssRenderer::SetFonts(const std::wstring& defaultFont, const std::wstring& defaultFamily) {
    if (!m_renderer)
        return false;
    std::string utf8Default;
    std::string utf8Family;
    if (!defaultFont.empty()) {
        int len = WideCharToMultiByte(CP_UTF8, 0, defaultFont.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (len > 0) {
            utf8Default.resize(len - 1);
            WideCharToMultiByte(CP_UTF8, 0, defaultFont.c_str(), -1, &utf8Default[0], len, nullptr, nullptr);
        }
    }
    if (!defaultFamily.empty()) {
        int len = WideCharToMultiByte(CP_UTF8, 0, defaultFamily.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (len > 0) {
            utf8Family.resize(len - 1);
            WideCharToMultiByte(CP_UTF8, 0, defaultFamily.c_str(), -1, &utf8Family[0], len, nullptr, nullptr);
        }
    }
    ass_set_fonts(m_renderer, utf8Default.empty() ? nullptr : utf8Default.c_str(),
                  utf8Family.empty() ? nullptr : utf8Family.c_str(), ASS_FONTPROVIDER_DIRECTWRITE, nullptr, 0);
    return true;
}

bool AssRenderer::LoadTrackFromMemory(const char* data, int size) {
    if (!m_library || !data || size <= 0)
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

bool AssRenderer::ProcessChunk(const char* data, int size, long long timecodeMs, long long durationMs) {
    if (!m_track || !data || size <= 0)
        return false;
    ass_process_chunk(m_track, data, size, timecodeMs, durationMs);
    return true;
}

void AssRenderer::FlushEvents() {
    if (m_track)
        ass_flush_events(m_track);
}

void AssRenderer::SetCheckReadorder(bool check) {
    if (m_track)
        ass_set_check_readorder(m_track, check ? 1 : 0);
}

void AssRenderer::AddFont(const char* name, const char* data, int dataSize) {
    if (!m_library || !name || !data || dataSize <= 0)
        return;
    ass_add_font(m_library, name, data, dataSize);
}

void AssRenderer::ClearFonts() {
    if (m_library)
        ass_clear_fonts(m_library);
}

RenderResult AssRenderer::RenderFrame(long long timeMs) {
    RenderResult result;
    result.changed = false;
    if (!m_renderer || !m_track)
        return result;

    int detectChange = 0;
    ASS_Image* img = ass_render_frame(m_renderer, m_track, timeMs, &detectChange);
    result.changed = (detectChange != 0);
    while (img) {
        if (img->w > 0 && img->h > 0) {
            RenderedBitmap rb;
            rb.x = img->dst_x;
            rb.y = img->dst_y;
            rb.width = img->w;
            rb.height = img->h;
            rb.color = img->color;
            rb.bitmap.reserve(img->w * img->h);
            for (int y = 0; y < img->h; y++) {
                rb.bitmap.insert(rb.bitmap.end(), img->bitmap + y * img->stride,
                                 img->bitmap + y * img->stride + img->w);
            }
            result.bitmaps.push_back(std::move(rb));
        }
        img = img->next;
    }
    return result;
}

} // namespace kernelPlayerVidi

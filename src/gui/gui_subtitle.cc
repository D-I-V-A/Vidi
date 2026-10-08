#include "../../include/gui/gui.hh"
#include "../../include/kernels/ids.hh"

#include <cstdint>
#include <cstring>

namespace guiVidi {

// ============================================================
// Subtitle bitmap hash
//
// Digunakan untuk mendeteksi apakah hasil subtitle benar-benar
// berubah.
//
// Kalau sama:
// - tidak rebuild DIB
// - tidak UpdateLayeredWindow
// - overlay lama dipertahankan
// ============================================================

template <typename BitmapContainer> uint64_t HashSubtitleBitmaps(const BitmapContainer& bitmaps) {
    uint64_t hash = 1469598103934665603ULL;

    auto HashBytes = [&hash](const void* data, size_t size) {
        const uint8_t* bytes = static_cast<const uint8_t*>(data);

        for (size_t i = 0; i < size; ++i) {
            hash ^= bytes[i];

            hash *= 1099511628211ULL;
        }
    };

    for (const auto& b : bitmaps) {

        HashBytes(&b.x, sizeof(b.x));

        HashBytes(&b.y, sizeof(b.y));

        HashBytes(&b.width, sizeof(b.width));

        HashBytes(&b.height, sizeof(b.height));

        HashBytes(&b.color, sizeof(b.color));

        if (!b.bitmap.empty()) {

            HashBytes(b.bitmap.data(), b.bitmap.size());
        }
    }

    return hash;
}

void VideoPlayerGUI::BeginSubtitleSeekDelay() {
    // fungsi ini hanya untuk sembunyikan overlay pada subtitle
    // warning: jangan panggil method `HideAllSubOverlays()`
    // karena mengakibatkan value dari `m_subsHidden` menjadi true
    for (int i = 0; i < MAX_SUB_OVERLAYS; ++i) {
        if (m_hSubOverlay[i]) {
            ShowWindow(m_hSubOverlay[i], SW_HIDE);
        }
    }
    // subtitle akan boleh dirender kembali setelah 150ms
    m_subtitleSeekUntilTick = GetTickCount() + 150;
    m_subNeedsUpdate = true;
}

// ============================================================
// Create subtitle overlay
// ============================================================

void VideoPlayerGUI::CreateSubtitleOverlay(HWND hwnd) {
    (void)hwnd;

    static bool s_registered = false;

    if (!s_registered) {

        WNDCLASSW wc = {};

        wc.lpfnWndProc = DefWindowProcW;

        wc.hInstance = GetModuleHandleW(nullptr);

        wc.lpszClassName = L"VidiSubOverlay";

        RegisterClassW(&wc);

        s_registered = true;
    }

    for (int i = 0; i < MAX_SUB_OVERLAYS; ++i) {
        m_hSubOverlay[i] = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,

                                           L"VidiSubOverlay",

                                           L"",

                                           WS_POPUP,

                                           0, 0, 100, 40,

                                           hwnd, nullptr,

                                           GetModuleHandleW(nullptr), nullptr);

        m_hSubBmp[i] = nullptr;

        m_pSubBmpBits[i] = nullptr;

        m_subBmpW[i] = 0;
        m_subBmpH[i] = 0;
    }
}

// ============================================================
// Hide all subtitle overlays
// ============================================================

void VideoPlayerGUI::HideAllSubOverlays() {
    m_subsHidden = true;

    m_lastSubContentHash = 0;
    m_lastSubRenderTick = 0;

    m_subNeedsUpdate = false;

    m_lastSubFrameW = 0;
    m_lastSubFrameH = 0;

    for (int i = 0; i < MAX_SUB_OVERLAYS; ++i) {
        if (m_hSubOverlay[i]) {
            ShowWindow(m_hSubOverlay[i], SW_HIDE);
        }

        if (m_hSubBmp[i]) {
            DeleteObject(m_hSubBmp[i]);

            m_hSubBmp[i] = nullptr;

            m_pSubBmpBits[i] = nullptr;

            m_subBmpW[i] = 0;
            m_subBmpH[i] = 0;
        }
    }
}

// ============================================================
// Update subtitle displays
// ============================================================

void VideoPlayerGUI::UpdateSubtitleDisplays(double posSeconds, bool force) {

    // --------------------------------------------------------
    // Subtitle disabled
    // --------------------------------------------------------
    if (m_subsHidden)
        return;

    // setelah seek,jangan langsung render subtitle
    // tunggu video stabil terlebih dahulu
    if (m_subtitleSeekUntilTick != 0) {
        const DWORD now = GetTickCount();

        if (static_cast<LONG>(now - m_subtitleSeekUntilTick) < 0) {
            return;
        }

        m_subtitleSeekUntilTick = 0;
    }
    // Simpan posisi terakhir supaya fullscreen bisa melakukan
    // refresh subtitle pada posisi video terakhir.
    m_lastSubPosition = posSeconds;

    // --------------------------------------------------------
    // Subtitle belum loaded
    // --------------------------------------------------------

    if (!m_player.IsSubtitlesLoaded()) {
        static int skipCount = 0;

        if (++skipCount % 300 == 1) {
            OutputDebugStringW(L"[VIDI] gui: UpdateSub skipped (not loaded)\n");
        }

        return;
    }

    // --------------------------------------------------------
    // VSFilter active
    // --------------------------------------------------------

    if (m_player.IsVSFilterSubtitleActive()) {
        if (m_hSubOverlay[0]) {
            ShowWindow(m_hSubOverlay[0], SW_HIDE);
        }

        return;
    }

    // --------------------------------------------------------
    // Get video area
    //
    // Geometry harus dihitung SEBELUM throttle.
    // Ini penting ketika fullscreen mengubah posisi/ukuran
    // video area.
    // --------------------------------------------------------

    RECT videoRC = {};

    if (g_hVideoArea) {
        GetClientRect(g_hVideoArea, &videoRC);
    }

    POINT tl = {videoRC.left, videoRC.top};

    POINT br = {videoRC.right, videoRC.bottom};

    if (g_hVideoArea) {
        ClientToScreen(g_hVideoArea, &tl);
        ClientToScreen(g_hVideoArea, &br);
    }

    int vidX = tl.x;
    int vidY = tl.y;

    int vidW = br.x - tl.x;
    int vidH = br.y - tl.y;

    if (vidW <= 0 || vidH <= 0) {
        if (m_hSubOverlay[0]) {
            ShowWindow(m_hSubOverlay[0], SW_HIDE);
        }

        return;
    }

    // --------------------------------------------------------
    // Native video dimensions
    // --------------------------------------------------------

    int nativeW = 0;
    int nativeH = 0;

    m_player.GetNativeVideoSize(nativeW, nativeH);

    if (nativeW <= 0 || nativeH <= 0) {
        if (m_hSubOverlay[0]) {
            ShowWindow(m_hSubOverlay[0], SW_HIDE);
        }

        return;
    }

    // --------------------------------------------------------
    // Aspect-fit calculation
    //
    // Subtitle overlay mengikuti content video,
    // bukan seluruh black-bar area.
    // --------------------------------------------------------

    double scaleX = static_cast<double>(vidW) / static_cast<double>(nativeW);

    double scaleY = static_cast<double>(vidH) / static_cast<double>(nativeH);

    double scale = (scaleX < scaleY) ? scaleX : scaleY;

    int contentW = static_cast<int>(nativeW * scale);
    int contentH = static_cast<int>(nativeH * scale);

    int contentX = vidX + (vidW - contentW) / 2;
    int contentY = vidY + (vidH - contentH) / 2;

    if (contentW <= 0 || contentH <= 0) {
        if (m_hSubOverlay[0]) {
            ShowWindow(m_hSubOverlay[0], SW_HIDE);
        }

        return;
    }

    // --------------------------------------------------------
    // Detect geometry changes
    //
    // frameSizeChanged:
    //     ukuran subtitle render berubah
    //
    // overlayPositionChanged:
    //     posisi subtitle overlay berubah
    //
    // geometryChanged:
    //     salah satu dari keduanya berubah
    // --------------------------------------------------------

    const bool frameSizeChanged = contentW != m_lastSubFrameW || contentH != m_lastSubFrameH;

    const bool overlayPositionChanged = contentX != m_lastSubOverlayX || contentY != m_lastSubOverlayY;

    const bool geometryChanged = frameSizeChanged || overlayPositionChanged;

    // --------------------------------------------------------
    // Throttle subtitle GUI update
    //
    // Kalau geometry berubah, JANGAN throttle.
    //
    // Ini penting ketika:
    //     window -> fullscreen
    //     fullscreen -> window
    //     resize
    //
    // Walaupun bitmap subtitle sama, posisi overlay harus
    // tetap diperbarui.
    // --------------------------------------------------------

    DWORD now = GetTickCount();

    if (!force && !geometryChanged && m_lastSubRenderTick != 0 && now - m_lastSubRenderTick < 50) {

        if (m_hSubBmp[0] && m_subNeedsUpdate) {
            ShowWindow(m_hSubOverlay[0], SW_SHOW);
        }

        return;
    }

    m_lastSubRenderTick = now;

    // --------------------------------------------------------
    // Update libass render size only if display size changes.
    //
    // JANGAN memanggil SetFrameSize setiap frame.
    // --------------------------------------------------------

    if (frameSizeChanged) {
        auto& assRender = m_player.GetSubtitleReader().GetAssRenderer();

        assRender.SetStorageSize(contentW, contentH);
        assRender.SetFrameSize(contentW, contentH);

        m_lastSubFrameW = contentW;
        m_lastSubFrameH = contentH;

        // Render sebelumnya sudah tidak valid
        // karena ukuran frame berubah.
        m_lastSubContentHash = 0;
    }

    // --------------------------------------------------------
    // Simpan posisi overlay terakhir.
    // --------------------------------------------------------

    m_lastSubOverlayX = contentX;
    m_lastSubOverlayY = contentY;

    // --------------------------------------------------------
    // Render ASS at current video position.
    //
    // Ini TIDAK reload subtitle.
    // Track libass sudah ada sejak Open().
    // --------------------------------------------------------

    auto renderResult = m_player.GetSubtitleReader().RenderFrame(posSeconds);

    // --------------------------------------------------------
    // Tidak ada subtitle aktif.
    // --------------------------------------------------------

    if (renderResult.bitmaps.empty()) {
        if (m_hSubOverlay[0]) {
            ShowWindow(m_hSubOverlay[0], SW_HIDE);
        }

        m_subNeedsUpdate = false;
        m_lastSubContentHash = 0;

        return;
    }

    // --------------------------------------------------------
    // Kalau libass tidak menghasilkan bitmap baru tetapi
    // overlay sudah ada, tidak perlu composite ulang.
    //
    // Pengecualian:
    // geometry berubah atau force.
    // --------------------------------------------------------

    if (!force && !geometryChanged && !renderResult.changed && m_hSubBmp[0] && m_subNeedsUpdate) {

        ShowWindow(m_hSubOverlay[0], SW_SHOW);
        return;
    }

    // --------------------------------------------------------
    // Calculate visual hash
    // --------------------------------------------------------

    const uint64_t currentHash = HashSubtitleBitmaps(renderResult.bitmaps);

    // --------------------------------------------------------
    // Optimization
    //
    // Kalau bitmap subtitle sama persis dan geometry juga sama,
    // jangan:
    //
    // - memset DIB
    // - composite bitmap
    // - UpdateLayeredWindow
    //
    // Overlay lama masih benar.
    // --------------------------------------------------------

    if (!force && !geometryChanged && !renderResult.changed) {
        return;
    }

    // --------------------------------------------------------
    // Visual subtitle memang berubah
    // --------------------------------------------------------

    m_lastSubContentHash = currentHash;

    // --------------------------------------------------------
    // Recreate DIB only when size changed
    // --------------------------------------------------------

    if (m_hSubBmp[0] && (m_subBmpW[0] != contentW || m_subBmpH[0] != contentH)) {

        DeleteObject(m_hSubBmp[0]);

        m_hSubBmp[0] = nullptr;
        m_pSubBmpBits[0] = nullptr;

        m_subBmpW[0] = 0;
        m_subBmpH[0] = 0;
    }

    // --------------------------------------------------------
    // Create DIB
    // --------------------------------------------------------

    if (!m_hSubBmp[0]) {
        BITMAPINFO bmi = {};

        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

        bmi.bmiHeader.biWidth = contentW;

        bmi.bmiHeader.biHeight = -contentH;

        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        m_hSubBmp[0] = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &m_pSubBmpBits[0], nullptr, 0);

        m_subBmpW[0] = contentW;
        m_subBmpH[0] = contentH;
    }

    if (!m_hSubBmp[0] || !m_pSubBmpBits[0]) {
        return;
    }

    // --------------------------------------------------------
    // Clear bitmap
    // --------------------------------------------------------

    memset(m_pSubBmpBits[0], 0, static_cast<size_t>(contentW) * static_cast<size_t>(contentH) * 4);

    // --------------------------------------------------------
    // Composite libass images
    // --------------------------------------------------------

    for (const auto& b : renderResult.bitmaps) {
        if (b.bitmap.empty() || b.width <= 0 || b.height <= 0) {
            continue;
        }

        // ----------------------------------------------------
        // ASS color format:
        //
        // 0xRRGGBBTT
        //
        // TT:
        //     00 = opaque
        //     FF = transparent
        // ----------------------------------------------------

        const uint32_t c = b.color;

        const BYTE srcR = static_cast<BYTE>((c >> 24) & 0xFF);

        const BYTE srcG = static_cast<BYTE>((c >> 16) & 0xFF);

        const BYTE srcB = static_cast<BYTE>((c >> 8) & 0xFF);

        const BYTE assTransparency = static_cast<BYTE>(c & 0xFF);

        const BYTE assAlpha = static_cast<BYTE>(255 - assTransparency);

        if (assAlpha == 0)
            continue;

        // ----------------------------------------------------
        // Bitmap
        // ----------------------------------------------------

        for (int y = 0; y < b.height; ++y) {
            const BYTE* srcRow = b.bitmap.data() + static_cast<size_t>(y) * static_cast<size_t>(b.width);

            for (int x = 0; x < b.width; ++x) {
                const BYTE coverage = srcRow[x];

                if (coverage == 0)
                    continue;

                const int dx = b.x + x;
                const int dy = b.y + y;

                // ------------------------------------------------
                // Clip
                // ------------------------------------------------

                if ((unsigned)dx >= (unsigned)contentW || (unsigned)dy >= (unsigned)contentH) {
                    continue;
                }

                // ------------------------------------------------
                // ASS alpha * glyph coverage
                // ------------------------------------------------

                const BYTE srcAlpha =
                    static_cast<BYTE>((static_cast<int>(assAlpha) * static_cast<int>(coverage) + 127) / 255);

                if (srcAlpha == 0)
                    continue;

                DWORD* dst = static_cast<DWORD*>(m_pSubBmpBits[0]) +
                             static_cast<size_t>(dy) * static_cast<size_t>(contentW) + dx;

                // ------------------------------------------------
                // Destination BGRA
                // ------------------------------------------------

                const BYTE dstB = static_cast<BYTE>(*dst & 0xFF);

                const BYTE dstG = static_cast<BYTE>((*dst >> 8) & 0xFF);

                const BYTE dstR = static_cast<BYTE>((*dst >> 16) & 0xFF);

                const BYTE dstA = static_cast<BYTE>((*dst >> 24) & 0xFF);

                const int invA = 255 - srcAlpha;

                // ------------------------------------------------
                // Premultiply source
                // ------------------------------------------------

                const BYTE srcPR = static_cast<BYTE>((static_cast<int>(srcR) * static_cast<int>(srcAlpha) + 127) / 255);

                const BYTE srcPG = static_cast<BYTE>((static_cast<int>(srcG) * static_cast<int>(srcAlpha) + 127) / 255);

                const BYTE srcPB = static_cast<BYTE>((static_cast<int>(srcB) * static_cast<int>(srcAlpha) + 127) / 255);

                // ------------------------------------------------
                // Source-over
                // ------------------------------------------------

                const BYTE outR = static_cast<BYTE>(srcPR + (static_cast<int>(dstR) * invA + 127) / 255);

                const BYTE outG = static_cast<BYTE>(srcPG + (static_cast<int>(dstG) * invA + 127) / 255);

                const BYTE outB = static_cast<BYTE>(srcPB + (static_cast<int>(dstB) * invA + 127) / 255);

                const BYTE outA = static_cast<BYTE>(srcAlpha + (static_cast<int>(dstA) * invA + 127) / 255);

                *dst = (static_cast<DWORD>(outA) << 24) | (static_cast<DWORD>(outR) << 16) |
                       (static_cast<DWORD>(outG) << 8) | static_cast<DWORD>(outB);
            }
        }
    }

    // --------------------------------------------------------
    // Update layered window
    // --------------------------------------------------------

    HDC hdcScreen = GetDC(nullptr);

    if (!hdcScreen)
        return;

    HDC hMemDC = CreateCompatibleDC(hdcScreen);

    if (!hMemDC) {
        ReleaseDC(nullptr, hdcScreen);
        return;
    }

    HBITMAP hOld = static_cast<HBITMAP>(SelectObject(hMemDC, m_hSubBmp[0]));

    POINT ptDst = {contentX, contentY};

    SIZE sizeWnd = {contentW, contentH};

    POINT ptSrc = {0, 0};

    BLENDFUNCTION blend = {};

    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(m_hSubOverlay[0], hdcScreen, &ptDst, &sizeWnd, hMemDC, &ptSrc, 0, &blend, ULW_ALPHA);

    // --------------------------------------------------------
    // Cleanup DC
    // --------------------------------------------------------

    SelectObject(hMemDC, hOld);

    DeleteDC(hMemDC);

    ReleaseDC(nullptr, hdcScreen);

    // --------------------------------------------------------
    // Show overlay
    // --------------------------------------------------------

    ShowWindow(m_hSubOverlay[0], SW_SHOW);

    m_subNeedsUpdate = true;
}

} // namespace guiVidi
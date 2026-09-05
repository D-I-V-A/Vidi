#include "../../include/gui/gui.hh"
#include "../../include/kernels/ids.hh"
#include <functional>

namespace guiVidi {

// ==========================================
// SUBTITLE OVERLAY
// ==========================================
void VideoPlayerGUI::CreateSubtitleOverlay(HWND hwnd) {
    double dpi = GetDpiScale(hwnd);
    m_hSubFont = CreateFontW((int)(-22 * dpi), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");

    static bool s_registered = false;
    if (!s_registered) {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"VidiSubOverlay";
        RegisterClassW(&wc);
        s_registered = true;
    }

    for (int i = 0; i < MAX_SUB_OVERLAYS; i++) {
        m_hSubOverlay[i] =
            CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE, L"VidiSubOverlay",
                            L"", WS_POPUP, 0, 0, 100, 40, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        m_hSubBmp[i] = nullptr;
        m_pSubBmpBits[i] = nullptr;
        m_subBmpW[i] = 0;
        m_subBmpH[i] = 0;
    }
}

void VideoPlayerGUI::HideAllSubOverlays() {
    m_subsHidden = true;
    m_lastSubContentHash = 0;
    for (int i = 0; i < MAX_SUB_OVERLAYS; i++) {
        if (m_hSubOverlay[i])
            ShowWindow(m_hSubOverlay[i], SW_HIDE);
        if (m_hSubBmp[i]) {
            DeleteObject(m_hSubBmp[i]);
            m_hSubBmp[i] = nullptr;
            m_pSubBmpBits[i] = nullptr;
            m_subBmpW[i] = 0;
            m_subBmpH[i] = 0;
        }
    }
}

void VideoPlayerGUI::UpdateSubtitleDisplays(double posSeconds) {
    if (m_subsHidden)
        return;
    if (!m_player.IsSubtitlesLoaded()) {
        static int skipCount = 0;
        if (++skipCount % 300 == 1)
            OutputDebugStringW(L"[VIDI] gui: UpdateSub skipped (not loaded)\n");
        return;
    }

    RECT videoRC = {};
    if (g_hVideoArea)
        GetClientRect(g_hVideoArea, &videoRC);
    POINT tl = {videoRC.left, videoRC.top};
    POINT br = {videoRC.right, videoRC.bottom};
    if (g_hVideoArea) {
        ClientToScreen(g_hVideoArea, &tl);
        ClientToScreen(g_hVideoArea, &br);
    }
    int vidX = tl.x, vidY = tl.y;
    int vidW = br.x - tl.x, vidH = br.y - tl.y;

    int nativeW = 0, nativeH = 0;
    m_player.GetNativeVideoSize(nativeW, nativeH);
    double scaleX = (nativeW > 0) ? (double)vidW / nativeW : 1.0;
    double scaleY = (nativeH > 0) ? (double)vidH / nativeH : 1.0;
    double scale = (scaleX < scaleY) ? scaleX : scaleY;
    int contentW = (int)(nativeW * scale);
    int contentH = (int)(nativeH * scale);
    int contentX = vidX + (vidW - contentW) / 2;
    int contentY = vidY + (vidH - contentH) / 2;

    if (contentW <= 0 || contentH <= 0) {
        ShowWindow(m_hSubOverlay[0], SW_HIDE);
        return;
    }

    m_player.GetSubtitleReader().GetAssRenderer().SetFrameSize(contentW, contentH);

    auto bitmaps = m_player.GetSubtitleReader().RenderFrame(posSeconds);

    if (bitmaps.empty()) {
        ShowWindow(m_hSubOverlay[0], SW_HIDE);
        return;
    }

    size_t contentHash = bitmaps.size();
    for (auto& b : bitmaps) {
        contentHash ^= std::hash<int>{}(b.x) + 0x9e3779b9 + (contentHash << 6) + (contentHash >> 2);
        contentHash ^= std::hash<int>{}(b.y) + 0x9e3779b9 + (contentHash << 6) + (contentHash >> 2);
        contentHash ^= std::hash<int>{}(b.width) + 0x9e3779b9 + (contentHash << 6) + (contentHash >> 2);
        contentHash ^= std::hash<int>{}(b.height) + 0x9e3779b9 + (contentHash << 6) + (contentHash >> 2);
        contentHash ^= std::hash<uint32_t>{}(b.color) + 0x9e3779b9 + (contentHash << 6) + (contentHash >> 2);
    }

    if (m_hSubBmp[0] && m_subBmpW[0] == contentW && m_subBmpH[0] == contentH && m_lastSubContentHash == contentHash) {
        return;
    }

    if (m_hSubBmp[0] && (m_subBmpW[0] != contentW || m_subBmpH[0] != contentH)) {
        DeleteObject(m_hSubBmp[0]);
        m_hSubBmp[0] = nullptr;
    }
    if (!m_hSubBmp[0]) {
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = contentW;
        bmi.bmiHeader.biHeight = -contentH;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        m_hSubBmp[0] = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &m_pSubBmpBits[0], nullptr, 0);
        m_subBmpW[0] = contentW;
        m_subBmpH[0] = contentH;
    }

    if (!m_hSubBmp[0])
        return;

    memset(m_pSubBmpBits[0], 0, contentW * contentH * 4);

    for (auto& b : bitmaps) {
        uint32_t c = b.color;
        BYTE srcA = (c >> 24) & 0xFF;
        BYTE srcR = c & 0xFF;
        BYTE srcG = (c >> 8) & 0xFF;
        BYTE srcB = (c >> 16) & 0xFF;

        int dstX = b.x;
        int dstY = b.y;

        for (int y = 0; y < b.height; y++) {
            for (int x = 0; x < b.width; x++) {
                int dx = dstX + x;
                int dy = dstY + y;
                if (dx < 0 || dx >= contentW || dy < 0 || dy >= contentH)
                    continue;

                BYTE alpha = b.bitmap[y * b.width + x];
                if (alpha == 0)
                    continue;

                BYTE finalA = (BYTE)((int)srcA * alpha / 255);
                DWORD* dst = (DWORD*)m_pSubBmpBits[0] + dy * contentW + dx;

                BYTE oldB = *dst & 0xFF;
                BYTE oldG = (*dst >> 8) & 0xFF;
                BYTE oldR = (*dst >> 16) & 0xFF;
                BYTE oldA = (*dst >> 24) & 0xFF;

                int invA = 255 - finalA;
                BYTE newR = (BYTE)((srcR * finalA + oldR * invA) / 255);
                BYTE newG = (BYTE)((srcG * finalA + oldG * invA) / 255);
                BYTE newB = (BYTE)((srcB * finalA + oldB * invA) / 255);
                BYTE newA = finalA + (BYTE)((int)oldA * invA / 255);
                *dst = (newA << 24) | (newR << 16) | (newG << 8) | newB;
            }
        }
    }

    HDC hdcScreen = GetDC(NULL);
    HDC hMemDC = CreateCompatibleDC(hdcScreen);
    HBITMAP hOld = (HBITMAP)SelectObject(hMemDC, m_hSubBmp[0]);

    POINT ptDst = {contentX, contentY};
    SIZE sizeWnd = {contentW, contentH};
    POINT ptSrc = {0, 0};
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(m_hSubOverlay[0], hdcScreen, &ptDst, &sizeWnd, hMemDC, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(hMemDC, hOld);
    DeleteDC(hMemDC);
    ShowWindow(m_hSubOverlay[0], SW_SHOW);
    ReleaseDC(NULL, hdcScreen);

    m_lastSubContentHash = contentHash;
}

} // namespace guiVidi

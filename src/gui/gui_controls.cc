#include "../../include/gui/gui.hh"
#include "../../include/kernels/ids.hh"
#include <string>
#include <functional>
#include <cmath>

namespace guiVidi {

// ==========================================
// MEDIA READY — set range trackbar sesuai durasi asli video
// ==========================================
void VideoPlayerGUI::OnMediaReady() {
    m_cachedDuration = m_player.GetDuration();
    double dur = m_cachedDuration;
    double range = dur * 10.0;
    if (range < 100.0)
        range = 100.0;
    // Capping range maksimal 10000 (16 menit) agar tidak overflow
    if (range > 10000.0)
        range = 10000.0;
    m_progressRangeMax = range;
    SendMessage(g_hProgress, TBM_SETRANGEMIN, TRUE, 0);
    SendMessage(g_hProgress, TBM_SETRANGEMAX, TRUE, m_progressRangeMax);
    SetProgressPos(0);
    UpdateTimeLabel(0.0, dur);
    FitWindowToVideo();

    m_player.ShowVideoWindow();
}

// ==========================================
// COMMAND HANDLER
// ==========================================
void VideoPlayerGUI::OnCommand(WPARAM wParam, LPARAM lParam) {
    switch (LOWORD(wParam)) {
    case IDC_BTN_PLAY:
    case IDM_PLAYBACK_PLAY:
        if (m_isPlaying) {
            m_player.Pause();
            SetPlayPauseUI(false);
        } else {
            m_player.Play();
            SetPlayPauseUI(true);
        }
        break;

    case IDC_BTN_STOP:
    case IDM_PLAYBACK_STOP:
        m_player.Stop();
        SetPlayPauseUI(false);
        SetProgressPos(0);
        m_cachedDuration = 0.0;
        break;

    case IDC_BTN_SKIPBACK:
    case IDM_PLAYBACK_SKIPBACK: {
        double pos = m_player.GetPosition();
        m_player.Seek(pos > 10.0 ? pos - 10.0 : 0.0);
        break;
    }
    case IDC_BTN_SKIPFORWARD:
    case IDM_PLAYBACK_SKIPFWD: {
        double pos = m_player.GetPosition();
        m_player.Seek((pos + 10.0 < m_cachedDuration) ? pos + 10.0 : m_cachedDuration);
        break;
    }

    case IDC_BTN_FULLSCREEN:
    case IDM_VIEW_FULLSCREEN:
        if (m_isFullscreen)
            ExitFullscreen();
        else
            EnterFullscreen();
        break;

    case IDC_BTN_PLAYLIST:
        break;

    case IDC_BTN_LOOP:
        m_isLooping = !m_isLooping;
        SetToggleBtnState(g_hLoopBtn, m_isLooping);
        break;

    case IDC_BTN_SHUFFLE:
        m_isShuffle = !m_isShuffle;
        SetToggleBtnState(g_hShuffleBtn, m_isShuffle);
        break;

    case IDM_FILE_OPEN:
        OpenFileDialog();
        break;

    case IDM_FILE_EXIT:
        PostMessage(g_hMainWnd, WM_CLOSE, 0, 0);
        break;

    case IDM_AUDIO_VOLUP: {
        int vol = (int)SendMessage(g_hVolume, TBM_GETPOS, 0, 0);
        vol = (vol + 10 > VOL_MAX) ? VOL_MAX : vol + 10;
        SendMessage(g_hVolume, TBM_SETPOS, TRUE, vol);
        ApplyVolumeFromSlider(vol);
        UpdateVolumePercent(vol);
        break;
    }
    case IDM_AUDIO_VOLDOWN: {
        int vol = (int)SendMessage(g_hVolume, TBM_GETPOS, 0, 0);
        vol = (vol - 10 < 0) ? 0 : vol - 10;
        SendMessage(g_hVolume, TBM_SETPOS, TRUE, vol);
        ApplyVolumeFromSlider(vol);
        UpdateVolumePercent(vol);
        break;
    }
    case IDM_AUDIO_MUTE:
        ToggleMute();
        break;

    case IDM_APP_ESCAPE:
        if (m_isFullscreen)
            ExitFullscreen();
        break;

    case IDM_HELP_ABOUT:
        MessageBox(g_hMainWnd, L"Vidi Video Player\nDibangun dengan Win32 + DirectShow", L"About Vidi Player",
                   MB_OK | MB_ICONINFORMATION);
        break;
    }
}

// ==========================================
// TOGGLE MUTE
// ==========================================
void VideoPlayerGUI::ToggleMute() {
    if (!m_isMuted) {
        m_lastVolume = SendMessage(g_hVolume, TBM_GETPOS, 0, 0) / 100.0f;
        m_player.SetVolume(0.0f);
        m_player.SetDspGain(1.0f);
        SendMessage(g_hVolume, TBM_SETPOS, TRUE, 0);
        InvalidateRect(g_hVolume, nullptr, FALSE);
        UpdateVolumePercent(0);
        m_isMuted = true;
    } else {
        int back = (int)(m_lastVolume * 100.0f + 0.5f);
        SendMessage(g_hVolume, TBM_SETPOS, TRUE, back);
        ApplyVolumeFromSlider(back);
        InvalidateRect(g_hVolume, nullptr, FALSE);
        m_isMuted = false;
    }
}

void VideoPlayerGUI::SetToggleBtnState(HWND btn, bool active) {
    if (!btn)
        return;
    LONG style = GetWindowLong(btn, GWL_STYLE);
    if (active)
        style |= WS_BORDER;
    else
        style &= ~WS_BORDER;
    SetWindowLong(btn, GWL_STYLE, style);
    InvalidateRect(btn, nullptr, FALSE);
}

// ==========================================
// TIMER TICK — auto update posisi & label
// ==========================================
void VideoPlayerGUI::OnTimerTick() {
    if (m_isFullscreen) {
        POINT pt;
        GetCursorPos(&pt);
        if (pt.x != m_lastCursor.x || pt.y != m_lastCursor.y) {
            m_lastCursor = pt;
            PokeOSControls();
        }
    }
    if (m_isDraggingProgress)
        return;
    DWORD now = GetTickCount();

    // Adaptive interval berdasarkan durasi video
    int interval = 500; // default
    if (m_cachedDuration > 0.0) {
        if (m_cachedDuration < 60.0) {
            interval = 100; // < 1 menit: update setiap 100ms (akurasi tinggi)
        } else if (m_cachedDuration < 30 * 60) {
            interval = 500; // 1 - 30 menit: default (500ms)
        } else {
            interval = 2000; // > 30 menit: kurangi ke 2 detik
        }
    }

    if (now - m_lastDurCheckTick > interval) {
        m_lastDurCheckTick = now;
        double fresh = m_player.GetDuration();
        if (fresh > 0.0 && fabs(fresh - m_cachedDuration) > 0.5) {
            m_cachedDuration = fresh;
            int range = static_cast<int>(fresh * 10.0);
            if (range < 100)
                range = 100;
            // Capping range maksimal 10000 (16 menit) agar tidak overflow
            if (range > 10000)
                range = 10000;
            m_progressRangeMax = range;
            SendMessage(g_hProgress, TBM_SETRANGEMIN, TRUE, 0);
            SendMessage(g_hProgress, TBM_SETRANGEMAX, TRUE, m_progressRangeMax);
        }
    }
    double dur = m_cachedDuration;
    if (dur <= 0.0)
        return;
    if (m_hasPendingSeek) {
        double actualPos = m_player.GetPosition();
        DWORD elapsed = GetTickCount() - m_pendingSeekStartTick;
        bool settled = (fabs(actualPos - m_pendingSeekTarget) < 1.0) || (elapsed > 1500);

        if (settled) {
            m_hasPendingSeek = false;
        } else {
            if (dur > 0.0) {
                int sliderPos = static_cast<int>((m_pendingSeekTarget / dur) * m_progressRangeMax);
                SetProgressPos(sliderPos);
            }
            UpdateTimeLabel(m_pendingSeekTarget, dur);
            return;
        }
    }

    double pos = m_player.GetPosition();
    int sliderPos = static_cast<int>((pos / dur) * m_progressRangeMax);
    if (sliderPos < 0)
        sliderPos = 0;
    if (sliderPos > m_progressRangeMax)
        sliderPos = m_progressRangeMax;
    SetProgressPos(sliderPos);
    UpdateTimeLabel(pos, dur);

    UpdateSubtitleDisplays(pos);
}

void VideoPlayerGUI::UpdateTimeLabel(double posSeconds, double durSeconds) {
    wchar_t buf[64];
    int p = (int)posSeconds, d = (int)durSeconds;

    if (d <= 0) {
        swprintf_s(buf, L"--:-- / --:--");
    } else if (d >= 3600) {
        // < 1 jam: MM:SS
        swprintf_s(buf, L"%02d:%02d / %02d:%02d", p / 60, p % 60, d / 60, d % 60);

    } else {
        // >= 1 jam: HH:MM:SS
        swprintf_s(buf, L"%d:%02d:%02d / %d:%02d:%02d", p / 3600, (p % 3600) / 60, p % 60, d / 3600, (d % 3600) / 60,
                   d % 60);
    }

    SetWindowTextW(g_hTimeLabel, buf);
    InvalidateRect(g_hTimeLabel, nullptr, FALSE);

    int needed = MeasureStringWidth(g_hTimeLabel, m_hTimeFont, buf);
    RECT rc;
    GetWindowRect(g_hTimeLabel, &rc);
    if (needed > (rc.right - rc.left) && g_hMainWnd) {
        RECT rcC;
        GetClientRect(g_hMainWnd, &rcC);
        LayoutControls(rcC.right, rcC.bottom);
    }
}

void VideoPlayerGUI::SetPlayPauseUI(bool playing) {
    m_isPlaying = playing;
    if (m_hIconPlay && m_hIconPause) {
        HICON icon = playing ? m_hIconPause : m_hIconPlay;
        SendMessage(g_hPlayBtn, BM_SETIMAGE, IMAGE_ICON, (LPARAM)icon);
    }
    SetThreadExecutionState(playing ? (ES_CONTINUOUS | ES_DISPLAY_REQUIRED) : ES_CONTINUOUS);
    KillTimer(g_hMainWnd, ID_TIMER_UPDATE);
    SetTimer(g_hMainWnd, ID_TIMER_UPDATE, playing ? 33 : 500, nullptr);
}

void VideoPlayerGUI::OpenFileDialog() {
    HideAllSubOverlays();

    wchar_t filePath[MAX_PATH] = {0};
    OPENFILENAME ofn = {};

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hMainWnd;
    ofn.lpstrFilter = L"Video Files\0*.mp4;*.mkv;*.avi;*.mov;*.wmv;*.webm;*.m4v;*.ts;*.flv\0"
                      L"Audio Files\0*.mp3;*.aac;*.flac;*.wav;*.ogg\0"
                      L"All Files\0*.*\0\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

    if (!GetOpenFileName(&ofn))
        return;

    m_player.Stop();
    SetPlayPauseUI(false);
    SetProgressPos(0);
    UpdateTimeLabel(0.0, 0.0);
    m_cachedDuration = 0.0;
    m_hasPendingSeek = false;
    m_isDraggingProgress = false;

    if (m_player.OpenFile(filePath)) {
        m_player.Play();
        SetPlayPauseUI(true);
        m_subsHidden = false;
    } else {
        MessageBox(g_hMainWnd, L"Gagal membuka file. Format mungkin tidak didukung.", L"Vidi", MB_OK | MB_ICONERROR);
    }
}

// ==========================================
// CREATE MENU BAR
// ==========================================
void VideoPlayerGUI::CreateMenuBar(HWND hwnd) {
    HMENU hMenuBar = CreateMenu();

    HMENU hMedia = CreatePopupMenu();
    AppendMenu(hMedia, MF_STRING, IDM_FILE_OPEN, L"Open...\tCtrl+O");
    AppendMenu(hMedia, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hMedia, MF_STRING, IDM_FILE_EXIT, L"Exit\tAlt+F4");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hMedia, L"Media");

    HMENU hPlayback = CreatePopupMenu();
    AppendMenu(hPlayback, MF_STRING, IDM_PLAYBACK_PLAY, L"Play/Pause\tSpace");
    AppendMenu(hPlayback, MF_STRING, IDM_PLAYBACK_STOP, L"Stop\tS");
    AppendMenu(hPlayback, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hPlayback, MF_STRING, IDM_PLAYBACK_SKIPBACK, L"Skip Back 10s\tLeft");
    AppendMenu(hPlayback, MF_STRING, IDM_PLAYBACK_SKIPFWD, L"Skip Forward 10s\tRight");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hPlayback, L"Playback");

    HMENU hAudio = CreatePopupMenu();
    AppendMenu(hAudio, MF_STRING, IDM_AUDIO_VOLUP, L"Volume Up\tUp");
    AppendMenu(hAudio, MF_STRING, IDM_AUDIO_VOLDOWN, L"Volume Down\tDown");
    AppendMenu(hAudio, MF_STRING, IDM_AUDIO_MUTE, L"Mute\tM");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hAudio, L"Audio");

    HMENU hVideo = CreatePopupMenu();
    AppendMenu(hVideo, MF_STRING, IDM_VIEW_FULLSCREEN, L"Fullscreen\tF");
    AppendMenu(hVideo, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hVideo, MF_STRING, IDM_TAKE_SNAPSHOT, L"Take Snapshot");
    AppendMenu(hVideo, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hVideo, MF_STRING, IDM_ALWAYS_FIT_WINDOW, L"Fit Window");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hVideo, L"Video");

    HMENU hSubtitle = CreatePopupMenu();
    AppendMenu(hSubtitle, MF_STRING, IDM_SUB_ADD_FILE, L"Add Subtitle File...");
    AppendMenu(hSubtitle, MF_STRING, IDM_SUB_TRACK, L"Subtitle Track");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hSubtitle, L"Subtitle");

    HMENU hTools = CreatePopupMenu();
    AppendMenu(hTools, MF_STRING, IDM_EFFECTS_FILTERS, L"Effects and Filters");
    AppendMenu(hTools, MF_STRING, IDM_CODEC_INFO, L"Codec Information");
    AppendMenu(hTools, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hTools, MF_STRING, IDM_PREFERENCES, L"Preferences");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hTools, L"Tools");

    HMENU hView = CreatePopupMenu();
    AppendMenu(hView, MF_STRING, IDM_PLAYLIST, L"Playlist");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hView, L"View");

    HMENU hHelp = CreatePopupMenu();
    AppendMenu(hHelp, MF_STRING, IDM_HELP_ABOUT, L"About");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hHelp, L"Help");

    m_hMenuBar = hMenuBar;
    SetMenu(hwnd, hMenuBar);
}

// ==========================================
// CREATE CONTROLS
// ==========================================
void VideoPlayerGUI::CreateControls(HWND hwnd) {
    g_hMainWnd = hwnd;
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);
    double dpi = GetDpiScale(hwnd);

    m_hModernFont =
        CreateFontW((int)(-14 * dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                    CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
    m_hTimeFont = CreateFontW((int)(-15 * dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
    m_hTipFont = CreateFontW((int)(-12 * dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");

    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring exeDir = exePath;
    exeDir = exeDir.substr(0, exeDir.find_last_of(L'\\') + 1);
    std::wstring assetsDir = exeDir + L"assets\\";

    int iconSize = (int)(24 * dpi);
    m_hIconPlay = (HICON)LoadImageW(nullptr, (assetsDir + L"play-button-arrowhead.ico").c_str(), IMAGE_ICON, iconSize,
                                    iconSize, LR_LOADFROMFILE);
    m_hIconPause =
        (HICON)LoadImageW(nullptr, (assetsDir + L"pause.ico").c_str(), IMAGE_ICON, iconSize, iconSize, LR_LOADFROMFILE);
    m_hIconStop = (HICON)LoadImageW(nullptr, (assetsDir + L"stop-button.ico").c_str(), IMAGE_ICON, iconSize, iconSize,
                                    LR_LOADFROMFILE);
    m_hIconSkipBack = (HICON)LoadImageW(nullptr, (assetsDir + L"left-arrow.ico").c_str(), IMAGE_ICON, iconSize,
                                        iconSize, LR_LOADFROMFILE);
    m_hIconSkipForward = (HICON)LoadImageW(nullptr, (assetsDir + L"fast-forward.ico").c_str(), IMAGE_ICON, iconSize,
                                           iconSize, LR_LOADFROMFILE);
    m_hIconSpeaker = (HICON)LoadImageW(nullptr, (assetsDir + L"speaker.ico").c_str(), IMAGE_ICON, iconSize, iconSize,
                                       LR_LOADFROMFILE);
    m_hIconFullscreen = (HICON)LoadImageW(nullptr, (assetsDir + L"fullscreen.ico").c_str(), IMAGE_ICON, iconSize,
                                          iconSize, LR_LOADFROMFILE);
    m_hIconPlaylist = (HICON)LoadImageW(nullptr, (assetsDir + L"playlist.ico").c_str(), IMAGE_ICON, iconSize, iconSize,
                                        LR_LOADFROMFILE);
    m_hIconLoop =
        (HICON)LoadImageW(nullptr, (assetsDir + L"loop.ico").c_str(), IMAGE_ICON, iconSize, iconSize, LR_LOADFROMFILE);
    m_hIconShuffle = (HICON)LoadImageW(nullptr, (assetsDir + L"shuffle.ico").c_str(), IMAGE_ICON, iconSize, iconSize,
                                       LR_LOADFROMFILE);

    DWORD btnStyle = WS_CHILD | BS_ICON | BS_FLAT;

    g_hVideoArea = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_BLACKRECT, 0, 0, 100, 100, hwnd,
                                   nullptr, nullptr, nullptr);
    SetWindowSubclass(g_hVideoArea, VideoAreaSubclassProc, 2, (DWORD_PTR)this);

    g_hSkipBack =
        CreateWindowW(L"BUTTON", L"", btnStyle, 0, 0, 44, 44, hwnd, (HMENU)IDC_BTN_SKIPBACK, nullptr, nullptr);
    if (m_hIconSkipBack)
        SendMessage(g_hSkipBack, BM_SETIMAGE, IMAGE_ICON, (LPARAM)m_hIconSkipBack);

    g_hPlayBtn = CreateWindowW(L"BUTTON", L"", btnStyle, 0, 0, 48, 48, hwnd, (HMENU)IDC_BTN_PLAY, nullptr, nullptr);
    if (m_hIconPlay)
        SendMessage(g_hPlayBtn, BM_SETIMAGE, IMAGE_ICON, (LPARAM)m_hIconPlay);

    g_hStopBtn = CreateWindowW(L"BUTTON", L"", btnStyle, 0, 0, 44, 44, hwnd, (HMENU)IDC_BTN_STOP, nullptr, nullptr);
    if (m_hIconStop)
        SendMessage(g_hStopBtn, BM_SETIMAGE, IMAGE_ICON, (LPARAM)m_hIconStop);

    g_hSkipForward =
        CreateWindowW(L"BUTTON", L"", btnStyle, 0, 0, 44, 44, hwnd, (HMENU)IDC_BTN_SKIPFORWARD, nullptr, nullptr);
    if (m_hIconSkipForward)
        SendMessage(g_hSkipForward, BM_SETIMAGE, IMAGE_ICON, (LPARAM)m_hIconSkipForward);

    g_hFullscreenBtn =
        CreateWindowW(L"BUTTON", L"", btnStyle, 0, 0, 44, 44, hwnd, (HMENU)IDC_BTN_FULLSCREEN, nullptr, nullptr);
    if (m_hIconFullscreen)
        SendMessage(g_hFullscreenBtn, BM_SETIMAGE, IMAGE_ICON, (LPARAM)m_hIconFullscreen);

    g_hPlaylistBtn =
        CreateWindowW(L"BUTTON", L"", btnStyle, 0, 0, 44, 44, hwnd, (HMENU)IDC_BTN_PLAYLIST, nullptr, nullptr);
    if (m_hIconPlaylist)
        SendMessage(g_hPlaylistBtn, BM_SETIMAGE, IMAGE_ICON, (LPARAM)m_hIconPlaylist);

    g_hLoopBtn = CreateWindowW(L"BUTTON", L"", btnStyle, 0, 0, 44, 44, hwnd, (HMENU)IDC_BTN_LOOP, nullptr, nullptr);
    if (m_hIconLoop)
        SendMessage(g_hLoopBtn, BM_SETIMAGE, IMAGE_ICON, (LPARAM)m_hIconLoop);

    g_hShuffleBtn =
        CreateWindowW(L"BUTTON", L"", btnStyle, 0, 0, 44, 44, hwnd, (HMENU)IDC_BTN_SHUFFLE, nullptr, nullptr);
    if (m_hIconShuffle)
        SendMessage(g_hShuffleBtn, BM_SETIMAGE, IMAGE_ICON, (LPARAM)m_hIconShuffle);

    g_hProgress = CreateWindowExW(0, TRACKBAR_CLASS, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS, 0, 0, 100, 24,
                                  hwnd, (HMENU)IDC_PROGRESS, nullptr, nullptr);
    SetWindowTheme(g_hProgress, L" ", L" ");
    SendMessage(g_hProgress, TBM_SETRANGEMIN, TRUE, 0);
    SendMessage(g_hProgress, TBM_SETRANGEMAX, TRUE, m_progressRangeMax);
    SendMessage(g_hProgress, TBM_SETTHUMBLENGTH, 12, 0);
    SetWindowSubclass(g_hProgress, ProgressSubclassProc, 1, (DWORD_PTR)this);

    m_hTimeTip = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"STATIC", L"",
                                 WS_POPUP | SS_CENTER | SS_CENTERIMAGE, 0, 0, 80, 22, hwnd, nullptr, nullptr, nullptr);
    SetWindowLongPtr(m_hTimeTip, -8, (LONG_PTR)hwnd);
    SendMessage(m_hTimeTip, WM_SETFONT, (WPARAM)m_hTipFont, TRUE);

    g_hVolIcon = CreateWindowW(L"STATIC", L"", WS_CHILD | SS_CENTER | SS_CENTERIMAGE, 0, 0, 20, 24, hwnd,
                               (HMENU)IDC_VOL_ICON, nullptr, nullptr);
    if (m_hIconSpeaker)
        SendMessage(g_hVolIcon, STM_SETIMAGE, IMAGE_ICON, (LPARAM)m_hIconSpeaker);

    g_hVolume = CreateWindowExW(0, TRACKBAR_CLASS, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS, 0, 0, 100, 24,
                                hwnd, (HMENU)IDC_VOLUME, nullptr, nullptr);
    SetWindowTheme(g_hVolume, L" ", L" ");
    SendMessage(g_hVolume, TBM_SETRANGEMIN, TRUE, 0);
    SendMessage(g_hVolume, TBM_SETRANGEMAX, TRUE, VOL_MAX);
    SendMessage(g_hVolume, TBM_SETPOS, TRUE, 100);
    SetWindowSubclass(g_hVolume, VolumeSubclassProc, 3, (DWORD_PTR)this);

    g_hVolPercent = CreateWindowW(L"STATIC", L"100%", WS_CHILD | SS_LEFT | SS_CENTERIMAGE, 0, 0, 40, 24, hwnd,
                                  (HMENU)IDC_VOL_PERCENT, nullptr, nullptr);

    g_hTimeLabel = CreateWindowW(L"STATIC", L"--:-- / --:--", WS_CHILD | SS_LEFT | SS_CENTERIMAGE, 0, 0, 150, 30, hwnd,
                                 (HMENU)IDC_TIME_LABEL, nullptr, nullptr);
    SendMessage(g_hTimeLabel, WM_SETFONT, (WPARAM)m_hTimeFont, TRUE);

    HWND hCtrl = GetWindow(hwnd, GW_CHILD);
    while (hCtrl) {
        if (hCtrl == g_hTimeLabel)
            SendMessage(hCtrl, WM_SETFONT, (WPARAM)m_hTimeFont, TRUE);
        else
            SendMessage(hCtrl, WM_SETFONT, (WPARAM)m_hModernFont, TRUE);
        hCtrl = GetNextWindow(hCtrl, GW_HWNDNEXT);
    }

    m_player.Initialize(g_hVideoArea, hwnd);
    CreateSubtitleOverlay(hwnd);
    SetTimer(hwnd, ID_TIMER_UPDATE, TIMER_INTERVAL_MS, nullptr);
}

} // namespace guiVidi

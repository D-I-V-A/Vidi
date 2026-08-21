#ifndef IDS_HH
#define IDS_HH
#pragma once

#include <windows.h>

// ==========================================
// CONTROL IDs (UI Elements)
// ==========================================
#define IDC_PLAY            1001
#define IDC_PAUSE           1002
#define IDC_STOP            1003
#define IDC_SKIP_BACK       1004
#define IDC_SKIP_FORWARD    1005
#define IDC_PROGRESS        1006
#define IDC_VOLUME          1007
#define IDC_TIME_LABEL      1008
#define IDC_BACK            1009
#define IDC_FORWARD         1010
#define IDC_SETTINGS        1011
#define IDC_VIDEO_AREA      1012
#define IDC_TOOLBAR         5000

// Alias untuk kompatibilitas kode lama (opsional)
#define IDC_BTN_PLAY        IDC_PLAY
#define IDC_BTN_PAUSE       IDC_PAUSE
#define IDC_BTN_STOP        IDC_STOP
#define IDC_BTN_SKIPBACK    IDC_SKIP_BACK
#define IDC_BTN_SKIPFORWARD IDC_SKIP_FORWARD

// ==========================================
// MENU IDs
// ==========================================
// --- Media ---
#define IDM_OPEN_FILE       2001
#define IDM_OPEN_FOLDER     2002
#define IDM_OPEN_DISC       2003
#define IDM_OPEN_NETWORK    2004
#define IDM_OPEN_CAPTURE    2005
#define IDM_OPEN_RECENT     2006
#define IDM_QUIT            2007
#define IDM_SAVE_PLAYLIST   2008
#define IDM_CONVERT         2009
#define IDM_STREAM          2010

// Alias backward compat
#define IDM_FILE_OPEN       IDM_OPEN_FILE
#define IDM_FILE_EXIT       IDM_QUIT

// --- Playback ---
#define IDM_PLAY            2101
#define IDM_PAUSE           2102
#define IDM_STOP            2103
#define IDM_PREVIOUS        2104
#define IDM_NEXT            2105
#define IDM_RECORD          2106
#define IDM_SNAPSHOT        2107
#define IDM_SPEED_FASTER    2121
#define IDM_SPEED_FINE_FASTER 2122
#define IDM_SPEED_NORMAL    2123
#define IDM_SPEED_FINE_SLOWER 2124
#define IDM_SPEED_SLOWER    2125
#define IDM_JUMP_BACK_10    2131
#define IDM_JUMP_BACK_1MIN  2132
#define IDM_JUMP_FORWARD_10 2133
#define IDM_JUMP_FORWARD_1MIN 2134
#define IDM_FRAME_BY_FRAME  2141
#define IDM_LOOP            2142
#define IDM_A_TO_B          2143
#define IDM_PROGRAM         2144
#define IDM_TITLE           2145
#define IDM_CHAPTER         2146

#define IDM_PLAYBACK_PLAY       IDM_PLAY
#define IDM_PLAYBACK_PAUSE      IDM_PAUSE
#define IDM_PLAYBACK_STOP       IDM_STOP
#define IDM_PLAYBACK_SKIPBACK   IDM_PREVIOUS
#define IDM_PLAYBACK_SKIPFWD    IDM_NEXT

// --- Audio ---
#define IDM_INCREASE_VOL    2201
#define IDM_DECREASE_VOL    2202
#define IDM_MUTE            2203
#define IDM_AUDIO_TRACK     2204
#define IDM_AUDIO_DEVICE    2205
#define IDM_VIS_NONE        2211
#define IDM_VIS_SPECTRUM    2212
#define IDM_VIS_SCOPE       2213
#define IDM_VIS_VU_METER    2214
#define IDM_VIS_GOM         2215
#define IDM_AUDIO_STATS     2221
#define IDM_AUDIO_EFFECTS   2222
#define IDM_STEREO_MODE     2223
#define IDM_SPATIALIZER     2224

#define IDM_AUDIO_VOLUP     IDM_INCREASE_VOL
#define IDM_AUDIO_VOLDOWN   IDM_DECREASE_VOL
#define IDM_AUDIO_MUTE      IDM_MUTE

// --- Video ---
#define IDM_FULLSCREEN          2301
#define IDM_FULLSCREEN_INTERFACE 2302
#define IDM_ALWAYS_FIT_WINDOW   2303
#define IDM_SNAPSHOT_FOLDER     2304
#define IDM_DEINTERLACE         2305
#define IDM_DEINTERLACE_MODE    2306
#define IDM_CROP                2307
#define IDM_CROP_PADDING        2308
#define IDM_ASPECT_RATIO        2309
#define IDM_VIDEO_TRACK         2310
#define IDM_TAKE_SNAPSHOT       2311
#define IDM_VIDEO_WALLPAPER     2312
#define IDM_VIDEO_EFFECTS       2313
#define IDM_VIDEO_STATS         2314

#define IDM_VIEW_FULLSCREEN     IDM_FULLSCREEN

// --- Subtitle ---
#define IDM_SUB_ADD_FILE    2401
#define IDM_SUB_SAVE_FILE   2402
#define IDM_SUB_TRACK       2403
#define IDM_SUB_SETTINGS    2404
#define IDM_SUB_OSD         2405
#define IDM_SUB_SVCD        2406
#define IDM_SUB_EFFECTS     2407
#define IDM_SUB_SYNC        2408

// --- Tools ---
#define IDM_EFFECTS_FILTERS     2501
#define IDM_MEDIA_INFO          2502
#define IDM_CODEC_INFO          2503
#define IDM_VLMC_SHARE          2504
#define IDM_MESSAGES            2505
#define IDM_MODULE_MESSAGES     2506
#define IDM_PLUGINS             2507
#define IDM_CUSTOMIZE           2508
#define IDM_PREFERENCES         2509

// --- View ---
#define IDM_PLAYLIST            2601
#define IDM_DOCKED_PLAYLIST     2602
#define IDM_MINIMAL_INTERFACE   2603
#define IDM_ADVANCED_CONTROLS   2605
#define IDM_STATUS_BAR          2606
#define IDM_ALWAYS_ON_TOP       2607

// --- Help ---
#define IDM_HELP_WEBSITE        2701
#define IDM_HELP_DOCUMENTATION  2702
#define IDM_HELP_KB             2703
#define IDM_HELP_FAQ            2704
#define IDM_HELP_CHECK_UPDATE   2705
#define IDM_ABOUT               2706
#define IDM_APP_ESCAPE          2901
#define IDM_HELP_ABOUT          IDM_ABOUT

// ==========================================
// TIMER & MESSAGES
// ==========================================
#define ID_TIMER_UPDATE         3001
#define ID_TIMER_OSI_HIDE       3002
#define TIMER_INTERVAL_MS       33

#define WM_APP_MEDIA_READY      (WM_APP + 1)
#define WM_APP_PLAYBACK_ENDED   (WM_APP + 2)
#define WM_APP_MEDIA_ERROR      (WM_APP + 3)
#define WM_APP_AUDIO_MISSING    (WM_APP + 4)
#define WM_APP_GRAPH_EVENT      (WM_APP + 10)

// ==========================================
// ACCELERATOR TABLE
// ==========================================
inline HACCEL CreatePlayerAccelTable() {
    ACCEL accels[] = {
        { FVIRTKEY,                  VK_SPACE,  IDM_PLAY        },
        { FVIRTKEY,                  'S',       IDM_STOP        },
        { FVIRTKEY,                  VK_LEFT,   IDM_PREVIOUS    },
        { FVIRTKEY,                  VK_RIGHT,  IDM_NEXT        },
        { FVIRTKEY,                  VK_UP,     IDM_INCREASE_VOL},
        { FVIRTKEY,                  VK_DOWN,   IDM_DECREASE_VOL},
        { FVIRTKEY,                  'M',       IDM_MUTE        },
        { FVIRTKEY,                  'F',       IDM_FULLSCREEN  },
        { FVIRTKEY | FCONTROL,       'O',       IDM_OPEN_FILE   },
    };
    return CreateAcceleratorTable(accels, sizeof(accels) / sizeof(ACCEL));
}

#endif // IDS_HH
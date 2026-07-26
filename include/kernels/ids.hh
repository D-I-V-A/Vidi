#ifndef IDS_HH
#define IDS_HH

#include <windows.h>

// --- Control IDs ---
#define IDC_BTN_PLAY        1001
#define IDC_BTN_PAUSE       1002
#define IDC_BTN_STOP        1003
#define IDC_BTN_SKIPBACK    1004
#define IDC_BTN_SKIPFORWARD 1005
#define IDC_PROGRESS        1006
#define IDC_VOLUME          1007
#define IDC_TIME_LABEL      1008

// --- Menu: Media ---
#define IDM_FILE_OPEN       2001
#define IDM_FILE_EXIT       2002

// --- Menu: Playback ---
#define IDM_PLAYBACK_PLAY       2101
#define IDM_PLAYBACK_PAUSE      2102
#define IDM_PLAYBACK_STOP       2103
#define IDM_PLAYBACK_SKIPBACK   2104
#define IDM_PLAYBACK_SKIPFWD    2105

// --- Menu: Audio ---
#define IDM_AUDIO_VOLUP     2201
#define IDM_AUDIO_VOLDOWN   2202
#define IDM_AUDIO_MUTE      2203

// --- Menu: View ---
#define IDM_VIEW_FULLSCREEN 2301

// --- Menu: Help ---
#define IDM_HELP_ABOUT      2401

// --- Timer IDs ---
#define ID_TIMER_UPDATE     3001
#define TIMER_INTERVAL_MS   250

// --- Custom window messages ---
#define WM_APP_MEDIA_READY      (WM_APP + 1)
#define WM_APP_PLAYBACK_ENDED   (WM_APP + 2)
#define WM_APP_MEDIA_ERROR      (WM_APP + 3)



// create static
static HACCEL CreatePlayerAccelTable() {
    ACCEL accels[] = {
        { FVIRTKEY,                  VK_SPACE,  IDM_PLAYBACK_PLAY   }, // Space -> Play/Pause toggle (di-handle manual di handler)
        { FVIRTKEY,                  'S',       IDM_PLAYBACK_STOP   },
        { FVIRTKEY,                  VK_LEFT,   IDM_PLAYBACK_SKIPBACK },
        { FVIRTKEY,                  VK_RIGHT,  IDM_PLAYBACK_SKIPFWD },
        { FVIRTKEY,                  VK_UP,     IDM_AUDIO_VOLUP     },
        { FVIRTKEY,                  VK_DOWN,   IDM_AUDIO_VOLDOWN   },
        { FVIRTKEY,                  'M',       IDM_AUDIO_MUTE      },
        { FVIRTKEY,                  'F',       IDM_VIEW_FULLSCREEN },
        { FVIRTKEY | FCONTROL,       'O',       IDM_FILE_OPEN       },
    };
    return CreateAcceleratorTable(accels, sizeof(accels) / sizeof(ACCEL));
}

#endif // IDS_HH
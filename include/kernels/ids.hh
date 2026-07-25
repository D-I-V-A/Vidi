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

// --- Menu IDs ---
#define IDM_FILE_OPEN       2001

// --- Timer IDs ---
#define ID_TIMER_UPDATE     3001
#define TIMER_INTERVAL_MS   250   // update progress tiap 250ms

// --- Custom window messages (dikirim dari MF thread ke UI thread) ---
#define WM_APP_MEDIA_READY      (WM_APP + 1)  // media item siap, boleh mulai
#define WM_APP_PLAYBACK_ENDED   (WM_APP + 2)
#define WM_APP_MEDIA_ERROR      (WM_APP + 3)

#endif // IDS_HH
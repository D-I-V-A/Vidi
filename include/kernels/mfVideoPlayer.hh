#ifndef MF_VIDEO_PLAYER_HH
#define MF_VIDEO_PLAYER_HH

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfplay.h>
#include <mferror.h>


namespace kernelPlayerVidi{
    class MediaPlayerCallback;
    class MediaPlayer{
    private:
        IMFPMediaPlayer* m_pPlayer;
        MediaPlayerCallback* m_pCallback;
        
        HWND m_hVideoWnd;
        HWND m_hNotifyWnd;
        bool m_mfStarted;
    public:
        MediaPlayer();
        ~MediaPlayer();

        bool Initialize(HWND hVideoWnd, HWND hNotifyWnd);
        bool OpenFile(const wchar_t* path);
        void Play();
        void Pause();
        void Stop();
        void SetVolume(float vol); // range value between 0.0 to 1.0 
        void Seek(double seconds);
        double GetDuration();
        double GetPosition();
        void Shutdown();

        // dipanggil dari callback saat state berubah / video punya ukuran
        void UpdateVideoSize();
        HWND GetNotifyWnd() const {return m_hNotifyWnd;}
        void GetNativeVideoSize(int& width,int&height);
    };
}

#endif
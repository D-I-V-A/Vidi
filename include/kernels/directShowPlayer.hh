#ifndef DIRECTSHOW_PLAYER_HH
#define DIRECTSHOW_PLAYER_HH

#include <windows.h>
#include <dshow.h>
#include <control.h>

namespace kernelPlayerVidi{
    class DirectShowPlayer{
        private:
            HMODULE m_hLavSplitterDll;   // simpan handle biar bisa di-FreeLibrary saat shutdown
            HMODULE m_hLavVideoDll;
            HMODULE m_hLavAudioDll;
            HMODULE m_hVSFilterDll;
            IBaseFilter* LoadUnregisteredFilter(const wchar_t* dllPath, REFCLSID clsid, HMODULE* pOutModule);
            IGraphBuilder* m_pGraph;
            IMediaControl* m_pControl;
            IMediaEventEx* m_pEvent;
            IMediaSeeking*   m_pSeeking;
            IMediaSeeking*   m_pSourceSeeking; // fallback read-only durasi/posisi dari LAV Splitter
            IVideoWindow*    m_pVideoWindow;
            IBasicAudio*     m_pBasicAudio;
            IBasicVideo*     m_pBasicVideo;
            IBaseFilter*     m_pVSFilter;


            HWND m_hVideoWnd;
            HWND m_hNotifyWnd;

            bool m_comInitialized;
            bool m_graphBuilt;

            void DestroyGraph();
            bool CreateGraph();
            static long LinearToDShowVolume(float linearVol);
            IBaseFilter* FindFilterByName(const wchar_t* name);
            IPin* FindUnconnectedPin(IBaseFilter* pFilter, PIN_DIRECTION dir);
            GUID  GetPinMajorType(IPin* pPin);
            IPin* FindPinByMajorType(IBaseFilter* pFilter, PIN_DIRECTION dir, REFGUID majorType);
        public:
            DirectShowPlayer();
            ~DirectShowPlayer();

            bool Initialize(HWND hVideoWnd,HWND hNotifyWnd);
            bool OpenFile(const wchar_t* path);
            void Play();
            void Pause();
            void Stop();
            void SetVolume(float vol); // 0.0 - 1.0
            void Seek(double seconds);
            double GetDuration();
            double GetPosition();
            void Shutdown();

            void UpdateVideoSize();
            void GetNativeVideoSize(int& width,int& height);

            HWND GetNotifyWnd() const {return m_hNotifyWnd;}
            void HandleGraphEvent();
    };
}


#endif
#include "../../include/kernels/directShowPlayer.hh"
#include "../../include/kernels/ids.hh"
#include <cmath>
#include <string>
namespace kernelPlayerVidi{

    // MEDIATYPE_Subtitle tidak dideklarasikan di strmif.h SDK
    static const GUID GUID_MediaTypeSubtitle = {
        0x736c6774, 0x0000, 0x0010,
        { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }
    };
    static const CLSID CLSID_LAVSplitter =
    { 0x171252A0, 0x8820, 0x4AFE, { 0x9D, 0xF8, 0x5C, 0x92, 0xB2, 0xD6, 0x6B, 0x04 } };
    // "LAV Splitter Source" = file-source yang implement IFileSourceFilter
    static const CLSID CLSID_LAVSplitterSource =
        { 0xB98D13E7, 0x55DB, 0x4385, { 0xA3, 0x3D, 0x09, 0xFD, 0x1B, 0xA2, 0x63, 0x38 } };
    static const CLSID CLSID_LAVVideo =
        { 0xEE30215D, 0x164F, 0x4A92, { 0xA4, 0xEB, 0x9D, 0x4C, 0x13, 0x39, 0x0F, 0x9F } };
    static const CLSID CLSID_LAVAudio =
        { 0xE8E73B6B, 0x4CB3, 0x44A4, { 0xBE, 0x99, 0x4F, 0x7B, 0xCB, 0x96, 0xE4, 0x91 } };
    // VSFilter / DirectVobSub (xy-VSFilter juga memakai CLSID ini)
    static const CLSID CLSID_DirectVobSub =
        { 0x93a22e7a, 0x1291, 0x45c5, { 0xba, 0x6f, 0x6b, 0x54, 0x29, 0xeb, 0x7a, 0x53 } };
    static std::wstring GetExeDirW() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring full(path);
    size_t pos = full.find_last_of(L"\\/");
    return (pos != std::wstring::npos) ? full.substr(0, pos) : L".";
    }
    DirectShowPlayer::DirectShowPlayer()
    : m_pGraph(nullptr), m_pControl(nullptr), m_pEvent(nullptr),
      m_pSeeking(nullptr), m_pSourceSeeking(nullptr), m_pVideoWindow(nullptr), m_pBasicAudio(nullptr),
      m_pBasicVideo(nullptr), m_pVSFilter(nullptr),
      m_hVideoWnd(nullptr), m_hNotifyWnd(nullptr),
      m_comInitialized(false), m_graphBuilt(false),
      m_hLavSplitterDll(nullptr), m_hLavVideoDll(nullptr), m_hLavAudioDll(nullptr),
      m_hVSFilterDll(nullptr) {}

    DirectShowPlayer::~DirectShowPlayer() { Shutdown(); }
    static void FreeMediaTypeLocal(AM_MEDIA_TYPE& mt){
        if (mt.cbFormat != 0) {
        CoTaskMemFree((PVOID)mt.pbFormat);
        mt.cbFormat = 0;
        mt.pbFormat = nullptr;
        }
        if (mt.pUnk != nullptr) {
            mt.pUnk->Release();
            mt.pUnk = nullptr;
        }
    }
    
    static void DeleteMediaTypeLocal(AM_MEDIA_TYPE* pmt){
        if(pmt !=nullptr){
            FreeMediaTypeLocal(*pmt);
            CoTaskMemFree(pmt);
        }
    }

    bool DirectShowPlayer::Initialize(HWND hVideoWnd,HWND hNotifyWnd){
        m_hVideoWnd = hVideoWnd;
        m_hNotifyWnd = hNotifyWnd;

        HRESULT hr = CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
        if(FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;
        m_comInitialized = SUCCEEDED(hr);
        return true;
    }

    bool DirectShowPlayer::CreateGraph(){
        DestroyGraph();

        HRESULT hr = CoCreateInstance(CLSID_FilterGraph, nullptr,CLSCTX_INPROC_SERVER,
                                    IID_IGraphBuilder,(void**)&m_pGraph);
        if(FAILED(hr)) return false;
        m_pGraph->QueryInterface(IID_IMediaControl, (void**)&m_pControl);
        m_pGraph->QueryInterface(IID_IMediaEventEx, (void**)&m_pEvent);
        m_pGraph->QueryInterface(IID_IMediaSeeking, (void**)&m_pSeeking);
        m_pGraph->QueryInterface(IID_IVideoWindow, (void**)&m_pVideoWindow);
        m_pGraph->QueryInterface(IID_IBasicAudio, (void**)&m_pBasicAudio);
        m_pGraph->QueryInterface(IID_IBasicVideo, (void**)&m_pBasicVideo);

        if(m_pEvent)
            m_pEvent->SetNotifyWindow((OAHWND)m_hNotifyWnd, WM_APP_GRAPH_EVENT, 0);
        m_graphBuilt = true;
        return true;
    }


    void DirectShowPlayer::DestroyGraph() {
        if (m_pControl) m_pControl->Stop();
        if (m_pEvent)   m_pEvent->SetNotifyWindow((OAHWND)NULL, 0, 0);
        if (m_pVideoWindow) {
            m_pVideoWindow->put_Visible(OAFALSE);
            m_pVideoWindow->put_Owner((OAHWND)NULL);
        }

        if (m_pBasicVideo)  { m_pBasicVideo->Release();  m_pBasicVideo = nullptr; }
        if (m_pBasicAudio)  { m_pBasicAudio->Release();  m_pBasicAudio = nullptr; }
        if (m_pVideoWindow) { m_pVideoWindow->Release(); m_pVideoWindow = nullptr; }
        if (m_pSeeking)     { m_pSeeking->Release();     m_pSeeking = nullptr; }
        if (m_pSourceSeeking) { m_pSourceSeeking->Release(); m_pSourceSeeking = nullptr; }
        if (m_pEvent)       { m_pEvent->Release();       m_pEvent = nullptr; }
        if (m_pControl)     { m_pControl->Release();     m_pControl = nullptr; }
        if (m_pVSFilter)    { m_pVSFilter->Release();    m_pVSFilter = nullptr; }
        if (m_pGraph)       { m_pGraph->Release();        m_pGraph = nullptr; }

        if (m_hLavSplitterDll) { FreeLibrary(m_hLavSplitterDll); m_hLavSplitterDll = nullptr; }
        if (m_hLavVideoDll)    { FreeLibrary(m_hLavVideoDll);    m_hLavVideoDll = nullptr; }
        if (m_hLavAudioDll)    { FreeLibrary(m_hLavAudioDll);    m_hLavAudioDll = nullptr; }
        if (m_hVSFilterDll)    { FreeLibrary(m_hVSFilterDll);    m_hVSFilterDll = nullptr; }

        m_graphBuilt = false;
    }

    IBaseFilter* DirectShowPlayer::FindFilterByName(const wchar_t* name) {
    ICreateDevEnum* pDevEnum = nullptr;
    IEnumMoniker* pEnum = nullptr;
    IBaseFilter* pFilter = nullptr;

    if (FAILED(CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER,
                                 IID_ICreateDevEnum, (void**)&pDevEnum)))
        return nullptr;

    if (SUCCEEDED(pDevEnum->CreateClassEnumerator(CLSID_LegacyAmFilterCategory, &pEnum, 0)) && pEnum) {
        IMoniker* pMoniker = nullptr;
        while (pEnum->Next(1, &pMoniker, nullptr) == S_OK) {
            IPropertyBag* pBag = nullptr;
            if (SUCCEEDED(pMoniker->BindToStorage(nullptr, nullptr, IID_IPropertyBag, (void**)&pBag))) {
                VARIANT var;
                VariantInit(&var);
                if (SUCCEEDED(pBag->Read(L"FriendlyName", &var, nullptr))) {
                    if (var.vt == VT_BSTR && wcsstr(var.bstrVal, name) != nullptr) {
                        pMoniker->BindToObject(nullptr, nullptr, IID_IBaseFilter, (void**)&pFilter);
                        VariantClear(&var);
                        pBag->Release();
                        pMoniker->Release();
                        break;
                    }
                    VariantClear(&var);
                }
                pBag->Release();
            }
            pMoniker->Release();
        }
        pEnum->Release();
    }
    pDevEnum->Release();
    return pFilter;
}
    IBaseFilter* DirectShowPlayer::LoadUnregisteredFilter(const wchar_t* dllPath, REFCLSID clsid, HMODULE* pOutModule) {
    HMODULE hDll = LoadLibraryW(dllPath);
    if (!hDll) {
        wchar_t dbg[512];
        swprintf_s(dbg, L"[VIDI] LoadLibrary gagal: %s (err=%lu)\n", dllPath, GetLastError());
        OutputDebugString(dbg);
        return nullptr;
    }

    typedef HRESULT (STDAPICALLTYPE *DllGetClassObjectFunc)(REFCLSID, REFIID, LPVOID*);
    DllGetClassObjectFunc pDllGetClassObject =
        (DllGetClassObjectFunc)GetProcAddress(hDll, "DllGetClassObject");

    if (!pDllGetClassObject) {
        OutputDebugString(L"[VIDI] DllGetClassObject tidak ditemukan di DLL\n");
        FreeLibrary(hDll);
        return nullptr;
    }

    IClassFactory* pClassFactory = nullptr;
    HRESULT hr = pDllGetClassObject(clsid, IID_IClassFactory, (void**)&pClassFactory);
    if (FAILED(hr) || !pClassFactory) {
        OutputDebugString(L"[VIDI] DllGetClassObject gagal cari IClassFactory\n");
        FreeLibrary(hDll);
        return nullptr;
    }

    IBaseFilter* pFilter = nullptr;
    hr = pClassFactory->CreateInstance(nullptr, IID_IBaseFilter, (void**)&pFilter);
    pClassFactory->Release();

    if (FAILED(hr) || !pFilter) {
        OutputDebugString(L"[VIDI] CreateInstance gagal buat IBaseFilter\n");
        FreeLibrary(hDll);
        return nullptr;
    }

    *pOutModule = hDll;
    return pFilter;
}
    GUID DirectShowPlayer::GetPinMajorType(IPin* pPin) {
        GUID result = GUID_NULL;
        IEnumMediaTypes* pEnumMT = nullptr;
        if (SUCCEEDED(pPin->EnumMediaTypes(&pEnumMT)) && pEnumMT) {
            AM_MEDIA_TYPE* pmt = nullptr;
            if (pEnumMT->Next(1, &pmt, nullptr) == S_OK) {
                result = pmt->majortype;
                DeleteMediaTypeLocal(pmt);
            }
            pEnumMT->Release();
        }
        return result;
    }

    IPin* DirectShowPlayer::FindPinByMajorType(IBaseFilter* pFilter, PIN_DIRECTION dir, REFGUID majorType) {
        if (!pFilter) return nullptr;
        IEnumPins* pEnumPins = nullptr;
        if (FAILED(pFilter->EnumPins(&pEnumPins))) return nullptr;

        IPin* pFound = nullptr;
        IPin* pPin = nullptr;
        while (pEnumPins->Next(1, &pPin, nullptr) == S_OK) {
            PIN_DIRECTION pd;
            pPin->QueryDirection(&pd);
            GUID mt = GetPinMajorType(pPin);
            if (pd == dir && mt == majorType) {
                pFound = pPin;
                break;
            }
            pPin->Release();
        }
        pEnumPins->Release();
        return pFound;
    }

    IPin* DirectShowPlayer::FindUnconnectedPin(IBaseFilter* pFilter, PIN_DIRECTION dir) {
        if (!pFilter) return nullptr;
        IEnumPins* pEnumPins = nullptr;
        if (FAILED(pFilter->EnumPins(&pEnumPins))) return nullptr;

        IPin* pFound = nullptr;
        IPin* pPin = nullptr;
        while (pEnumPins->Next(1, &pPin, nullptr) == S_OK) {
            PIN_DIRECTION pd;
            pPin->QueryDirection(&pd);
            IPin* pConnectedTo = nullptr;
            pPin->ConnectedTo(&pConnectedTo);
            bool isConnected = (pConnectedTo != nullptr);
            if (pConnectedTo) pConnectedTo->Release();

            if (pd == dir && !isConnected) {
                pFound = pPin;
                break;
            }
            pPin->Release();
        }
        pEnumPins->Release();
        return pFound;
    }

    bool DirectShowPlayer::OpenFile(const wchar_t* path) {
    if (!CreateGraph()) return false;

    std::wstring exeDir = GetExeDirW();
    wchar_t dbg[512];

    // ===== Path A: LAV portable (bundled filters di folder filters\x64) =====
    bool portableOk = false;

    IBaseFilter* pSplitter = LoadUnregisteredFilter(
        (exeDir + L"\\filters\\x64\\LAVSplitter.ax").c_str(),
        CLSID_LAVSplitterSource, &m_hLavSplitterDll);

    if (pSplitter) {
        HRESULT hr = m_pGraph->AddFilter(pSplitter, L"LAV Splitter Source");
        swprintf_s(dbg, L"[VIDI] AddFilter splitter hr=0x%08X\n", (unsigned int)hr);
        OutputDebugString(dbg);
        if (SUCCEEDED(hr)) {
            IFileSourceFilter* pFileSource = nullptr;
            hr = pSplitter->QueryInterface(IID_IFileSourceFilter, (void**)&pFileSource);
            swprintf_s(dbg, L"[VIDI] QI IFileSourceFilter hr=0x%08X\n", (unsigned int)hr);
            OutputDebugString(dbg);
            if (SUCCEEDED(hr) && pFileSource) {
                hr = pFileSource->Load(path, nullptr);
                pFileSource->Release();
                swprintf_s(dbg, L"[VIDI] Load file hr=0x%08X\n", (unsigned int)hr);
                OutputDebugString(dbg);
                if (SUCCEEDED(hr)) {
                    IBaseFilter* pLavVideo = LoadUnregisteredFilter(
                        (exeDir + L"\\filters\\x64\\LAVVideo.ax").c_str(),
                        CLSID_LAVVideo, &m_hLavVideoDll);
                    IBaseFilter* pLavAudio = LoadUnregisteredFilter(
                        (exeDir + L"\\filters\\x64\\LAVAudio.ax").c_str(),
                        CLSID_LAVAudio, &m_hLavAudioDll);
                    if (pLavVideo) m_pGraph->AddFilter(pLavVideo, L"LAV Video Decoder");
                    if (pLavAudio) m_pGraph->AddFilter(pLavAudio, L"LAV Audio Decoder");

                    IEnumPins* pEnumPins = nullptr;
                    pSplitter->EnumPins(&pEnumPins);
                    IPin* pPin = nullptr;
                    IPin* pVideoPin = nullptr;
                    IPin* pSubPin = nullptr;
                    int pinCount = 0, renderOkCount = 0;
                    while (pEnumPins && pEnumPins->Next(1, &pPin, nullptr) == S_OK) {
                        PIN_DIRECTION pd;
                        pPin->QueryDirection(&pd);
                        if (pd == PINDIR_OUTPUT) {
                            pinCount++;
                            GUID majorType = GetPinMajorType(pPin);
                            if (majorType == GUID_MediaTypeSubtitle) {
                                pSubPin = pPin;
                                pSubPin->AddRef();
                            } else if (majorType == MEDIATYPE_Video) {
                                pVideoPin = pPin;
                                pVideoPin->AddRef();
                            } else {
                                hr = m_pGraph->Render(pPin);
                                swprintf_s(dbg, L"[VIDI] Render pin #%d hr=0x%08X\n", pinCount, (unsigned int)hr);
                                OutputDebugString(dbg);
                                if (SUCCEEDED(hr)) renderOkCount++;
                            }
                        }
                        pPin->Release();
                    }
                    if (pEnumPins) pEnumPins->Release();
                    swprintf_s(dbg, L"[VIDI] Pin output=%d, ter-render=%d, ada subtitle=%d\n", pinCount, renderOkCount, pSubPin ? 1 : 0);
                    OutputDebugString(dbg);

                    // ===== Video + subtitle lewat VSFilter (portable) =====
                    bool vsPathOk = false;
                    IBaseFilter* pVSFilter = nullptr;
                    if (pSubPin && pVideoPin && pLavVideo) {
                        pVSFilter = LoadUnregisteredFilter(
                            (exeDir + L"\\filters\\x64\\VSFilter.dll").c_str(),
                            CLSID_DirectVobSub, &m_hVSFilterDll);
                        if (!pVSFilter)
                            pVSFilter = LoadUnregisteredFilter(
                                (exeDir + L"\\filters\\x64\\xy-VSFilter.dll").c_str(),
                                CLSID_DirectVobSub, &m_hVSFilterDll);
                        if (pVSFilter) {
                            OutputDebugString(L"[VIDI] VSFilter berhasil di-load\n");
                            m_pGraph->AddFilter(pVSFilter, L"VSFilter");

                            IPin* lavVideoIn = FindUnconnectedPin(pLavVideo, PINDIR_INPUT);
                            IPin* lavVideoOut = lavVideoIn ? FindUnconnectedPin(pLavVideo, PINDIR_OUTPUT) : nullptr;
                            IPin* vsVideoIn = FindPinByMajorType(pVSFilter, PINDIR_INPUT, MEDIATYPE_Video);
                            IPin* vsSubIn  = FindPinByMajorType(pVSFilter, PINDIR_INPUT, GUID_MediaTypeSubtitle);
                            IPin* vsOut    = FindUnconnectedPin(pVSFilter, PINDIR_OUTPUT);

                            bool connectedLav = lavVideoIn &&
                                SUCCEEDED(m_pGraph->ConnectDirect(pVideoPin, lavVideoIn, nullptr));
                            bool connectedVs = false;
                            if (connectedLav && lavVideoOut && vsVideoIn)
                                connectedVs = SUCCEEDED(m_pGraph->ConnectDirect(lavVideoOut, vsVideoIn, nullptr));

                            bool rendered = false;
                            if (connectedVs && vsOut) {
                                hr = m_pGraph->Render(vsOut);
                                swprintf_s(dbg, L"[VIDI] Render output VSFilter hr=0x%08X\n", (unsigned int)hr);
                                OutputDebugString(dbg);
                                rendered = SUCCEEDED(hr);
                            }

                            if (rendered) {
                                if (pSubPin && vsSubIn) {
                                    hr = m_pGraph->ConnectDirect(pSubPin, vsSubIn, nullptr);
                                    swprintf_s(dbg, L"[VIDI] Connect subtitle->VSFilter hr=0x%08X\n", (unsigned int)hr);
                                    OutputDebugString(dbg);
                                }
                                vsPathOk = true;
                                renderOkCount++;
                            } else {
                                if (connectedVs) {
                                    if (lavVideoOut) m_pGraph->Disconnect(lavVideoOut);
                                    if (vsVideoIn) m_pGraph->Disconnect(vsVideoIn);
                                }
                                if (connectedLav) {
                                    if (pVideoPin) m_pGraph->Disconnect(pVideoPin);
                                    if (lavVideoIn) m_pGraph->Disconnect(lavVideoIn);
                                }
                            }

                            if (lavVideoIn) lavVideoIn->Release();
                            if (lavVideoOut) lavVideoOut->Release();
                            if (vsVideoIn) vsVideoIn->Release();
                            if (vsSubIn) vsSubIn->Release();
                            if (vsOut) vsOut->Release();
                        }
                    }

                    if (!vsPathOk) {
                        if (pSubPin)
                            OutputDebugString(L"[VIDI] Tanpa VSFilter, lewati pin subtitle\n");
                        if (pVideoPin) {
                            hr = m_pGraph->Render(pVideoPin);
                            swprintf_s(dbg, L"[VIDI] Render video pin hr=0x%08X\n", (unsigned int)hr);
                            OutputDebugString(dbg);
                            if (SUCCEEDED(hr)) renderOkCount++;
                        }
                    }

                    if (pSubPin) pSubPin->Release();
                    if (pVideoPin) pVideoPin->Release();

                    if (renderOkCount > 0) portableOk = true;

                    // Fallback read-only: bila agregat graph tidak melaporkan durasi
                    // (sering terjadi untuk video berjam-jam), tanya langsung ke LAV Splitter.
                    // SetPositions/seek TETAP lewat agregat m_pSeeking.
                    if (portableOk) {
                        IMediaSeeking* pSourceSeeking = nullptr;
                        if (SUCCEEDED(pSplitter->QueryInterface(IID_IMediaSeeking, (void**)&pSourceSeeking))) {
                            m_pSourceSeeking = pSourceSeeking;
                            LONGLONG d1 = 0, d2 = 0;
                            if (m_pSeeking) m_pSeeking->GetDuration(&d1);
                            m_pSourceSeeking->GetDuration(&d2);
                            swprintf_s(dbg, L"[VIDI] Durasi agregat=%.1fs source=%.1fs\n",
                                       (double)d1 / 10000000.0, (double)d2 / 10000000.0);
                            OutputDebugString(dbg);
                        }
                    }
                    if (pVSFilter) pVSFilter->Release();
                    if (pLavVideo) pLavVideo->Release();
                    if (pLavAudio) pLavAudio->Release();
                }
            }
        }
        pSplitter->Release();
    }

    // ===== Path B: fallback ke filter terdaftar (LAV terinstal) =====
    if (!portableOk) {
        OutputDebugString(L"[VIDI] Jalur portable gagal, fallback ke RenderFile\n");
        DestroyGraph();
        if (!CreateGraph()) {
            if (m_hNotifyWnd) PostMessage(m_hNotifyWnd, WM_APP_MEDIA_ERROR, (WPARAM)E_FAIL, 0);
            return false;
        }
        HRESULT hr = m_pGraph->RenderFile(path, nullptr);
        swprintf_s(dbg, L"[VIDI] RenderFile hr=0x%08X\n", (unsigned int)hr);
        OutputDebugString(dbg);
        if (FAILED(hr)) {
            if (m_hNotifyWnd) PostMessage(m_hNotifyWnd, WM_APP_MEDIA_ERROR, (WPARAM)hr, 0);
            DestroyGraph();
            return false;
        }
    }

    // ===== Setup video window & mulai =====
    if (m_pVideoWindow) {
        m_pVideoWindow->put_Owner((OAHWND)m_hVideoWnd);
        m_pVideoWindow->put_WindowStyle(WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
        m_pVideoWindow->put_MessageDrain((OAHWND)m_hVideoWnd);
        m_pVideoWindow->put_Visible(OATRUE);
        UpdateVideoSize();
    }

    if (m_pSeeking) {
        GUID timeFormat = TIME_FORMAT_MEDIA_TIME;
        m_pSeeking->SetTimeFormat(&timeFormat);
    }

    Play();
    if (m_hNotifyWnd) PostMessage(m_hNotifyWnd, WM_APP_MEDIA_READY, 0, 0);
    return true;
}

    void DirectShowPlayer::Play()  { if (m_pControl) m_pControl->Run();   }
    void DirectShowPlayer::Pause() { if (m_pControl) m_pControl->Pause(); }
    void DirectShowPlayer::Stop()  { if (m_pControl) m_pControl->Stop();  }

    long DirectShowPlayer::LinearToDShowVolume(float linearVol){
        if(linearVol <= 0.0001f) return -10000;
        if(linearVol >= 1.0f) return 0;
        double centibel = 2000.0 * log10((double)linearVol);
        if (centibel < -10000.0) centibel = -10000.0;
        if (centibel > 0.0) centibel = 0.0;
        return (long)centibel;
    }

    void DirectShowPlayer::SetVolume(float vol){
        if(m_pBasicAudio) m_pBasicAudio->put_Volume(LinearToDShowVolume(vol));
    }

    double DirectShowPlayer::GetDuration(){
        LONGLONG dur = 0;
        if(m_pSeeking && SUCCEEDED(m_pSeeking->GetDuration(&dur)) && dur > 0)
            return (double)dur / 10000000.0;
        if(m_pSourceSeeking && SUCCEEDED(m_pSourceSeeking->GetDuration(&dur)) && dur > 0)
            return (double)dur / 10000000.0;
        return 0.0;
    }


    void DirectShowPlayer::GetNativeVideoSize(int& width,int& height){
        width = 0;
        height = 0;
        if(!m_pBasicVideo) return;
        long w=0,h=0;
        if(SUCCEEDED(m_pBasicVideo->GetVideoSize(&w,&h))){
            width = w;
            height = h;
        }
    }

    double DirectShowPlayer::GetPosition(){
        LONGLONG pos = 0;
        if(m_pSeeking && SUCCEEDED(m_pSeeking->GetCurrentPosition(&pos)))
            return (double)pos / 10000000.0;
        if(m_pSourceSeeking && SUCCEEDED(m_pSourceSeeking->GetCurrentPosition(&pos)))
            return (double)pos / 10000000.0;
        return 0.0;
    }

    void DirectShowPlayer::Seek(double seconds){
        if(!m_pSeeking) return;
        LONGLONG pos = (LONGLONG)(seconds * 10000000.0);
        if (pos < 0) pos = 0;
        bool wasRunning  = false;
        OAFilterState fs;
        if(m_pControl && SUCCEEDED(m_pControl->GetState(0,&fs))){wasRunning = (fs==State_Running);}
        if(m_pControl && wasRunning) m_pControl->Pause();

        HRESULT hr = m_pSeeking->SetPositions(&pos, AM_SEEKING_AbsolutePositioning,
                                 nullptr, AM_SEEKING_NoPositioning);
        if(FAILED(hr)){
            wchar_t dbg[512];
            swprintf_s(dbg, L"[VIDI] Seek absolute hr=0x%08X, retry keyframe\n", (unsigned int)hr);
            OutputDebugString(dbg);
            hr = m_pSeeking->SetPositions(&pos,
                AM_SEEKING_AbsolutePositioning | AM_SEEKING_SeekToKeyFrame,
                nullptr, AM_SEEKING_NoPositioning);
        }
        wchar_t dbg[512];
        swprintf_s(dbg, L"[VIDI] Seek(%.1fs) -> hr=0x%08X\n", seconds, (unsigned int)hr);
        if(m_pControl && wasRunning) m_pControl->Run();
    }

    void DirectShowPlayer::UpdateVideoSize(){
        if(!m_pVideoWindow || !m_hVideoWnd) return;
        RECT rc;
        if(!GetClientRect(m_hVideoWnd, &rc)) return;
        m_pVideoWindow->put_Left(0);
        m_pVideoWindow->put_Top(0);
        m_pVideoWindow->put_Width(rc.right);
        m_pVideoWindow->put_Height(rc.bottom);
    }

    void DirectShowPlayer::HandleGraphEvent(){
        if(!m_pEvent) return ;
        long evCode;
        LONG_PTR param1,param2;

        while(SUCCEEDED(m_pEvent->GetEvent(&evCode,&param1,&param2,0))){
            m_pEvent->FreeEventParams(evCode, param1, param2);

        switch (evCode) {
            case EC_COMPLETE:
                if (m_pControl) m_pControl->Stop();
                if (m_hNotifyWnd) PostMessage(m_hNotifyWnd, WM_APP_PLAYBACK_ENDED, 0, 0);
                break;

            case EC_ERRORABORT:
            case EC_USERABORT:
                if (m_hNotifyWnd) PostMessage(m_hNotifyWnd, WM_APP_MEDIA_ERROR, (WPARAM)param1, 0);
                break;

            default:
                break;

        }
    }
    }

    void DirectShowPlayer::Shutdown() {
    DestroyGraph();
    if (m_comInitialized) {
        CoUninitialize();
        m_comInitialized = false;
        }
    }
}

#include "../../include/kernels/directShowPlayer.hh"
#include "../../include/kernels/ids.hh"
#include <mmreg.h>   // WAVEFORMATEXTENSIBLE, WAVE_FORMAT_IEEE_FLOAT
#include <cmath>
#include <string>
#include <cstdio>
#include <cstdarg>
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
    // [FIX ANTI-HIJAU] VMR-7 (Video Mixing Renderer). Tanpa ini Intelligent Connect
    // bisa jatuh ke legacy Video Renderer yang frame idle-nya berupa GRADIENT HIJAU +
    // logo blur khas quartz.dll -- muncul saat maximize/repaint tanpa frame baru.
    static const CLSID CLSID_VMR7 =
        { 0x87A59784, 0x25CF, 0x4A13, { 0x9B, 0xBE, 0x0D, 0xE8, 0x85, 0x58, 0xFF, 0xC5 } };
    
    enum { SG_FMT_PASSTHROUGH = 0, SG_FMT_INT16 = 1, SG_FMT_FLOAT32 = 2 };
    static std::wstring GetExeDirW() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring full(path);
    size_t pos = full.find_last_of(L"\\/");
    return (pos != std::wstring::npos) ? full.substr(0, pos) : L".";
    }

    // [DIAG] Log serentak ke OutputDebugString + %TEMP%\vidi_debug.log
    static void VLog(const wchar_t* fmt, ...) {
        wchar_t buf[512];
        va_list ap;
        va_start(ap, fmt);
        _vsnwprintf_s(buf, _TRUNCATE, fmt, ap);
        va_end(ap);
        wcscat_s(buf, L"\n");
        OutputDebugStringW(buf);

        wchar_t lp[MAX_PATH];
        if (GetTempPathW(MAX_PATH, lp)) {
            wcscat_s(lp, L"vidi_debug.log");
            FILE* f = nullptr;
            if (_wfopen_s(&f, lp, L"a") == 0 && f) {
                SYSTEMTIME st;
                GetLocalTime(&st);
                fwprintf(f, L"[%02u:%02u:%02u.%03u] %s",
                         st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, buf);
                fclose(f);
            }
        }
    }
    class GainCallback : public ISampleGrabberCB {
    public:
        GainCallback(std::atomic<float>* gain, std::atomic<int>* fmt)
            : m_gain(gain), m_fmt(fmt), m_ref(1) {}

        STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
            if (!ppv) return E_POINTER;
            if (riid == IID_IUnknown || riid == IID_ISampleGrabberCB) {
                *ppv = static_cast<ISampleGrabberCB*>(this);
                AddRef();
                return S_OK;
            }
            *ppv = nullptr;
            return E_NOINTERFACE;
        }
        STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_ref); }
        STDMETHODIMP_(ULONG) Release() override {
            ULONG r = InterlockedDecrement(&m_ref);
            if (!r) delete this;
            return r;
        }
        STDMETHODIMP SampleCB(double, IMediaSample*) override { return S_OK; }

        // Dipanggil thread streaming utk SETIAP buffer audio
        STDMETHODIMP BufferCB(double, BYTE* buf, long len) override {
            if (!buf || len <= 0 || !m_gain || !m_fmt) return S_OK;
            float g = m_gain->load(std::memory_order_relaxed);
            if (g <= 1.0001f) return S_OK;   // unity -> pass cepat
            switch (m_fmt->load(std::memory_order_relaxed)) {
            case SG_FMT_FLOAT32: {
                float* s = reinterpret_cast<float*>(buf);
                const long n = len / 4;
                for (long i = 0; i < n; ++i) {
                    float v = s[i] * g;
                    if (v >  1.0f) v =  1.0f;   // hard clamp anti-pecah
                    if (v < -1.0f) v = -1.0f;
                    s[i] = v;
                }
                break;
            }
            case SG_FMT_INT16: {
                short* s = reinterpret_cast<short*>(buf);
                const long n = len / 2;
                for (long i = 0; i < n; ++i) {
                    int v = (int)(s[i] * g);
                    if (v >  32767) v =  32767;
                    if (v < -32768) v = -32768;
                    s[i] = (short)v;
                }
                break;
            }
            default: break;   // format tak dikenal -> pass-through
            }
            return S_OK;
        }
    private:
        std::atomic<float>* m_gain;
        std::atomic<int>*   m_fmt;
        ULONG m_ref;
    };

    static int DetectPcmFormat(const AM_MEDIA_TYPE& mt) {
        const WAVEFORMATEX* wfx = nullptr;
        if (mt.formattype == FORMAT_WaveFormatEx && mt.pbFormat &&
            mt.cbFormat >= sizeof(WAVEFORMATEX))
            wfx = reinterpret_cast<const WAVEFORMATEX*>(mt.pbFormat);
        if (!wfx) {
            if (mt.subtype == SG_SUBTYPE_IEEE_FLOAT) return SG_FMT_FLOAT32;
            return SG_FMT_PASSTHROUGH;
        }
        if (wfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) return SG_FMT_FLOAT32;
        if (wfx->wFormatTag == WAVE_FORMAT_PCM && wfx->wBitsPerSample == 16)
            return SG_FMT_INT16;
        if (wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
            mt.cbFormat >= sizeof(WAVEFORMATEXTENSIBLE)) {
            auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(wfx);
            // LAV umumnya menawarkan float32 lewat tag EXTENSIBLE
            if (ext->SubFormat == SG_SUBTYPE_IEEE_FLOAT &&
                ext->Format.wBitsPerSample == 32) return SG_FMT_FLOAT32;
            if (ext->SubFormat == SG_SUBTYPE_PCM &&
                ext->Format.wBitsPerSample == 16) return SG_FMT_INT16;
        }
        return SG_FMT_PASSTHROUGH;
    }
    DirectShowPlayer::DirectShowPlayer()
    : m_pGraph(nullptr), m_pControl(nullptr), m_pEvent(nullptr),
      m_pSeeking(nullptr), m_pSourceSeeking(nullptr), m_pVideoWindow(nullptr), m_pBasicAudio(nullptr),
      m_pBasicVideo(nullptr), m_pVSFilter(nullptr),
      m_hVideoWnd(nullptr), m_hNotifyWnd(nullptr),
      m_pGrabberBF(nullptr), m_pGrabber(nullptr), m_pGainCb(nullptr),
      m_dspGain(1.0f), m_dspFmt(SG_FMT_PASSTHROUGH),
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

        // [DIAG] mulai log segar tiap sesi
        {
            wchar_t lp[MAX_PATH];
            if (GetTempPathW(MAX_PATH, lp)) {
                wcscat_s(lp, L"vidi_debug.log");
                FILE* f = nullptr;
                if (_wfopen_s(&f, lp, L"w") == 0 && f) {
                    SYSTEMTIME st;
                    GetLocalTime(&st);
                    fwprintf(f, L"=== Vidi session %02u-%02u-%04u %02u:%02u:%02u ===\n",
                             st.wDay, st.wMonth, st.wYear,
                             st.wHour, st.wMinute, st.wSecond);
                    fclose(f);
                }
            }
        }

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

    // [FIX ANTI-HIJAU] Tambahkan VMR-7 SEBELUM pin video di-render, supaya
    // Intelligent Connect memakai VMR-7 (bg hitam, frame terakhir dipertahankan
    // saat repaint) alih-alih legacy Video Renderer (gradient hijau + logo).
    bool DirectShowPlayer::AddVideoRenderer() {
        if (!m_pGraph) return false;
        IBaseFilter* pVMR = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_VMR7, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_IBaseFilter, (void**)&pVMR);
        if (FAILED(hr)) {
            OutputDebugString(L"[VIDI] CoCreateInstance VMR-7 gagal\n");
            return false;
        }
        hr = m_pGraph->AddFilter(pVMR, L"Vidi Video Renderer");
        pVMR->Release();
        if (FAILED(hr)) {
            OutputDebugString(L"[VIDI] AddFilter VMR-7 gagal\n");
            return false;
        }
        OutputDebugString(L"[VIDI] VMR-7 ditambahkan ke graph\n");
        return true;
    }

    // [FIX CRASH-24H2] qedit.dll pada Windows 11 24H2 (build 26100) rusak:
    // CoCreateInstance dan QI berhasil, tetapi hampir semua metode
    // ISampleGrabber mengakses memori tak valid (access violation).
    // Setiap panggilan dibungkus SEH -- sekali crash, fitur boost dimatikan
    // permanen untuk sisa proses dan audio jatuh ke fallback bersih tanpa SG.
    static bool g_sgBroken = false;

    // CATATAN: fungsi ber-SEH tidak boleh memiliki objek C++ berdestruktor.
    static bool SGTry_SetConfig(ISampleGrabber* sg, ISampleGrabberCB* cb) {
        __try {
            sg->SetCallback(cb, 1);   // 1 = BufferCB
            sg->SetBufferSamples(0);
            sg->SetOneShot(0);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            g_sgBroken = true;
            return false;
        }
    }

    static HRESULT SGTry_GetConnectedType(ISampleGrabber* sg, AM_MEDIA_TYPE* mt) {
        __try {
            return sg->GetConnectedMediaType(mt);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            g_sgBroken = true;
            return E_UNEXPECTED;
        }
    }

    // [DSP GAIN] Buat + pasang SampleGrabber ke graph (sekali saja, lazy)
    bool DirectShowPlayer::EnsureGainFilter() {
        if (g_sgBroken) return false;
        if (m_pGrabberBF && m_pGrabber) return true;

        VLog(L"[VIDI] SG: CoCreateInstance...");
        HRESULT hr = CoCreateInstance(CLSID_SampleGrabber, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_IBaseFilter, (void**)&m_pGrabberBF);
        if (FAILED(hr)) {
            VLog(L"[VIDI] SampleGrabber CoCreateInstance gagal hr=0x%08X (qedit tidak terdaftar, boost off)",
                 (unsigned int)hr);
            return false;
        }
        VLog(L"[VIDI] SG: instans OK, AddFilter...");
        hr = m_pGraph->AddFilter(m_pGrabberBF, L"Vidi Audio Gain");
        if (FAILED(hr)) {
            VLog(L"[VIDI] AddFilter grabber gagal hr=0x%08X", (unsigned int)hr);
            m_pGrabberBF->Release(); m_pGrabberBF = nullptr;
            return false;
        }
        VLog(L"[VIDI] SG: QI ISampleGrabber...");
        hr = m_pGrabberBF->QueryInterface(IID_ISampleGrabber, (void**)&m_pGrabber);
        if (FAILED(hr)) {
            VLog(L"[VIDI] QI ISampleGrabber gagal hr=0x%08X", (unsigned int)hr);
            // [FIX BISU] WAJIB dicabut dari graph -- kalau hanya di-Release,
            // filter sisa tetap hidup dan menelan pin audio saat fallback Render().
            m_pGraph->RemoveFilter(m_pGrabberBF);
            m_pGrabberBF->Release(); m_pGrabberBF = nullptr;
            return false;
        }

        // [FIX CRASH] JANGAN panggil SetMediaType sama sekali -- terbukti
        // mem-crash qedit asli di sistem ini. Tanpa type, SG menerima apa pun;
        // data terkompresi disaring SETELAH koneksi lewat verifikasi format
        // di RouteAudioThroughGain (bongkar + fallback kalau bukan PCM).
        VLog(L"[VIDI] SG: konfigurasi callback...");
        if (!m_pGainCb)
            m_pGainCb = new GainCallback(&m_dspGain, &m_dspFmt);

        if (!SGTry_SetConfig(m_pGrabber, m_pGainCb)) {
            VLog(L"[VIDI] SG: qedit crash saat konfigurasi (Win11 24H2) -> boost off permanen");
            TeardownGainFilter();
            return false;
        }
        VLog(L"[VIDI] SG: grabber siap");
        return true;
    }

    static void DisconnectPinPair(IGraphBuilder* g,IPin* p){
        if (!g || !p) return;
        IPin* peer = nullptr;
        if (SUCCEEDED(p->ConnectedTo(&peer)) && peer) {
            g->Disconnect(peer);
            g->Disconnect(p);
            peer->Release();
        }
    }

    // [FIX] lepas semua koneksi pin sebuah filter (utk membongkar rantai parsial)
    static void DisconnectAllPins(IGraphBuilder* g, IBaseFilter* f) {
        if (!g || !f) return;
        IEnumPins* en = nullptr;
        if (SUCCEEDED(f->EnumPins(&en)) && en) {
            IPin* p = nullptr;
            while (en->Next(1, &p, nullptr) == S_OK) {
                DisconnectPinPair(g, p);
                p->Release();
            }
            en->Release();
        }
    }
    void DirectShowPlayer::TeardownGainFilter() {
        if (m_pGrabberBF && m_pGraph)
            m_pGraph->RemoveFilter(m_pGrabberBF);
        // [FIX CRASH] urutan anti use-after-free:
        // callback dilepas DULU selagi grabber masih memegang referensinya,
        // baru interface/filter dirilis (dtor filter melepas sisa ref
        // callback sehingga terhapus tepat satu kali).
        if (m_pGainCb)    { m_pGainCb->Release();    m_pGainCb    = nullptr; }
        if (m_pGrabber)   { m_pGrabber->Release();   m_pGrabber   = nullptr; }
        if (m_pGrabberBF) { m_pGrabberBF->Release(); m_pGrabberBF = nullptr; }
    }

    // [DSP GAIN] pin audio splitter -> [decoder via IC] -> grabber -> renderer
    bool DirectShowPlayer::RouteAudioThroughGain(IPin* pAudioOut) {
        if (!pAudioOut || !m_pGraph || !EnsureGainFilter()) return false;

        VLog(L"[VIDI] Gain: mulai routing pin audio");

        // [FIX MULTI-AUDIO] input pin grabber sudah terpakai track lain?
        // JANGAN teardown -- chain track pertama masih hidup lewat grabber itu.
        IPin* freeIn = FindUnconnectedPin(m_pGrabberBF, PINDIR_INPUT);
        if (!freeIn) {
            VLog(L"[VIDI] Gain: grabber terpakai track lain -> skip (tanpa teardown)");
            return false;
        }
        freeIn->Release();

        // [FIX CRASH] tanpa SetMediaType -- SG menerima apa pun (default baku).
        // Kompresi disaring pasca-koneksi lewat verifikasi GetConnectedMediaType.
        IPin* grabIn = FindUnconnectedPin(m_pGrabberBF, PINDIR_INPUT);
        if (!grabIn) { TeardownGainFilter(); return false; }

        VLog(L"[VIDI] Gain: Connect(tanpa batasan type)...");
        HRESULT hrC = m_pGraph->Connect(pAudioOut, grabIn);
        grabIn->Release();
        if (FAILED(hrC)) {
            VLog(L"[VIDI] Gain: Connect hr=0x%08X -> bongkar + fallback", (unsigned int)hrC);
            TeardownGainFilter();   // graph bersih agar fallback Render() tak terganggu
            return false;
        }

        VLog(L"[VIDI] Gain: input tersambung, cari pin output");

        // Pin OUTPUT Sample Grabber baru ADA setelah input terkoneksi.
        IPin* grabOut = FindUnconnectedPin(m_pGrabberBF, PINDIR_OUTPUT);
        if (!grabOut) {
            VLog(L"[VIDI] Gain: pin output tidak ditemukan pasca-connect");
            DisconnectAllPins(m_pGraph, m_pGrabberBF);
            DisconnectPinPair(m_pGraph, pAudioOut);
            TeardownGainFilter();
            return false;
        }

        VLog(L"[VIDI] Gain: Render(grabOut)...");
        HRESULT hrR = m_pGraph->Render(grabOut);
        grabOut->Release();
        if (FAILED(hrR)) {
            VLog(L"[VIDI] Gain: Render(grabOut) hr=0x%08X -> bongkar rantai", (unsigned int)hrR);
            DisconnectAllPins(m_pGraph, m_pGrabberBF);
            DisconnectPinPair(m_pGraph, pAudioOut);
            TeardownGainFilter();
            return false;
        }

        // Verifikasi akhir: yang melewati grabber HARUS PCM/float.
        AM_MEDIA_TYPE got = {};
        int fmt = SG_FMT_PASSTHROUGH;
        HRESULT hrG = SGTry_GetConnectedType(m_pGrabber, &got);
        if (g_sgBroken) {
            VLog(L"[VIDI] SG: qedit crash saat GetConnectedMediaType -> bongkar");
            DisconnectAllPins(m_pGraph, m_pGrabberBF);
            DisconnectPinPair(m_pGraph, pAudioOut);
            TeardownGainFilter();
            return false;
        }
        if (SUCCEEDED(hrG)) {
            fmt = DetectPcmFormat(got);
            const WAVEFORMATEX* wfx =
                (got.formattype == FORMAT_WaveFormatEx && got.pbFormat &&
                 got.cbFormat >= sizeof(WAVEFORMATEX))
                    ? reinterpret_cast<const WAVEFORMATEX*>(got.pbFormat) : nullptr;
            if (wfx)
                VLog(L"[VIDI] Gain OK fmt=%d tag=0x%04X ch=%u %u Hz bits=%u",
                     fmt, wfx->wFormatTag, wfx->nChannels,
                     wfx->nSamplesPerSec, wfx->wBitsPerSample);
            else
                VLog(L"[VIDI] Gain OK fmt=%d (format bukan WaveFormatEx)", fmt);
            FreeMediaTypeLocal(got);
        } else {
            VLog(L"[VIDI] Gain OK fmt=%d (GetConnectedMediaType gagal)", fmt);
        }
        if (fmt == SG_FMT_PASSTHROUGH) {
            // Masih terkompresi -- DSP tak boleh menyentuh data ini.
            VLog(L"[VIDI] Gain: ternyata compressed -> bongkar + fallback");
            DisconnectAllPins(m_pGraph, m_pGrabberBF);
            DisconnectPinPair(m_pGraph, pAudioOut);
            TeardownGainFilter();
            return false;
        }

        m_dspFmt.store(fmt);
        return true;
    }

    void DirectShowPlayer::SetDspGain(float gain) {
        if (gain < 1.0f) gain = 1.0f;
        if (gain > 1.5f) gain = 1.5f;
        m_dspGain.store(gain);   // atomic: aman dibaca thread streaming kapan pun
    }


    void DirectShowPlayer::DestroyGraph() {
        if (m_pControl) m_pControl->Stop();
        if (m_pEvent)   m_pEvent->SetNotifyWindow((OAHWND)NULL, 0, 0);
        if (m_pVideoWindow) {
            m_pVideoWindow->put_Visible(OAFALSE);
            m_pVideoWindow->put_Owner((OAHWND)NULL);
        }

        // [DSP GAIN] lepas grabber + callback
        TeardownGainFilter();
        m_dspGain.store(1.0f);
        m_dspFmt.store(SG_FMT_PASSTHROUGH);

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
    VLog(L"[VIDI] ===== OpenFile: %s", path);

    std::wstring exeDir = GetExeDirW();
    wchar_t dbg[512];

    // ===== Path A: LAV portable (bundled filters di folder filters\x64) =====
    bool portableOk = false;

    IBaseFilter* pSplitter = LoadUnregisteredFilter(
    (exeDir + L"\\filters\\x64\\LAVSplitter.ax").c_str(),
    CLSID_LAVSplitterSource, &m_hLavSplitterDll);

if (pSplitter) {
    HRESULT hr = m_pGraph->AddFilter(pSplitter, L"LAV Splitter Source");
    VLog(L"[VIDI] AddFilter splitter hr=0x%08X", (unsigned int)hr);
    if (SUCCEEDED(hr)) {
        IFileSourceFilter* pFileSource = nullptr;
        hr = pSplitter->QueryInterface(IID_IFileSourceFilter, (void**)&pFileSource);
        VLog(L"[VIDI] QI IFileSourceFilter hr=0x%08X", (unsigned int)hr);
        if (SUCCEEDED(hr) && pFileSource) {
            hr = pFileSource->Load(path, nullptr);
            pFileSource->Release();
            VLog(L"[VIDI] Load file hr=0x%08X", (unsigned int)hr);
            if (SUCCEEDED(hr)) {
                IBaseFilter* pLavVideo = LoadUnregisteredFilter(
                    (exeDir + L"\\filters\\x64\\LAVVideo.ax").c_str(),
                    CLSID_LAVVideo, &m_hLavVideoDll);
                IBaseFilter* pLavAudio = LoadUnregisteredFilter(
                    (exeDir + L"\\filters\\x64\\LAVAudio.ax").c_str(),
                    CLSID_LAVAudio, &m_hLavAudioDll);

                // [FIX AUDIO] Fallback ke CLSID terdaftar (LAV Filters ter-install system-wide)
                if (!pLavAudio) {
                    OutputDebugString(L"[VIDI] LAVAudio.ax portable gagal, coba CLSID terdaftar\n");
                    HRESULT hrAud = CoCreateInstance(CLSID_LAVAudio, nullptr, CLSCTX_INPROC_SERVER,
                                                      IID_IBaseFilter, (void**)&pLavAudio);
                    if (FAILED(hrAud)) {
                        pLavAudio = nullptr;
                        OutputDebugString(L"[VIDI] LAV Audio Decoder TIDAK DITEMUKAN (portable & terdaftar gagal)\n");
                    } else {
                        OutputDebugString(L"[VIDI] LAV Audio Decoder terdaftar berhasil dipakai\n");
                    }
                }
                if (!pLavVideo) {
                    HRESULT hrVid = CoCreateInstance(CLSID_LAVVideo, nullptr, CLSCTX_INPROC_SERVER,
                                                      IID_IBaseFilter, (void**)&pLavVideo);
                    if (FAILED(hrVid)) pLavVideo = nullptr;
                }

                if (pLavVideo) m_pGraph->AddFilter(pLavVideo, L"LAV Video Decoder");
                if (pLavAudio) m_pGraph->AddFilter(pLavAudio, L"LAV Audio Decoder");

                // Renderer WAJIB didaftarkan sebelum pin video di-render
                AddVideoRenderer();
                VLog(L"[VIDI] Decoder+VMR terpasang, mulai enumerasi pin");

                IEnumPins* pEnumPins = nullptr;
                pSplitter->EnumPins(&pEnumPins);
                IPin* pPin = nullptr;
                IPin* pVideoPin = nullptr;
                IPin* pSubPin = nullptr;
                int pinCount = 0, renderOkCount = 0;
                bool audioPinFound = false, audioRendered = false;

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
                            // [FIX AUDIO] pin ini diasumsikan audio -- kasih log yang jelas
                            audioPinFound = true;
                            if (!pLavAudio)
                                VLog(L"[VIDI] WARNING: render pin audio TANPA decoder LAV di graph");

                            if (RouteAudioThroughGain(pPin)) {
                                hr = S_OK;
                            } else {
                                VLog(L"[VIDI] RouteAudioThroughGain gagal -> fallback Render() langsung");
                                hr = m_pGraph->Render(pPin);
                            }
                            VLog(L"[VIDI] Render pin #%d (AUDIO) hr=0x%08X", pinCount, (unsigned int)hr);

                            if (SUCCEEDED(hr)) {
                                renderOkCount++;
                                audioRendered = true;
                            } else {
                                VLog(L"[VIDI] AUDIO GAGAL DI-RENDER SAMA SEKALI -- inilah penyebab tidak ada suara");
                            }
                        }
                    }
                    pPin->Release();
                }
                if (pEnumPins) pEnumPins->Release();
                swprintf_s(dbg, L"[VIDI] Pin output=%d, ter-render=%d, ada subtitle=%d\n",
                           pinCount, renderOkCount, pSubPin ? 1 : 0);
                OutputDebugString(dbg);

                // [FIX AUDIO] Kasih tahu GUI kalau file punya track audio tapi gagal total dirender,
                // supaya user tahu ini masalah codec/graph, bukan bug volume/mute.
                if (audioPinFound && !audioRendered && m_hNotifyWnd) {
                    VLog(L"[VIDI] Post WM_APP_AUDIO_MISSING (pin ada, render nol)");
                    PostMessage(m_hNotifyWnd, WM_APP_AUDIO_MISSING, 0, 0);
                }

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
        VLog(L"[VIDI] Jalur portable gagal, fallback ke RenderFile");
        DestroyGraph();
        if (!CreateGraph()) {
            if (m_hNotifyWnd) PostMessage(m_hNotifyWnd, WM_APP_MEDIA_ERROR, (WPARAM)E_FAIL, 0);
            return false;
        }
        AddVideoRenderer();   // [FIX ANTI-HIJAU] juga di jalur RenderFile fallback
        EnsureGainFilter();   // [DSP GAIN] pasang sebelum RenderFile agar ikut terkoneksi otomatis
        HRESULT hr = m_pGraph->RenderFile(path, nullptr);
        VLog(L"[VIDI] RenderFile hr=0x%08X", (unsigned int)hr);
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
        m_pVideoWindow->put_BackgroundPalette(0);   // pakai palet sistem, hindari warna aneh
        m_pVideoWindow->put_BorderColor(RGB(0, 0, 0));
        m_pVideoWindow->put_Visible(OAFALSE);   // sembunyi sampai frame pertama siap (anti hijau/abu)
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
        long cb = LinearToDShowVolume(vol);
        if(m_pBasicAudio) m_pBasicAudio->put_Volume(cb);
        VLog(L"[VIDI] SetVolume %.2f -> %ld centibel (basicAudio=%s)",
             vol, cb, m_pBasicAudio ? L"ada" : L"NULL");
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

        HRESULT hr = m_pSeeking->SetPositions(&pos, AM_SEEKING_AbsolutePositioning,
                                 nullptr, AM_SEEKING_NoPositioning);
        if(FAILED(hr)){
            hr = m_pSeeking->SetPositions(&pos,
                AM_SEEKING_AbsolutePositioning | AM_SEEKING_SeekToKeyFrame,
                nullptr, AM_SEEKING_NoPositioning);
        }
        wchar_t dbg[512];
        swprintf_s(dbg, L"[VIDI] Seek(%.1fs) -> hr=0x%08X\n", seconds, (unsigned int)hr);
        OutputDebugString(dbg);
    }
    void DirectShowPlayer::ForceFrameRefresh() {
        if (!m_pSeeking || !m_graphBuilt) return;
        Seek(GetPosition());   // seek ke posisi yg sama = decoder dipaksa push frame
    }

    void DirectShowPlayer::ShowVideoWindow() {
        if (m_pVideoWindow) m_pVideoWindow->put_Visible(OATRUE);
    }
    void DirectShowPlayer::UpdateVideoSize(){
        if(!m_pVideoWindow || !m_hVideoWnd) return;
        RECT rc;
        if(!GetClientRect(m_hVideoWnd, &rc)) return;
        if(rc.right <=0||rc.bottom <= 0) return ;
        // dimensi asli video render
        long x = 0, y = 0, w = rc.right, h = rc.bottom;
        int vidW = 0,vidH = 0;
        GetNativeVideoSize(vidW,vidH);
        if (vidW > 0 && vidH > 0) {
            // Letterbox fit: skala menjaga rasio, center di area video
            double scaleX = (double)rc.right  / vidW;
            double scaleY = (double)rc.bottom / vidH;
            double scale  = (scaleX < scaleY) ? scaleX : scaleY;
            w = (long)(vidW * scale + 0.5);
            h = (long)(vidH * scale + 0.5);
            x = (rc.right - w) / 2;
            y = (rc.bottom - h) / 2;
        }
        // [FIX MAXIMIZE] SetWindowPosition = move+size ATOMIK dalam 1 panggilan.
        // put_Left/put_Top/put_Width/put_Height terpisah menyebabkan window renderer
        // di-resize 4x -> distorsi/gradient hijau sesaat saat maximize.
        m_pVideoWindow->SetWindowPosition(x, y, w, h);
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

            case EC_REPAINT:
                // VMR kehilangan surface (biasanya setelah maximize/restore):
                // sinkronkan posisi dulu, baru paksa decoder push 1 frame.
                UpdateVideoSize();
                ForceFrameRefresh();
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

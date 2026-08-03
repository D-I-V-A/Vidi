#include "../../include/kernels/mfVideoPlayer.hh"
#include "../../include/kernels/ids.hh"

#include <propvarutil.h>

namespace kernelPlayerVidi {

class MediaPlayerCallback : public IMFPMediaPlayerCallback {
    long m_refCount;
    MediaPlayer* m_owner;

public:
    MediaPlayerCallback(MediaPlayer* owner) : m_refCount(1), m_owner(owner) {}

    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_refCount); }
    STDMETHODIMP_(ULONG) Release() override {
        long count = InterlockedDecrement(&m_refCount);
        if (count == 0) delete this;
        return count;
    }
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IMFPMediaPlayerCallback || riid == IID_IUnknown) {
            *ppv = static_cast<IMFPMediaPlayerCallback*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    void STDMETHODCALLTYPE OnMediaPlayerEvent(MFP_EVENT_HEADER* pEventHeader) override {
        HWND hNotify = m_owner->GetNotifyWnd();

        if (FAILED(pEventHeader->hrEvent)) {
            if (hNotify) PostMessage(hNotify, WM_APP_MEDIA_ERROR, (WPARAM)pEventHeader->hrEvent, 0);
            return;
        }

        switch (pEventHeader->eEventType) {
            case MFP_EVENT_TYPE_MEDIAITEM_CREATED: {
                MFP_MEDIAITEM_CREATED_EVENT* pEvent =
                    MFP_GET_MEDIAITEM_CREATED_EVENT(pEventHeader);
                pEventHeader->pMediaPlayer->SetMediaItem(pEvent->pMediaItem);
                break;
            }
            case MFP_EVENT_TYPE_MEDIAITEM_SET:
                m_owner->UpdateVideoSize();
                m_owner->Play();
                if (hNotify) PostMessage(hNotify, WM_APP_MEDIA_READY, 0, 0);
                break;
            case MFP_EVENT_TYPE_PLAYBACK_ENDED:
                if (hNotify) PostMessage(hNotify, WM_APP_PLAYBACK_ENDED, 0, 0);
                break;
            default:
                break;
        }
    }
};

MediaPlayer::MediaPlayer()
    : m_pPlayer(nullptr), m_pCallback(nullptr), m_hVideoWnd(nullptr),
      m_hNotifyWnd(nullptr), m_mfStarted(false) {}

MediaPlayer::~MediaPlayer() { Shutdown(); }

bool MediaPlayer::Initialize(HWND hVideoWnd, HWND hNotifyWnd) {
    m_hVideoWnd = hVideoWnd;
    m_hNotifyWnd = hNotifyWnd;

    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) return false;
    m_mfStarted = true;

    m_pCallback = new MediaPlayerCallback(this);

    hr = MFPCreateMediaPlayer(nullptr, FALSE, 0, m_pCallback, m_hVideoWnd, &m_pPlayer);
    return SUCCEEDED(hr);
}

bool MediaPlayer::OpenFile(const wchar_t* path) {
    if (!m_pPlayer) return false;
    return SUCCEEDED(m_pPlayer->CreateMediaItemFromURL(path, FALSE, 0, nullptr));
}

void MediaPlayer::Play()  { if (m_pPlayer) m_pPlayer->Play();  }
void MediaPlayer::Pause() { if (m_pPlayer) m_pPlayer->Pause(); }
void MediaPlayer::Stop()  { if (m_pPlayer) m_pPlayer->Stop();  }
void MediaPlayer::SetVolume(float vol) { if (m_pPlayer) m_pPlayer->SetVolume(vol); }

void MediaPlayer::Seek(double seconds) {
    if (!m_pPlayer) return;
    PROPVARIANT var;
    PropVariantInit(&var);
    var.vt = VT_I8;
    var.hVal.QuadPart = static_cast<LONGLONG>(seconds * 10000000.0);
    m_pPlayer->SetPosition(MFP_POSITIONTYPE_100NS, &var);
    PropVariantClear(&var);
}

double MediaPlayer::GetDuration() {
    if (!m_pPlayer) return 0.0;

    PROPVARIANT var;
    PropVariantInit(&var);
    double result = 0.0;

    HRESULT hr = m_pPlayer->GetDuration(MFP_POSITIONTYPE_100NS, &var);
    if (SUCCEEDED(hr)) {
        if (var.vt == VT_I8) {
            result = static_cast<double>(var.hVal.QuadPart) / 10000000.0;
        } else if (var.vt == VT_UI8) {
            result = static_cast<double>(var.uhVal.QuadPart) / 10000000.0;
        }
    }
    PropVariantClear(&var);
    return result;
}

double MediaPlayer::GetPosition() {
    if (!m_pPlayer) return 0.0;

    PROPVARIANT var;
    PropVariantInit(&var);
    double result = 0.0;

    HRESULT hr = m_pPlayer->GetPosition(MFP_POSITIONTYPE_100NS, &var);
    if (SUCCEEDED(hr)) {
        if (var.vt == VT_I8) {
            result = static_cast<double>(var.hVal.QuadPart) / 10000000.0;
        } else if (var.vt == VT_UI8) {
            result = static_cast<double>(var.uhVal.QuadPart) / 10000000.0;
        }
    }
    PropVariantClear(&var);
    return result;
}

void MediaPlayer::GetNativeVideoSize(int& width,int& height){
    width = 0;
    height = 0;
    if (!m_pPlayer) return;

    SIZE szVideo, szAR;
    if (SUCCEEDED(m_pPlayer->GetNativeVideoSize(&szVideo, &szAR))) {
        width = szVideo.cx;
        height = szVideo.cy;
    }
}
void MediaPlayer::UpdateVideoSize() { if (m_pPlayer) m_pPlayer->UpdateVideo(); }

void MediaPlayer::Shutdown() {
    if (m_pPlayer) { m_pPlayer->Shutdown(); m_pPlayer->Release(); m_pPlayer = nullptr; }
    if (m_pCallback) { m_pCallback->Release(); m_pCallback = nullptr; }
    if (m_mfStarted) { MFShutdown(); m_mfStarted = false; }
}

} // namespace kernelPlayerVidi
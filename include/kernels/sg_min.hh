#ifndef SG_MIN_HH
#define SG_MIN_HH

// [DSP GAIN] Deklarasi minimal ISampleGrabber/ISampleGrabberCB.
// qedit.h sudah dihapus dari Windows SDK modern, jadi kita deklarasikan sendiri
// (pola sama dengan VMR-7 yang didefinisikan lokal di directShowPlayer.cc).

#include <windows.h>
#include <dshow.h>

// {C1F400A0-3F08-11D3-9F0B-006008039E37}  <- Sample Grabber
// [FIX BISU] sebelumnya tertulis C1F400A4 = CLSID_NullRenderer! Akibatnya:
// QI(ISampleGrabber) selalu E_NOINTERFACE dan "audio berhasil dirender"
// ke Null Renderer -- suara dibuang ke void.
static const CLSID CLSID_SampleGrabber =
{ 0xC1F400A0, 0x3F08, 0x11D3, { 0x9F, 0x0B, 0x00, 0x60, 0x08, 0x03, 0x9E, 0x37 } };

// {6B652FFF-11FE-4FCE-92AD-0266B5D7C78F}
static const IID IID_ISampleGrabber =
{ 0x6B652FFF, 0x11FE, 0x4FCE, { 0x92, 0xAD, 0x02, 0x66, 0xB5, 0xD7, 0xC7, 0x8F } };

// {0579154A-2B53-4994-B0D0-E773148EFF85}
static const IID IID_ISampleGrabberCB =
{ 0x0579154A, 0x2B53, 0x4994, { 0xB0, 0xD0, 0xE7, 0x73, 0x14, 0x8E, 0xFF, 0x85 } };

// Subformat WAVEFORMATEXTENSIBLE (hindari dependensi ksmedia.h)
static const GUID SG_SUBTYPE_PCM =
{ 0x00000001, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
static const GUID SG_SUBTYPE_IEEE_FLOAT =
{ 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };

DECLARE_INTERFACE_(ISampleGrabberCB, IUnknown) {
    STDMETHOD(QueryInterface)(THIS_ REFIID, LPVOID*) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;
    STDMETHOD(SampleCB)(THIS_ double SampleTime, IMediaSample* pSample) PURE;
    STDMETHOD(BufferCB)(THIS_ double SampleTime, BYTE* pBuffer, long BufferLen) PURE;
};

DECLARE_INTERFACE_(ISampleGrabber, IUnknown) {
    STDMETHOD(QueryInterface)(THIS_ REFIID, LPVOID*) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;
    STDMETHOD(SetOneShot)(THIS_ long OneShot) PURE;
    STDMETHOD(SetMediaType)(THIS_ const AM_MEDIA_TYPE* pType) PURE;
    STDMETHOD(GetConnectedMediaType)(THIS_ AM_MEDIA_TYPE* pType) PURE;
    STDMETHOD(SetBufferSamples)(THIS_ long BufferThem) PURE;
    STDMETHOD(GetCurrentBuffer)(THIS_ long* pBufferSize, long* pBuffer) PURE;
    STDMETHOD(GetCurrentSample)(THIS_ IMediaSample** ppSample) PURE;
    STDMETHOD(SetReferenceClock)(THIS_ IReferenceClock* pClock) PURE;
    STDMETHOD(SetCallback)(THIS_ ISampleGrabberCB* pCallback, long WhichMethodToCallback) PURE;
};

#endif // SG_MIN_HH

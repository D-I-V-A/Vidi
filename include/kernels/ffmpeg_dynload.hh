#ifndef FFMPEG_DYNLOAD_HH
#define FFMPEG_DYNLOAD_HH

#include <windows.h>
#include <cstdint>

// ============================================================
// Minimal FFmpeg forward declarations (opaque types)
// ============================================================

struct AVFormatContext;
struct AVCodecContext;
struct AVStream;
struct AVPacket;
struct AVSubtitle;
struct AVSubtitleRect;
struct AVCodec;
struct AVCodecParameters;
struct AVDictionary;
struct AVDictionaryEntry;

struct AVRational {
    int num;
    int den;
};

// ============================================================
// AVCodecID constants (dari libavcodec/codec_id.h)
// Nilai numerik di-hardcode agar tidak butuh header FFmpeg.
// ============================================================

static constexpr int VIDI_AV_CODEC_ID_DVD_SUBTITLE = 0x17000;
static constexpr int VIDI_AV_CODEC_ID_DVB_SUBTITLE = 0x17001;
static constexpr int VIDI_AV_CODEC_ID_TEXT = 0x17002;
static constexpr int VIDI_AV_CODEC_ID_XSUB = 0x17003;
static constexpr int VIDI_AV_CODEC_ID_SSA = 0x17004;
static constexpr int VIDI_AV_CODEC_ID_MOV_TEXT = 0x17005;
static constexpr int VIDI_AV_CODEC_ID_HDMV_PGS_SUBTITLE = 0x17006;
static constexpr int VIDI_AV_CODEC_ID_DVB_TELETEXT = 0x17007;
static constexpr int VIDI_AV_CODEC_ID_SRT = 0x17008;
static constexpr int VIDI_AV_CODEC_ID_MICRODVD = 0x17009;
static constexpr int VIDI_AV_CODEC_ID_EIA_608 = 0x1700A;
static constexpr int VIDI_AV_CODEC_ID_JACOSUB = 0x1700B;
static constexpr int VIDI_AV_CODEC_ID_SAMI = 0x1700C;
static constexpr int VIDI_AV_CODEC_ID_REALTEXT = 0x1700D;
static constexpr int VIDI_AV_CODEC_ID_STL = 0x1700E;
static constexpr int VIDI_AV_CODEC_ID_SUBVIEWER1 = 0x1700F;
static constexpr int VIDI_AV_CODEC_ID_SUBVIEWER = 0x17010;
static constexpr int VIDI_AV_CODEC_ID_SUBRIP = 0x17011;
static constexpr int VIDI_AV_CODEC_ID_WEBVTT = 0x17012;
static constexpr int VIDI_AV_CODEC_ID_MPL2 = 0x17013;
static constexpr int VIDI_AV_CODEC_ID_VPLAYER = 0x17014;
static constexpr int VIDI_AV_CODEC_ID_PJS = 0x17015;
static constexpr int VIDI_AV_CODEC_ID_ASS = 0x17016;
static constexpr int VIDI_AV_CODEC_ID_HDMV_TEXT_SUBTITLE = 0x17017;
static constexpr int VIDI_AV_CODEC_ID_TTML = 0x17018;

// ============================================================
// Compatible struct layouts (opaque FFmpeg types)
// ============================================================

struct AVCodecParametersCompat {
    int codec_type;     // +0
    int codec_id;       // +4
    uint32_t codec_tag; // +8
    uint8_t* extradata; // +16
    int extradata_size; // +24
    char _pad[256];
};

struct AVStreamCompat {
    const void* av_class;              // +0
    int index;                         // +8
    int id;                            // +12
    AVCodecParametersCompat* codecpar; // +16
    void* priv_data;                   // +24
    AVRational time_base;              // +32
    int64_t start_time;                // +40
    int64_t duration;                  // +48
    int64_t nb_frames;                 // +56
    int disposition;                   // +64
    int discard;                       // +68
    AVRational sample_aspect_ratio;    // +72
    void* metadata;                    // +80
    char _pad[128];
};

struct AVFormatContextCompat {
    const void* av_class;     // +0
    const void* iformat;      // +8
    const void* oformat;      // +16
    void* priv_data;          // +24
    void* pb;                 // +32
    int ctx_flags;            // +40
    unsigned int nb_streams;  // +44
    AVStreamCompat** streams; // +48
    char _pad[512];
};

struct AVPacketRaw {
    void* buf;
    int64_t pts;
    int64_t dts;
    uint8_t* data;
    int size;
    int stream_index;
    int flags;
    int64_t duration;
    int64_t pos;
    char _pad[256];
};

enum AVMediaType {
    AVMEDIA_TYPE_UNKNOWN = -1,
    AVMEDIA_TYPE_VIDEO,
    AVMEDIA_TYPE_AUDIO,
    AVMEDIA_TYPE_DATA,
    AVMEDIA_TYPE_SUBTITLE,
    AVMEDIA_TYPE_ATTACHMENT
};

// ============================================================
// Function pointer typedefs
// ============================================================

typedef int (*fn_avformat_open_input)(AVFormatContext**, const char*, void*, void**);
typedef AVRational (*fn_av_stream_get_time_base)(const AVStream*);
typedef int (*fn_avformat_find_stream_info)(AVFormatContext*, void**);
typedef void (*fn_avformat_close_input)(AVFormatContext**);
typedef int (*fn_av_find_best_stream)(AVFormatContext*, AVMediaType, int, int, const AVCodec**, int);
typedef int (*fn_av_read_frame)(AVFormatContext*, AVPacket*);
typedef int (*fn_av_seek_frame)(AVFormatContext*, int, int64_t, int);
typedef int (*fn_av_dict_get)(void*, const char*, const char*, int);

typedef const char* (*fn_avcodec_descriptor_name)(int);
typedef AVCodecContext* (*fn_avcodec_alloc_context3)(const AVCodec*);
typedef void (*fn_avcodec_free_context)(AVCodecContext**);
typedef int (*fn_avcodec_parameters_to_context)(AVCodecContext*, const AVCodecParameters*);
typedef int (*fn_avcodec_open2)(AVCodecContext*, const AVCodec*, void**);
typedef int (*fn_avcodec_send_packet)(AVCodecContext*, const AVPacket*);
typedef int (*fn_avcodec_receive_subtitle)(AVCodecContext*, AVSubtitle*);

typedef AVPacket* (*fn_av_packet_alloc)(void);
typedef void (*fn_av_packet_free)(AVPacket**);
typedef void (*fn_av_packet_unref)(AVPacket*);

typedef void (*fn_avsubtitle_free)(AVSubtitle*);

typedef const AVCodec* (*fn_avcodec_find_decoder)(int);

struct FFmpegFuncs {
    fn_avformat_open_input avformat_open_input;
    fn_av_stream_get_time_base av_stream_get_time_base;
    fn_avformat_find_stream_info avformat_find_stream_info;
    fn_avformat_close_input avformat_close_input;
    fn_av_find_best_stream av_find_best_stream;
    fn_av_read_frame av_read_frame;
    fn_av_seek_frame av_seek_frame;

    fn_avcodec_descriptor_name avcodec_descriptor_name;
    fn_avcodec_alloc_context3 avcodec_alloc_context3;
    fn_avcodec_free_context avcodec_free_context;
    fn_avcodec_parameters_to_context avcodec_parameters_to_context;
    fn_avcodec_open2 avcodec_open2;
    fn_avcodec_send_packet avcodec_send_packet;
    fn_avcodec_receive_subtitle avcodec_receive_subtitle;
    fn_avcodec_find_decoder avcodec_find_decoder;

    fn_av_packet_alloc av_packet_alloc;
    fn_av_packet_free av_packet_free;
    fn_av_packet_unref av_packet_unref;

    fn_avsubtitle_free avsubtitle_free;
    fn_av_dict_get av_dict_get;
};

#endif // FFMPEG_DYNLOAD_HH
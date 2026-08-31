#ifndef FFMPEG_DYNLOAD_HH
#define FFMPEG_DYNLOAD_HH

#include <windows.h>
#include <cstdint>

// ============================================================
// Minimal FFmpeg forward declarations (opaque types)
// Kita hanya perlu pointer, isi internal-nya ada di dalam DLL
// ============================================================

struct AVFormatContext;
struct AVCodecContext;
struct AVStream;
struct AVPacket;
struct AVSubtitle;
struct AVSubtitleRect;
struct AVCodec;
struct AVCodecParameters;
struct AVRational {
    int num;
    int den;
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
// Function pointer typedefs (resolved via GetProcAddress)
// ============================================================

// avformat
typedef int (*fn_avformat_open_input)(AVFormatContext**, const char*, void*, void**);
typedef AVRational (*fn_av_stream_get_time_base)(const AVStream*);
typedef int (*fn_avformat_find_stream_info)(AVFormatContext*, void**);
typedef void (*fn_avformat_close_input)(AVFormatContext**);
typedef int (*fn_av_find_best_stream)(AVFormatContext*, AVMediaType, int, int, const AVCodec**, int);
typedef int (*fn_av_read_frame)(AVFormatContext*, AVPacket*);

// avcodec
typedef const char* (*fn_avcodec_descriptor_name)(int);
typedef AVCodecContext* (*fn_avcodec_alloc_context3)(const AVCodec*);
typedef void (*fn_avcodec_free_context)(AVCodecContext**);
typedef int (*fn_avcodec_parameters_to_context)(AVCodecContext*, const AVCodecParameters*);
typedef int (*fn_avcodec_open2)(AVCodecContext*, const AVCodec*, void**);
typedef int (*fn_avcodec_send_packet)(AVCodecContext*, const AVPacket*);
typedef int (*fn_avcodec_receive_subtitle)(AVCodecContext*, AVSubtitle*);

// avpacket
typedef AVPacket* (*fn_av_packet_alloc)(void);
typedef void (*fn_av_packet_free)(AVPacket**);
typedef void (*fn_av_packet_unref)(AVPacket*);

// avutil
typedef void (*fn_avsubtitle_free)(AVSubtitle*);

// avcodec search
typedef const AVCodec* (*fn_avcodec_find_decoder)(int);

// ============================================================
// Struct untuk menyimpan semua function pointer
// ============================================================

struct FFmpegFuncs {
    // avformat
    fn_avformat_open_input avformat_open_input;
    fn_av_stream_get_time_base av_stream_get_time_base;
    fn_avformat_find_stream_info avformat_find_stream_info;
    fn_avformat_close_input avformat_close_input;
    fn_av_find_best_stream av_find_best_stream;
    fn_av_read_frame av_read_frame;

    // avcodec
    fn_avcodec_descriptor_name avcodec_descriptor_name;
    fn_avcodec_alloc_context3 avcodec_alloc_context3;
    fn_avcodec_free_context avcodec_free_context;
    fn_avcodec_parameters_to_context avcodec_parameters_to_context;
    fn_avcodec_open2 avcodec_open2;
    fn_avcodec_send_packet avcodec_send_packet;
    fn_avcodec_receive_subtitle avcodec_receive_subtitle;
    fn_avcodec_find_decoder avcodec_find_decoder;

    // avpacket
    fn_av_packet_alloc av_packet_alloc;
    fn_av_packet_free av_packet_free;
    fn_av_packet_unref av_packet_unref;

    // avutil
    fn_avsubtitle_free avsubtitle_free;
};

#endif // FFMPEG_DYNLOAD_HH
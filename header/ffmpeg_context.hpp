#pragma once

#include <memory>
#include <string>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#define usleep(microseconds) Sleep((microseconds)/1000)
#else 
#include <unistd.h>
#endif

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
}

// Smart pointer deleters
auto frame_deleter = [](AVFrame* f) { av_frame_free(&f); };
auto packet_deleter = [](AVPacket* p) { av_packet_free(&p); };
auto filter_inout_deleter = [](AVFilterInOut* f) { avfilter_inout_free(&f); };

// Type aliases for improved readability
using FramePtr = std::unique_ptr<AVFrame, decltype(frame_deleter)>;
using PacketPtr = std::unique_ptr<AVPacket, decltype(packet_deleter)>;
using FilterInOutPtr = std::unique_ptr<AVFilterInOut, decltype(filter_inout_deleter)>;

// Main FFmpeg context class handling video decoding and filtering
class FFmpegContext {
public:
    FFmpegContext();
    ~FFmpegContext();

    // Delete copy/move operations to prevent resource leaks
    FFmpegContext(const FFmpegContext&) = delete;
    FFmpegContext& operator=(const FFmpegContext&) = delete;
    FFmpegContext(FFmpegContext&&) = delete;
    FFmpegContext& operator=(FFmpegContext&&) = delete;

    int open_input_file(const char* filename);
    int init_filters(const char* filters_descr);
    void display_frame(const AVFrame* frame, AVRational time_base);

    AVFormatContext* fmt_ctx;
    AVCodecContext* dec_ctx;
    AVFilterContext* buffersink_ctx;
    AVFilterContext* buffersrc_ctx;
    AVFilterGraph* filter_graph;
    int video_stream_index;
    int64_t last_pts;
};
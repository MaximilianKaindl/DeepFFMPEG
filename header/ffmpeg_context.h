// ffmpeg_context.h
#pragma once
#include "ffmpeg_utils.h"
#include <string>


#define _XOPEN_SOURCE 600 /* for usleep */
#ifdef _WIN32
#include <windows.h>
#define usleep(microseconds) Sleep((microseconds)/1000)
#else 
#include <unistd.h>
#endif

class FFmpegContext {
public:
    FFmpegContext();
    ~FFmpegContext();

    FFmpegContext(const FFmpegContext&) = delete;
    FFmpegContext& operator=(const FFmpegContext&) = delete;

    void open_input_file(const std::string& filename);
    void init_filters(const std::string& filters_descr);
    void process_frames();

private:
    void display_frame(const AVFrame* frame, AVRational time_base);
    void check_av_error(int error_code, const std::string& operation);
    void send_packet_to_decoder(const PacketPtr& packet);
    void receive_and_process_frames(FramePtr& frame, FramePtr& filt_frame);
    void flush_decoder_and_filters(FramePtr& frame, FramePtr& filt_frame);

    AVFormatContext* fmt_ctx{ nullptr };
    AVCodecContext* dec_ctx{ nullptr };
    AVFilterContext* buffersink_ctx{ nullptr };
    AVFilterContext* buffersrc_ctx{ nullptr };
    AVFilterGraph* filter_graph{ nullptr };
    int video_stream_index{ -1 };
    int64_t last_pts{ AV_NOPTS_VALUE };
};
/**
 * @file
 * API example for decoding and filtering
 * @example decode_filter_video.cpp
 */

#define _XOPEN_SOURCE 600 /* for usleep */
#include <iostream>
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

// Custom deleters
auto frame_deleter = [](AVFrame* f) { av_frame_free(&f); };
auto packet_deleter = [](AVPacket* p) { av_packet_free(&p); };
auto filter_inout_deleter = [](AVFilterInOut* f) { avfilter_inout_free(&f); };

// Type aliases for cleaner code
using FramePtr = std::unique_ptr<AVFrame, decltype(frame_deleter)>;
using PacketPtr = std::unique_ptr<AVPacket, decltype(packet_deleter)>;
using FilterInOutPtr = std::unique_ptr<AVFilterInOut, decltype(filter_inout_deleter)>;

const char* filter_descr = "scale=78:24,transpose=cclock";
/* other way:
   scale=78:24 [scl]; [scl] transpose=cclock // assumes "[in]" and "[out]" to be input output pads respectively
 */

class FFmpegContext {
public:
    FFmpegContext() :
        fmt_ctx(nullptr),
        dec_ctx(nullptr),
        buffersink_ctx(nullptr),
        buffersrc_ctx(nullptr),
        filter_graph(nullptr),
        video_stream_index(-1),
        last_pts(AV_NOPTS_VALUE) {}

    ~FFmpegContext() {
        avfilter_graph_free(&filter_graph);
        avcodec_free_context(&dec_ctx);
        avformat_close_input(&fmt_ctx);
    }

    int open_input_file(const char* filename) {
        const AVCodec* dec;
        int ret;

        if ((ret = avformat_open_input(&fmt_ctx, filename, nullptr, nullptr)) < 0) {
            throw std::runtime_error("Cannot open input file");
        }

        if ((ret = avformat_find_stream_info(fmt_ctx, nullptr)) < 0) {
            throw std::runtime_error("Cannot find stream information");
        }

        /* select the video stream */
        ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0);
        if (ret < 0) {
            throw std::runtime_error("Cannot find a video stream in the input file");
        }
        video_stream_index = ret;

        /* create decoding context */
        dec_ctx = avcodec_alloc_context3(dec);
        if (!dec_ctx) {
            throw std::runtime_error("Could not allocate decoder context");
        }
        avcodec_parameters_to_context(dec_ctx, fmt_ctx->streams[video_stream_index]->codecpar);

        /* init the video decoder */
        if ((ret = avcodec_open2(dec_ctx, dec, nullptr)) < 0) {
            throw std::runtime_error("Cannot open video decoder");
        }

        return 0;
    }

    int init_filters(const char* filters_descr) {
        char args[512];
        int ret = 0;
        const AVFilter* buffersrc = avfilter_get_by_name("buffer");
        const AVFilter* buffersink = avfilter_get_by_name("buffersink"); 
        AVRational time_base = fmt_ctx->streams[video_stream_index]->time_base;

        filter_graph = avfilter_graph_alloc();
        if (!filter_graph) {
            throw std::runtime_error("Could not allocate filter graph");
        }
        /* buffer video source: the decoded frames from the decoder will be inserted here. */
        snprintf(args, sizeof(args),
            "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
            dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
            time_base.num, time_base.den,
            dec_ctx->sample_aspect_ratio.num, dec_ctx->sample_aspect_ratio.den);

        ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
            args, nullptr, filter_graph);
        if (ret < 0) {
            throw std::runtime_error("Cannot create buffer source");
        }

        /* buffer video sink: to terminate the filter chain. */
        buffersink_ctx = avfilter_graph_alloc_filter(filter_graph, buffersink, "out");
        if (!buffersink_ctx) {
            throw std::runtime_error("Cannot create buffer sink");
        }

        // Set output pixel format - use input format instead of forcing gray8
        ret = av_opt_set(buffersink_ctx, "pixel_formats", "gray8",
            AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            throw std::runtime_error("Cannot set output pixel format");
        }

        ret = avfilter_init_dict(buffersink_ctx, nullptr);
        if (ret < 0) {
            throw std::runtime_error("Cannot initialize buffer sink");
        }
        /*
         * Set the endpoints for the filter graph. The filter_graph will
         * be linked to the graph described by filters_descr.
         */
        FilterInOutPtr outputs(avfilter_inout_alloc(), filter_inout_deleter);
        FilterInOutPtr inputs(avfilter_inout_alloc(), filter_inout_deleter);
        if (!outputs || !inputs) {
            throw std::runtime_error("Could not allocate filter graph inputs and outputs");
        }
        /*
         * The buffer source output must be connected to the input pad of
         * the first filter described by filters_descr; since the first
         * filter input label is not specified, it is set to "in" by
         * default.
         */
        outputs->name = av_strdup("in");
        outputs->filter_ctx = buffersrc_ctx;
        outputs->pad_idx = 0;
        outputs->next = nullptr;
        /*
         * The buffer sink input must be connected to the output pad of
         * the last filter described by filters_descr; since the last
         * filter output label is not specified, it is set to "out" by
         * default.
         */
        inputs->name = av_strdup("out");
        inputs->filter_ctx = buffersink_ctx;
        inputs->pad_idx = 0;
        inputs->next = nullptr;

        AVFilterInOut* inputs_ptr = inputs.get();
        AVFilterInOut* outputs_ptr = outputs.get();

        if ((ret = avfilter_graph_parse_ptr(filter_graph, filters_descr,
            &inputs_ptr, &outputs_ptr, nullptr)) < 0) {
            throw std::runtime_error("Error parsing filter graph");
        }
        if ((ret = avfilter_graph_config(filter_graph, nullptr)) < 0) {
            throw std::runtime_error("Error configuring filter graph");
        }

        // The ownership of inputs/outputs is transferred to the filter graph,
        // so release them from the unique_ptrs
        inputs.release();
        outputs.release();

        return ret;
    }

    void display_frame(const AVFrame* frame, AVRational time_base) {
        int x, y;
        uint8_t* p0, * p;
        int64_t delay;

        if (frame->pts != AV_NOPTS_VALUE) {
            if (last_pts != AV_NOPTS_VALUE) {
                /* sleep roughly the right amount of time;
                 * usleep is in microseconds, just like AV_TIME_BASE. */
                delay = av_rescale_q(frame->pts - last_pts,
                    time_base, AV_TIME_BASE_Q);
                if (delay > 0 && delay < 1000000)
                    usleep(delay);
            }
            last_pts = frame->pts;
        }

        /* Trivial ASCII grayscale display. */
        p0 = frame->data[0];
        std::cout << "\033c";
        for (y = 0; y < frame->height; y++) {
            p = p0;
            for (x = 0; x < frame->width; x++)
                std::cout << " .-+#"[*(p++) / 52];
            std::cout << '\n';
            p0 += frame->linesize[0];
        }
        std::cout.flush();
    }

    AVFormatContext* fmt_ctx;
    AVCodecContext* dec_ctx;
    AVFilterContext* buffersink_ctx;
    AVFilterContext* buffersrc_ctx;
    AVFilterGraph* filter_graph;
    int video_stream_index;
    int64_t last_pts;
};

int main(int argc, char** argv)
{
    try {
        //if (argc != 2) {
        //    throw std::runtime_error("Usage: " + std::string(argv[0]) + " file");
        //}

        FFmpegContext ctx; 


        FramePtr frame(av_frame_alloc(), frame_deleter);
        FramePtr filt_frame(av_frame_alloc(), frame_deleter);
        PacketPtr packet(av_packet_alloc(), packet_deleter);

        if (!frame || !filt_frame || !packet) {
            throw std::runtime_error("Could not allocate frame or packet");
        }

        ctx.open_input_file("../../../resources/input.mp4");
        ctx.init_filters(filter_descr);

        /* read all packets */
        while (true) {
            int ret = 0;
            if ((ret = av_read_frame(ctx.fmt_ctx, packet.get())) < 0)
                break;

            if (packet->stream_index == ctx.video_stream_index) {
                ret = avcodec_send_packet(ctx.dec_ctx, packet.get());
                if (ret < 0) {
                    throw std::runtime_error("Error while sending packet to decoder");
                }

                while (ret >= 0) {
                    ret = avcodec_receive_frame(ctx.dec_ctx, frame.get());
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                        break;
                    if (ret < 0)
                        throw std::runtime_error("Error while receiving frame from decoder");

                    frame->pts = frame->best_effort_timestamp;

                    /* push the decoded frame into the filtergraph */
                    if (av_buffersrc_add_frame_flags(ctx.buffersrc_ctx, frame.get(),
                        AV_BUFFERSRC_FLAG_KEEP_REF) < 0) {
                        throw std::runtime_error("Error while feeding the filtergraph");
                    }

                    /* pull filtered frames from the filtergraph */
                    while (true) {
                        ret = av_buffersink_get_frame(ctx.buffersink_ctx, filt_frame.get());
                        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                            break;
                        if (ret < 0)
                            throw std::runtime_error("Error while getting frame from filtergraph");

                        ctx.display_frame(filt_frame.get(),
                            ctx.buffersink_ctx->inputs[0]->time_base);
                        av_frame_unref(filt_frame.get());
                    }
                    av_frame_unref(frame.get());
                }
            }
            av_packet_unref(packet.get());
        }

        /* flush the filter graph */
        if (av_buffersrc_add_frame_flags(ctx.buffersrc_ctx, nullptr, 0) < 0) {
            throw std::runtime_error("Error while closing the filtergraph");
        }

        /* pull remaining frames from the filtergraph */
        while (true) {
            int ret = av_buffersink_get_frame(ctx.buffersink_ctx, filt_frame.get());
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            if (ret < 0)
                throw std::runtime_error("Error while getting frame from filtergraph");

            ctx.display_frame(filt_frame.get(), ctx.buffersink_ctx->inputs[0]->time_base);
            av_frame_unref(filt_frame.get());
        }

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
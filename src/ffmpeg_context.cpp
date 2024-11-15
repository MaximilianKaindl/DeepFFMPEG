#include "ffmpeg_context.hpp"
#include <iostream>

FFmpegContext::FFmpegContext() :
    fmt_ctx(nullptr),
    dec_ctx(nullptr),
    buffersink_ctx(nullptr),
    buffersrc_ctx(nullptr),
    filter_graph(nullptr),
    video_stream_index(-1),
    last_pts(AV_NOPTS_VALUE) {}

FFmpegContext::~FFmpegContext() {
    avfilter_graph_free(&filter_graph);
    avcodec_free_context(&dec_ctx);
    avformat_close_input(&fmt_ctx);
}

int FFmpegContext::open_input_file(const char* filename) {
    const AVCodec* dec;
    int ret;

    if ((ret = avformat_open_input(&fmt_ctx, filename, nullptr, nullptr)) < 0) {
        throw std::runtime_error("Cannot open input file");
    }

    if ((ret = avformat_find_stream_info(fmt_ctx, nullptr)) < 0) {
        throw std::runtime_error("Cannot find stream information");
    }

    ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0);
    if (ret < 0) {
        throw std::runtime_error("Cannot find a video stream in the input file");
    }
    video_stream_index = ret;

    dec_ctx = avcodec_alloc_context3(dec);
    if (!dec_ctx) {
        throw std::runtime_error("Could not allocate decoder context");
    }
    avcodec_parameters_to_context(dec_ctx, fmt_ctx->streams[video_stream_index]->codecpar);

    if ((ret = avcodec_open2(dec_ctx, dec, nullptr)) < 0) {
        throw std::runtime_error("Cannot open video decoder");
    }

    return 0;
}

int FFmpegContext::init_filters(const char* filters_descr) {
    char args[512];
    int ret = 0;
    const AVFilter* buffersrc = avfilter_get_by_name("buffer");
    const AVFilter* buffersink = avfilter_get_by_name("buffersink");
    AVRational time_base = fmt_ctx->streams[video_stream_index]->time_base;

    filter_graph = avfilter_graph_alloc();
    if (!filter_graph) {
        throw std::runtime_error("Could not allocate filter graph");
    }

    // Configure buffer source
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

    // Configure buffer sink
    buffersink_ctx = avfilter_graph_alloc_filter(filter_graph, buffersink, "out");
    if (!buffersink_ctx) {
        throw std::runtime_error("Cannot create buffer sink");
    }

    ret = av_opt_set(buffersink_ctx, "pixel_formats", "gray8",
        AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        throw std::runtime_error("Cannot set output pixel format");
    }

    ret = avfilter_init_dict(buffersink_ctx, nullptr);
    if (ret < 0) {
        throw std::runtime_error("Cannot initialize buffer sink");
    }

    // Setup filter graph endpoints
    FilterInOutPtr outputs(avfilter_inout_alloc(), filter_inout_deleter);
    FilterInOutPtr inputs(avfilter_inout_alloc(), filter_inout_deleter);
    if (!outputs || !inputs) {
        throw std::runtime_error("Could not allocate filter graph inputs/outputs");
    }

    outputs->name = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = nullptr;

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

    inputs.release();
    outputs.release();

    return ret;
}

void FFmpegContext::display_frame(const AVFrame* frame, AVRational time_base) {
    int x, y;
    uint8_t* p0, * p;
    int64_t delay;

    if (frame->pts != AV_NOPTS_VALUE) {
        if (last_pts != AV_NOPTS_VALUE) {
            delay = av_rescale_q(frame->pts - last_pts,
                time_base, AV_TIME_BASE_Q);
            if (delay > 0 && delay < 1000000)
                usleep(delay);
        }
        last_pts = frame->pts;
    }

    p0 = frame->data[0];
    std::cout << "\033c";  // Clear screen
    for (y = 0; y < frame->height; y++) {
        p = p0;
        for (x = 0; x < frame->width; x++)
            std::cout << " .-+#"[*(p++) / 52];
        std::cout << '\n';
        p0 += frame->linesize[0];
    }
    std::cout.flush();
}
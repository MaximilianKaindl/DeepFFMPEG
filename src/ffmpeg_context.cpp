// ffmpeg_context.cpp
#include "ffmpeg_context.h"
#include "ffmpeg_logger.h"

#include <iostream>
extern "C" {
#include <libavutil/opt.h>
}

/**
* Implements FFmpeg context for video decoding and filtering
* Based on FFmpeg decode_filter example
*/

FFmpegContext::FFmpegContext() {
    LOG_INFO("Initializing FFmpeg context");
}

FFmpegContext::~FFmpegContext() {
    LOG_INFO("Cleaning up FFmpeg context");
    avfilter_graph_free(&filter_graph);
    avcodec_free_context(&dec_ctx);
    avformat_close_input(&fmt_ctx);
}

void FFmpegContext::check_av_error(const int error_code, const std::string& operation) {
    // Check FFmpeg error code and throw exception with details if error occurred
    if (error_code < 0) {
        std::string error_msg = FFmpegLogger::av_error_to_string(error_code);
        LOG_ERR("Failed to " + operation + ": " + error_msg);
        throw FFmpegException(operation + " failed: " + error_msg, error_code);
    }
}

void FFmpegContext::open_input_file(const std::string& filename) {
    LOG_INFO("Opening input file: " + filename);
    const AVCodec* dec;
    int ret;

    // Open the input file to read from it
    ret = avformat_open_input(&fmt_ctx, filename.c_str(), nullptr, nullptr);
    check_av_error(ret, "open input");

    // Read packets of the file to get stream information
    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    check_av_error(ret, "find stream info");

    // Find the best video stream
    ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0);
    check_av_error(ret, "find video stream");
    video_stream_index = ret;

    // Allocate a codec context for the decoder
    dec_ctx = avcodec_alloc_context3(dec);
    if (!dec_ctx) {
        throw FFmpegException("Failed to allocate decoder context");
    }

    // Fill the codec context based on the values from the supplied codec parameters
    ret = avcodec_parameters_to_context(dec_ctx, fmt_ctx->streams[video_stream_index]->codecpar);
    check_av_error(ret, "copy codec params");

    // Initialize the decoder
    ret = avcodec_open2(dec_ctx, dec, nullptr);
    check_av_error(ret, "open decoder");
}

void FFmpegContext::init_filters(const std::string& filters_descr) {
    LOG_INFO("Initializing filters: " + filters_descr);

    const AVFilter* buffersrc = avfilter_get_by_name("buffer");
    const AVFilter* buffersink = avfilter_get_by_name("buffersink");
    FilterInOutPtr outputs(avfilter_inout_alloc());
    FilterInOutPtr inputs(avfilter_inout_alloc());
    AVRational time_base = fmt_ctx->streams[video_stream_index]->time_base;
    char args[512];
    int ret;

    // Create filter graph
    filter_graph = avfilter_graph_alloc();
    if (!filter_graph || !outputs || !inputs) {
        throw FFmpegException("Failed to allocate filter resources");
    }

    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    snprintf(args, sizeof(args),
        "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
        dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
        time_base.num, time_base.den,
        dec_ctx->sample_aspect_ratio.num, dec_ctx->sample_aspect_ratio.den);

    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
        args, nullptr, filter_graph);
    check_av_error(ret, "create buffer source");

    /* buffer video sink: to terminate the filter chain. */
    buffersink_ctx = avfilter_graph_alloc_filter(filter_graph, buffersink, "out");
    if (!buffersink_ctx) {
        throw FFmpegException("Failed to allocate buffer sink");
    }

    // Configure output pixel format
    ret = av_opt_set_bin(buffersink_ctx, "pix_fmts",
        reinterpret_cast<uint8_t*>(&dec_ctx->pix_fmt), sizeof(dec_ctx->pix_fmt),
        AV_OPT_SEARCH_CHILDREN);
    check_av_error(ret, "set output format");

    ret = avfilter_init_dict(buffersink_ctx, nullptr);
    check_av_error(ret, "initialize buffer sink");

    /* Set the endpoints for the filter graph. The filter_graph will
     * be linked to the graph described by filters_descr.
     */
    outputs->name = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = nullptr;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = nullptr;

    // Workaround with smart pointers
    AVFilterInOut* inputs_ptr = inputs.get();
    AVFilterInOut* outputs_ptr = outputs.get();

    ret = avfilter_graph_parse_ptr(filter_graph, filters_descr.c_str(),
        &inputs_ptr, &outputs_ptr, nullptr);
    check_av_error(ret, "parse filter graph");

    inputs.release();
    outputs.release();

    ret = avfilter_graph_config(filter_graph, nullptr);
    check_av_error(ret, "configure filter graph");
}

void FFmpegContext::process_frames() {
    LOG_INFO("Starting frame processing");

    FramePtr frame(av_frame_alloc());
    FramePtr filt_frame(av_frame_alloc());
    PacketPtr packet(av_packet_alloc());

    if (!frame || !filt_frame || !packet) {
        throw FFmpegException("Failed to allocate frame/packet resources");
    }

    /* read all packets */
    while (true) {
        int ret = av_read_frame(fmt_ctx, packet.get());
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                flush_decoder_and_filters(frame, filt_frame);
                break;
            }
            check_av_error(ret, "read frame");
        }

        if (packet->stream_index == video_stream_index) {
            send_packet_to_decoder(packet);
            receive_and_process_frames(frame, filt_frame);
        }
        av_packet_unref(packet.get());
    }
}

void FFmpegContext::send_packet_to_decoder(const PacketPtr& packet) {
    int ret = avcodec_send_packet(dec_ctx, packet.get());
    check_av_error(ret, "send packet to decoder");
}

void FFmpegContext::receive_and_process_frames(FramePtr& frame, FramePtr& filt_frame) {
    while (true) {
        int ret = avcodec_receive_frame(dec_ctx, frame.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        check_av_error(ret, "receive frame from decoder");

        /* Use the best effort timestamp */
        frame->pts = frame->best_effort_timestamp;

        /* push the decoded frame into the filtergraph */
        ret = av_buffersrc_add_frame_flags(buffersrc_ctx, frame.get(),
            AV_BUFFERSRC_FLAG_KEEP_REF);
        check_av_error(ret, "feed frame to filter graph");

        /* pull filtered frames from the filtergraph */
        while (true) {
            ret = av_buffersink_get_frame(buffersink_ctx, filt_frame.get());
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }
            check_av_error(ret, "get frame from filter graph");

            //display_frame(filt_frame.get(), buffersink_ctx->inputs[0]->time_base);
            av_frame_unref(filt_frame.get());
        }
        av_frame_unref(frame.get());
    }
}

void FFmpegContext::flush_decoder_and_filters(FramePtr& frame, FramePtr& filt_frame) {
    LOG_INFO("Flushing decoder and filters");

    /* flush the decoder */
    avcodec_send_packet(dec_ctx, nullptr);
    receive_and_process_frames(frame, filt_frame);

    /* flush the filters */
    av_buffersrc_add_frame_flags(buffersrc_ctx, nullptr, 0);
    while (true) {
        int ret = av_buffersink_get_frame(buffersink_ctx, filt_frame.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        check_av_error(ret, "get frame from filter graph during flush");

        display_frame(filt_frame.get(), buffersink_ctx->inputs[0]->time_base);
        av_frame_unref(filt_frame.get());
    }
}

void FFmpegContext::display_frame(const AVFrame* frame, AVRational time_base) {
    /* compute proper sleep time based on the time base */
    if (frame->pts != AV_NOPTS_VALUE) {
        if (last_pts != AV_NOPTS_VALUE) {
            int64_t delay = av_rescale_q(frame->pts - last_pts,
                time_base, AV_TIME_BASE_Q);
            if (delay > 0 && delay < 1000000)
                usleep(delay);
        }
        last_pts = frame->pts;
    }

    /* Trivial ASCII grayscale display */
    uint8_t* p0 = frame->data[0];
    std::cout << "\033c";  // Clear screen
    for (int y = 0; y < frame->height; y++) {
        uint8_t* p = p0;
        for (int x = 0; x < frame->width; x++)
            std::cout << " .-+#"[*(p++) / 52];
        std::cout << '\n';
        p0 += frame->linesize[0];
    }
    std::cout.flush();
}
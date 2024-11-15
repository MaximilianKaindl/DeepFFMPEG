#include "ffmpeg_context.hpp"
#include <iostream>

const char* filter_descr = "scale=78:24,transpose=cclock";

int main(int argc, char** argv) {
    try {
        FFmpegContext ctx;

        // Initialize smart pointers for frames and packet
        FramePtr frame(av_frame_alloc(), frame_deleter);
        FramePtr filt_frame(av_frame_alloc(), frame_deleter);
        PacketPtr packet(av_packet_alloc(), packet_deleter);

        if (!frame || !filt_frame || !packet) {
            throw std::runtime_error("Could not allocate frame or packet");
        }

        ctx.open_input_file("../../../resources/input.mp4");
        ctx.init_filters(filter_descr);

        // Main decoding and filtering loop
        while (true) {
            int ret = av_read_frame(ctx.fmt_ctx, packet.get());
            if (ret < 0) break;

            if (packet->stream_index == ctx.video_stream_index) {
                ret = avcodec_send_packet(ctx.dec_ctx, packet.get());
                if (ret < 0) {
                    throw std::runtime_error("Error sending packet to decoder");
                }

                while (ret >= 0) {
                    ret = avcodec_receive_frame(ctx.dec_ctx, frame.get());
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                        break;
                    if (ret < 0)
                        throw std::runtime_error("Error receiving frame from decoder");

                    frame->pts = frame->best_effort_timestamp;

                    // Process frame through filter graph
                    if (av_buffersrc_add_frame_flags(ctx.buffersrc_ctx, frame.get(),
                        AV_BUFFERSRC_FLAG_KEEP_REF) < 0) {
                        throw std::runtime_error("Error feeding the filtergraph");
                    }

                    // Retrieve and display filtered frames
                    while (true) {
                        ret = av_buffersink_get_frame(ctx.buffersink_ctx, filt_frame.get());
                        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                            break;
                        if (ret < 0)
                            throw std::runtime_error("Error getting frame from filtergraph");

                        ctx.display_frame(filt_frame.get(),
                            ctx.buffersink_ctx->inputs[0]->time_base);
                        av_frame_unref(filt_frame.get());
                    }
                    av_frame_unref(frame.get());
                }
            }
            av_packet_unref(packet.get());
        }

        // Flush remaining frames
        if (av_buffersrc_add_frame_flags(ctx.buffersrc_ctx, nullptr, 0) < 0) {
            throw std::runtime_error("Error closing the filtergraph");
        }

        while (true) {
            int ret = av_buffersink_get_frame(ctx.buffersink_ctx, filt_frame.get());
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            if (ret < 0)
                throw std::runtime_error("Error getting frame from filtergraph");

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
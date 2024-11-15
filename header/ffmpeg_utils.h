#pragma once
#include <memory>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
}

struct AVFrameDeleter { void operator()(AVFrame* f) { av_frame_free(&f); } };
struct AVPacketDeleter { void operator()(AVPacket* p) { av_packet_free(&p); } };
struct AVFilterInOutDeleter { void operator()(AVFilterInOut* f) { avfilter_inout_free(&f); } };

using FramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;
using PacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;
using FilterInOutPtr = std::unique_ptr<AVFilterInOut, AVFilterInOutDeleter>;
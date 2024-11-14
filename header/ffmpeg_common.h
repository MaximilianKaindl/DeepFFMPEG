#ifndef FFMPEG_COMMON_H
#define FFMPEG_COMMON_H

#include <string>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

std::string format_av_error(const std::string& message, int error_code);
void check_ff_result(int result, const std::string& action);

// Custom deleters
struct FormatContextDeleter {
	void operator()(AVFormatContext* ptr) const;
};

struct CodecContextDeleter {
	void operator()(AVCodecContext* ptr) const;
};

struct FrameDeleter {
	void operator()(AVFrame* ptr) const;
};

struct PacketDeleter {
	void operator()(AVPacket* ptr) const;
};

#endif // FFMPEG_COMMON_H
#ifndef FFMPEG_IO_H
#define FFMPEG_IO_H

#include <memory>
#include "ffmpeg_common.h"

class FFmpegInputContext {
public:
	explicit FFmpegInputContext(const std::string& filename);

	void initialize();
	int get_video_stream_index() const { return video_stream_index_; }
	const AVCodec* get_codec() const { return codec_; }
	AVCodecParameters* get_codec_parameters() const { return codec_parameters_; }
	AVStream* get_video_stream() const;

	std::unique_ptr<AVFormatContext, FormatContextDeleter> format_context;
	std::unique_ptr<AVCodecContext, CodecContextDeleter> codec_context;
	std::unique_ptr<AVFrame, FrameDeleter> frame;
	std::unique_ptr<AVPacket, PacketDeleter> packet;

private:
	void setup_input_format_context();
	void setup_video_stream();
	void setup_codec_context();
	void setup_frame_buffers();

	std::string filename_;
	const AVCodec* codec_;
	AVCodecParameters* codec_parameters_;
	int video_stream_index_;
};

class FFmpegOutputContext {
public:
	void initialize(const std::string& filename, const FFmpegInputContext& input_context);
	void write_frame(AVPacket* packet) const;
	void finalize() const;

	std::unique_ptr<AVFormatContext, FormatContextDeleter> format_context;
	std::unique_ptr<AVCodecContext, CodecContextDeleter> codec_context;
	AVStream* stream;

private:
	void setup_stream(const FFmpegInputContext& input_context);
	void setup_codec(const FFmpegInputContext& input_context);
	void open_output_file(const std::string& filename) const;
};

#endif // FFMPEG_IO_H
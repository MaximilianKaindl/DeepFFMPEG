#ifndef FFMPEG_PROCESSOR_H
#define FFMPEG_PROCESSOR_H
#include "FFmpegLogger.h"

#include <string>
#include <memory>
#include <mutex>

// Forward declarations for FFmpeg structures
struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct AVCodec;
struct AVCodecParameters;

// Convenience macros for logging
#define LOG_DEBUG(msg)   FFmpegLogger::get_instance().log(FFmpegLogger::Level::DEBUG, msg, __FILE__, __LINE__)
#define LOG_INFO(msg)    FFmpegLogger::get_instance().log(FFmpegLogger::Level::INFO, msg, __FILE__, __LINE__)
#define LOG_WARNING(msg) FFmpegLogger::get_instance().log(FFmpegLogger::Level::WARNING, msg, __FILE__, __LINE__)
#define LOG_ERROR(msg)   FFmpegLogger::get_instance().log(FFmpegLogger::Level::ERROR, msg, __FILE__, __LINE__)
#define LOG_FATAL(msg)   FFmpegLogger::get_instance().log(FFmpegLogger::Level::FATAL, msg, __FILE__, __LINE__)

// Helper function declaration
std::string format_av_error(const std::string& message, int error_code);

// Main FFmpeg processor class
class FFmpegProcessor {
public:
	explicit FFmpegProcessor(std::string input_file);

	// Initialize the processor
	void initialize();

	// Process frames
	void process_frames(const int num_frames) const;

private:
	// Custom deleters for RAII
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

	// Helper methods
	static int decode_packet(const AVPacket* p_packet, AVCodecContext* p_codec_context, AVFrame* p_frame);

	// Member variables
	std::unique_ptr<AVFormatContext, FormatContextDeleter> formatContext;
	std::unique_ptr<AVCodecContext, CodecContextDeleter> codecContext;
	std::unique_ptr<AVFrame, FrameDeleter> frame;
	std::unique_ptr<AVPacket, PacketDeleter> packet;

	const AVCodec* codec;
	AVCodecParameters* codecParameters;
	int videoStreamIndex;
	std::string filename;
};

#endif // FFMPEG_PROCESSOR_H
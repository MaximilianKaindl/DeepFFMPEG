#include "FFmpegLogger.h"
#include "ffmpegprocessor.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/timestamp.h>
}
#include <sstream>
#include <chrono>
#include <iomanip>

// Helper function implementation
std::string format_av_error(const std::string& message, int error_code) {
	std::stringstream ss;
	ss << message << ": " << FFmpegLogger::av_error_to_string(error_code)
		<< " (error code: " << error_code << ")";
	return ss.str();
}

// Custom deleters implementation
void FFmpegProcessor::FormatContextDeleter::operator()(AVFormatContext* ptr) const {
	if (ptr) {
		LOG_DEBUG("Closing format context");
		avformat_close_input(&ptr);
	}
}

void FFmpegProcessor::CodecContextDeleter::operator()(AVCodecContext* ptr) const {
	if (ptr) {
		LOG_DEBUG("Freeing codec context");
		avcodec_free_context(&ptr);
	}
}

void FFmpegProcessor::FrameDeleter::operator()(AVFrame* ptr) const {
	if (ptr) {
		LOG_DEBUG("Freeing frame");
		av_frame_free(&ptr);
	}
}

void FFmpegProcessor::PacketDeleter::operator()(AVPacket* ptr) const {
	if (ptr) {
		LOG_DEBUG("Freeing packet");
		av_packet_free(&ptr);
	}
}

// FFmpegProcessor implementation
FFmpegProcessor::FFmpegProcessor(std::string input_file)
	: codec(nullptr)
	, codecParameters(nullptr)
	, videoStreamIndex(-1)
	, filename(std::move(input_file))
{
	LOG_INFO("Creating FFmpegProcessor for file: " + filename);

	formatContext.reset(avformat_alloc_context());
	if (!formatContext) {
		throw FFmpegException("Could not allocate memory for Format Context");
	}
}

void FFmpegProcessor::initialize() {
	LOG_INFO("Initializing FFmpeg processor for: " + filename);

	AVFormatContext* format_context_raw = nullptr;
	int result = avformat_open_input(&format_context_raw, filename.c_str(), nullptr, nullptr);

	if (result < 0) {
		throw FFmpegException(format_av_error("Could not open input file", result), result);
	}

	formatContext.reset(format_context_raw);

	LOG_INFO("Format: " + std::string(formatContext->iformat->name) +
		", duration: " + std::to_string(formatContext->duration) + " us, " +
		"bit_rate: " + std::to_string(formatContext->bit_rate) + " bps");

	result = avformat_find_stream_info(formatContext.get(), nullptr);
	if (result < 0) {
		throw FFmpegException(format_av_error("Could not get stream info", result), result);
	}

	// Find video stream
	for (unsigned int i = 0; i < formatContext->nb_streams; i++) {
		AVCodecParameters* local_codec_params = formatContext->streams[i]->codecpar;
		const AVCodec* local_codec = avcodec_find_decoder(local_codec_params->codec_id);

		if (!local_codec) {
			LOG_WARNING("Unsupported codec for stream " + std::to_string(i));
			continue;
		}

		if (local_codec_params->codec_type == AVMEDIA_TYPE_VIDEO) {
			if (videoStreamIndex == -1) {
				videoStreamIndex = i;
				codec = local_codec;
				codecParameters = local_codec_params;

				LOG_INFO("Found video stream: " + std::to_string(i) +
					" (" + std::to_string(local_codec_params->width) + "x" +
					std::to_string(local_codec_params->height) + ")");
			}
		}
	}

	if (videoStreamIndex == -1) {
		throw FFmpegException("No video stream found in the input file");
	}

	// Set up codec context
	codecContext.reset(avcodec_alloc_context3(codec));
	if (!codecContext) {
		throw FFmpegException("Failed to allocate codec context");
	}

	result = avcodec_parameters_to_context(codecContext.get(), codecParameters);
	if (result < 0) {
		throw FFmpegException(format_av_error("Failed to copy codec parameters", result), result);
	}

	result = avcodec_open2(codecContext.get(), codec, nullptr);
	if (result < 0) {
		throw FFmpegException(format_av_error("Failed to open codec", result), result);
	}

	// Allocate frame and packet
	frame.reset(av_frame_alloc());
	if (!frame) {
		throw FFmpegException("Failed to allocate frame");
	}

	packet.reset(av_packet_alloc());
	if (!packet) {
		throw FFmpegException("Failed to allocate packet");
	}

	LOG_INFO("Initialization completed successfully");
}

int FFmpegProcessor::decode_packet(const AVPacket* p_packet, AVCodecContext* p_codec_context, AVFrame* p_frame) {
	int response = avcodec_send_packet(p_codec_context, p_packet);

	if (response < 0) {
		LOG_ERROR(format_av_error("Error sending packet to decoder", response));
		return response;
	}

	while (response >= 0) {
		response = avcodec_receive_frame(p_codec_context, p_frame);
		if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
			break;
		}
		else if (response < 0) {
			LOG_ERROR(format_av_error("Error receiving frame from decoder", response));
			return response;
		}

		if (response >= 0) {
			LOG_DEBUG("Processing frame " + std::to_string(p_codec_context->frame_num) +
				" (type=" + av_get_picture_type_char(p_frame->pict_type) +
				", size=" + std::to_string(p_frame->pkt_size) +
				", format=" + std::to_string(p_frame->format) +
				") pts " + std::to_string(p_frame->pts) +
				" key_frame " + std::to_string(p_frame->key_frame) +
				" [DTS " + std::to_string(p_frame->pkt_dts) + "]");
		}
	}
	return 0;
}

void FFmpegProcessor::process_frames(const int num_frames) const {
	LOG_INFO("Starting to process " + std::to_string(num_frames) + " frames");

	int frames_processed = 0;
	while (av_read_frame(formatContext.get(), packet.get()) >= 0) {
		if (packet->stream_index == videoStreamIndex) {
			LOG_DEBUG("Processing frame " + std::to_string(frames_processed + 1) +
				" of " + std::to_string(num_frames));

			if (decode_packet(packet.get(), codecContext.get(), frame.get()) < 0) {
				LOG_ERROR("Failed to decode packet, stopping processing");
				break;
			}

			if (++frames_processed >= num_frames) {
				LOG_INFO("Completed processing " + std::to_string(frames_processed) + " frames");
				break;
			}
		}
		av_packet_unref(packet.get());
	}

	if (frames_processed < num_frames) {
		LOG_WARNING("Reached end of file after processing only " +
			std::to_string(frames_processed) + " frames");
	}
}
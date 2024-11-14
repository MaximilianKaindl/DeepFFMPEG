// ffmpeg_common.cpp
#include "ffmpeg_common.h"
#include "ffmpeg_logger.h"

std::string format_av_error(const std::string& message, int error_code) {
	char error_buf[AV_ERROR_MAX_STRING_SIZE];
	av_strerror(error_code, error_buf, AV_ERROR_MAX_STRING_SIZE);
	return message + ": " + error_buf + " (error code: " + std::to_string(error_code) + ")";
}

void check_ff_result(const int result, const std::string& action) {
	if (result < 0) {
		throw FFmpegException(format_av_error(action, result), result);
	}
}

void FormatContextDeleter::operator()(AVFormatContext* ptr) const {
	if (ptr) {
		LOG_DEBUG("Closing format context");
		avformat_close_input(&ptr);
	}
}

void CodecContextDeleter::operator()(AVCodecContext* ptr) const {
	if (ptr) {
		LOG_DEBUG("Freeing codec context");
		avcodec_free_context(&ptr);
	}
}

void FrameDeleter::operator()(AVFrame* ptr) const {
	if (ptr) {
		LOG_DEBUG("Freeing frame");
		av_frame_free(&ptr);
	}
}

void PacketDeleter::operator()(AVPacket* ptr) const {
	if (ptr) {
		LOG_DEBUG("Freeing packet");
		av_packet_free(&ptr);
	}
}

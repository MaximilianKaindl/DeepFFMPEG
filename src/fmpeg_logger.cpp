#include "ffmpeg_logger.h"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <chrono>
extern "C" {
#include "libavutil/error.h"
}
// FFmpegException implementation
FFmpegException::FFmpegException(const std::string& message, const int error_code)
	: std::runtime_error(message), error_code_(error_code) {}

int FFmpegException::get_error_code() const {
	return error_code_;
}

FFmpegLogger FFmpegLogger::instance;

// FFmpegLogger implementation
FFmpegLogger& FFmpegLogger::get_instance() {
	return instance;
}

std::string FFmpegLogger::get_timestamp() {
	auto now = std::chrono::system_clock::now();
	auto time = std::chrono::system_clock::to_time_t(now);
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		now.time_since_epoch()) % 1000;

	std::tm tm_buf;
#ifdef _WIN32
	localtime_s(&tm_buf, &time);
#else
	localtime_r(&time, &tm_buf);
#endif

	std::stringstream ss;
	ss << std::put_time(&tm_buf, "%H:%M:%S")
		<< '.' << std::setfill('0') << std::setw(3) << ms.count();
	return ss.str();
}

std::string_view FFmpegLogger::level_to_string(Level level) {
	switch (level) {
	case Level::DEBUG:   return "DEBUG";
	case Level::INFO:    return "INFO";
	case Level::WARNING: return "WARNING";
	case Level::ERROR:   return "ERROR";
	case Level::FATAL:   return "FATAL";
	default:            return "UNKNOWN";
	}
}

void FFmpegLogger::log(const Level level, const std::string& message, const char* file, const int line) {
	std::lock_guard<std::mutex> lock(mutex_);

	std::stringstream ss;
	ss << "[" << get_timestamp() << "] "
		<< "[" << level_to_string(level) << "] "
		//<< "[" << file << ":" << line << "] "
		<< message << std::endl;

	std::cerr << ss.str();

	if (log_file_.is_open()) {
		log_file_ << ss.str();
		log_file_.flush();
	}
}

void FFmpegLogger::set_log_file(const std::string& filename) {
	std::lock_guard<std::mutex> lock(mutex_);
	if (log_file_.is_open()) {
		log_file_.close();
	}
	log_file_.open(filename, std::ios::app);
	if (!log_file_.is_open()) {
		throw FFmpegException("Failed to open log file: " + filename);
	}
}

std::string FFmpegLogger::av_error_to_string(const int errnum) {
	char errbuf[AV_ERROR_MAX_STRING_SIZE] = { 0 };
	av_strerror(errnum, errbuf, sizeof(errbuf));
	return std::string(errbuf);
}
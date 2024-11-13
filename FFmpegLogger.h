#ifndef FFMPEG_LOGGER_H
#define FFMPEG_LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <string_view>

// Custom exception class for FFmpeg-specific errors
class FFmpegException : public std::runtime_error {
public:
	explicit FFmpegException(const std::string& message, int error_code = 0);
	int get_error_code() const;

private:
	int error_code_;
};

// Logger class with different severity levels and formatting
class FFmpegLogger {
public:
	enum class Level {
		DEBUG,
		INFO,
		WARNING,
		ERROR,
		FATAL
	};

	static FFmpegLogger& get_instance();
	static FFmpegLogger instance;

	void log(Level level, const std::string& message, const char* file, int line);
	void set_log_file(const std::string& filename);
	static std::string av_error_to_string(int errnum);

	// Delete copy and move constructors/assignments
	FFmpegLogger(const FFmpegLogger&) = delete;
	FFmpegLogger& operator=(const FFmpegLogger&) = delete;
	FFmpegLogger(FFmpegLogger&&) = delete;
	FFmpegLogger& operator=(FFmpegLogger&&) = delete;

private:
	FFmpegLogger() = default;
	static std::string get_timestamp();
	static std::string_view level_to_string(Level level);

	std::mutex mutex_;
	std::ofstream log_file_;
};

// Convenience macros for logging
#define LOG_DEBUG(msg)   FFmpegLogger::get_instance().log(FFmpegLogger::Level::DEBUG, msg, __FILE__, __LINE__)
#define LOG_INFO(msg)    FFmpegLogger::get_instance().log(FFmpegLogger::Level::INFO, msg, __FILE__, __LINE__)
#define LOG_WARNING(msg) FFmpegLogger::get_instance().log(FFmpegLogger::Level::WARNING, msg, __FILE__, __LINE__)
#define LOG_ERROR(msg)   FFmpegLogger::get_instance().log(FFmpegLogger::Level::ERROR, msg, __FILE__, __LINE__)
#define LOG_FATAL(msg)   FFmpegLogger::get_instance().log(FFmpegLogger::Level::FATAL, msg, __FILE__, __LINE__)

#endif // FFMPEG_LOGGER_H
#pragma once
#include <mutex>
#include <fstream>
#include <string>
#include <string_view>

class FFmpegException : public std::runtime_error {
public:
    explicit FFmpegException(const std::string& message, int error_code = 0);
    int get_error_code() const;
private:
    int error_code_;
};

class FFmpegLogger {
public:
    enum class Level { DEBUG, INFO, WARNING, ERR, FATAL };

    static FFmpegLogger& get_instance();
    void log(Level level, const std::string& message, const char* file = __FILE__, int line = __LINE__);
    void set_log_file(const std::string& filename);
    static std::string av_error_to_string(int errnum);

private:
    FFmpegLogger() = default;
    static std::string get_timestamp();
    static std::string_view level_to_string(Level level);

    static FFmpegLogger instance;
    std::mutex mutex_;
    std::ofstream log_file_;
};

#define LOG_DEBUG(msg)   FFmpegLogger::get_instance().log(FFmpegLogger::Level::DEBUG, msg)
#define LOG_INFO(msg)    FFmpegLogger::get_instance().log(FFmpegLogger::Level::INFO, msg)
#define LOG_WARNING(msg) FFmpegLogger::get_instance().log(FFmpegLogger::Level::WARNING, msg)
#define LOG_ERR(msg) FFmpegLogger::get_instance().log(FFmpegLogger::Level::ERR, msg)
#define LOG_FATAL(msg)   FFmpegLogger::get_instance().log(FFmpegLogger::Level::FATAL, msg)
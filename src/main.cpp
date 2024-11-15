// main.cpp
#include "ffmpeg_context.h"
#include "ffmpeg_logger.h"
#include <iostream>

int main(int argc, char** argv) {
    try {
        /*if (argc != 2) {
            std::cerr << "Usage: " << argv[0] << " input_file" << std::endl;
            return 1;
        }*/

        FFmpegLogger::get_instance().set_log_file("ffmpeg_filter.log");
        LOG_INFO("Starting FFmpeg filter application");

        FFmpegContext ctx;
        ctx.open_input_file("../../../resources/input.mp4");
        ctx.init_filters("scale=78:24,transpose=cclock");
        ctx.process_frames();

        LOG_INFO("Processing completed successfully");
        return 0;
    }
    catch (const FFmpegException& e) {
        LOG_ERR("FFmpeg error: " + std::string(e.what()) +
            " (code: " + std::to_string(e.get_error_code()) + ")");
        return 1;
    }
    catch (const std::exception& e) {
        LOG_ERR("Unexpected error: " + std::string(e.what()));
        return 1;
    }
}
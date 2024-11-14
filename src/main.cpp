// main.cpp
#include <iostream>
#include "ffmpeg_logger.h"
#include "ffmpeg_processor.h"
#include "ffmpeg_filter.h"

int main(int argc, char* argv[]) {
    /*if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file>" << std::endl;
        std::cin.get();
        return -1;
    }*/

    try {
        // Set up logging (optional)
        FFmpegLogger::get_instance().set_log_file("ffmpeg_processing.log");

        // Initialize processor with input file
        FFmpegProcessor processor("../../../resources/input.mp4");

        // Initialize with output file, filter name, and filter arguments
        processor.initialize(
            "../../../resources/output.mp4",  // output filename
            "boxblur",                        // filter name
            FFmpegFilter::get_blur_args(2.0f).c_str() // filter arguments
        );

        // Process the video
        processor.process_with_filter();

        std::cout << "Video processing completed successfully" << std::endl;
        return 0;
    }
    catch (const FFmpegException& e) {
        LOG_ERROR(e.what());
        std::cerr << "FFmpeg error: " << e.what() << std::endl;
        return 1;
    }
    catch (const std::exception& e) {
		LOG_ERROR(e.what());
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
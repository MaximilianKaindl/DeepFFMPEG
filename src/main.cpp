#include <iostream>
#include "ffmpegprocessor.h"

int main(int argc, char* argv[]) {
	/*if (argc < 2) {
		std::cerr << "Usage: " << argv[0] << " <input_file>" << std::endl;
		std::cin.get();
		return -1;
	}*/

	try {
		FFmpegProcessor processor("C:/Users/mkaindl/Downloads/small_bunny_1080p_60fps.mp4");
		processor.initialize();
		processor.process_frames(0);
		return 0;
	}
	catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return -1;
	}
}
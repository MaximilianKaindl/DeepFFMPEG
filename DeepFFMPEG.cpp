#include "DeepFFMPEG.h"
#include <iostream>

// Include necessary FFmpeg headers
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

using namespace std;

void print_ffmpeg_version() {
	cout << "FFmpeg version information:" << endl;
	cout << "libavcodec version: " << AV_STRINGIFY(LIBAVCODEC_VERSION) << endl;
	cout << "libavformat version: " << AV_STRINGIFY(LIBAVFORMAT_VERSION) << endl;
	cout << "libavutil version: " << AV_STRINGIFY(LIBAVUTIL_VERSION) << endl;
}

bool test_ffmpeg_functionality() {
	// Try to allocate an AVFormatContext (basic FFmpeg structure)
	AVFormatContext* format_context = avformat_alloc_context();
	if (!format_context) {
		cout << "Failed to allocate AVFormatContext" << endl;
		return false;
	}

	// Clean up
	avformat_free_context(format_context);

	// Test codec registration
	const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!codec) {
		cout << "Failed to find H264 encoder" << endl;
		return false;
	}

	return true;
}

int main() {
	cout << "Testing FFmpeg integration..." << endl;

	// Print version information
	print_ffmpeg_version();

	// Test basic functionality
	if (test_ffmpeg_functionality()) {
		cout << "FFmpeg test passed successfully!" << endl;
	}
	else {
		cout << "FFmpeg test failed!" << endl;
		return 1;
	}

	return 0;
}
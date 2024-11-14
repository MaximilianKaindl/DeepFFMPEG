// ffmpeg_filter.h
#ifndef FFMPEG_FILTER_H
#define FFMPEG_FILTER_H
#include <string>


extern "C" {
#include <libavfilter/avfilter.h>
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include <libavutil/pixfmt.h> 
}

struct FilterGraph {
	AVFilterGraph* graph;
	AVFilterContext* buffer_src_context;
	AVFilterContext* buffer_sink_context;

	FilterGraph();
	~FilterGraph();
};

class FFmpegFilter {
public:
	static std::string get_blur_args(float blur_strength);
	static void initialize_filter_graph(FilterGraph& filter_graph,
	                                    const char* filter_name,
	                                    const char* filter_args,
	                                    const AVCodecContext* codec_context,
	                                    const AVStream* stream);
	static std::string get_pix_fmt_name(AVPixelFormat fmt);

private:
	static std::string create_filter_args(const AVCodecContext* codec_context,
		const AVStream* stream);
	static void setup_filter_contexts(FilterGraph& filter_graph,
	                                  const char* filter_name,
	                                  const char* filter_args, const std::string& buffer_args);
};

#endif // FFMPEG_FILTER_H
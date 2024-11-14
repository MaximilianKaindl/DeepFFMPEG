// ffmpeg_filter.cpp
#include "ffmpeg_filter.h"
#include "ffmpeg_common.h"
#include "ffmpeg_logger.h"

extern "C" {
#include <libavutil/pixfmt.h> 
}


FilterGraph::FilterGraph()
	: graph(nullptr)
	, buffer_src_context(nullptr)
	, buffer_sink_context(nullptr) {}

FilterGraph::~FilterGraph() {
	if (graph) {
		avfilter_graph_free(&graph);
	}
}

std::string FFmpegFilter::get_blur_args(float blur_strength) {
	static char args[128];

	// boxblur filter format: luma_radius:luma_power[:chroma_radius:chroma_power[:alpha_radius:alpha_power]]
	const int written = snprintf(args, sizeof(args),
		"luma_radius=%f:luma_power=1:chroma_radius=%f:chroma_power=1",
		blur_strength, blur_strength);

	if (written < 0 || written >= static_cast<int>(sizeof(args))) {
		throw FFmpegException("Failed to format blur arguments");
	}
	auto res = std::string(args);
	LOG_DEBUG("Created blur args: " + res);
	return res;
}

void FFmpegFilter::initialize_filter_graph(FilterGraph& filter_graph,
	const char* filter_name,
	const char* filter_args,
	const AVCodecContext* codec_context,
	const AVStream* stream) {

	const AVFilter* buffer_src = avfilter_get_by_name("buffer");
	const AVFilter* buffer_sink = avfilter_get_by_name("buffersink");
	const AVFilter* filter = avfilter_get_by_name(filter_name);

	if (!buffer_src || !buffer_sink || !filter) {
		throw FFmpegException("Could not find necessary filters");
	}

	filter_graph.graph = avfilter_graph_alloc();
	if (!filter_graph.graph) {
		throw FFmpegException("Could not allocate filter graph");
	}

	std::string buffer_args = create_filter_args(codec_context, stream);

	setup_filter_contexts(filter_graph, filter_name, filter_args, buffer_args);
}
//TODO replace with avutil library function -> had problems with reference
std::string FFmpegFilter::get_pix_fmt_name(AVPixelFormat fmt) {
	switch (fmt) {
		// Standard YUV formats
	case AV_PIX_FMT_YUV420P: return "yuv420p";
	case AV_PIX_FMT_YUV422P: return "yuv422p";
	case AV_PIX_FMT_YUV444P: return "yuv444p";
	case AV_PIX_FMT_YUVJ420P: return "yuvj420p";
	case AV_PIX_FMT_YUVJ422P: return "yuvj422p";
	case AV_PIX_FMT_YUVJ444P: return "yuvj444p";
	case AV_PIX_FMT_YUYV422: return "yuyv422";
	case AV_PIX_FMT_UYVY422: return "uyvy422";
	case AV_PIX_FMT_YUV420P10LE: return "yuv420p10le";
	case AV_PIX_FMT_YUV422P10LE: return "yuv422p10le";
	case AV_PIX_FMT_YUV444P10LE: return "yuv444p10le";

		// RGB formats
	case AV_PIX_FMT_RGB24: return "rgb24";
	case AV_PIX_FMT_BGR24: return "bgr24";
	case AV_PIX_FMT_RGBA: return "rgba";
	case AV_PIX_FMT_BGRA: return "bgra";
	case AV_PIX_FMT_RGB48BE: return "rgb48be";
	case AV_PIX_FMT_RGB48LE: return "rgb48le";
	case AV_PIX_FMT_ARGB: return "argb";
	case AV_PIX_FMT_ABGR: return "abgr";

		// Grayscale formats
	case AV_PIX_FMT_GRAY8: return "gray";
	case AV_PIX_FMT_GRAY16BE: return "gray16be";
	case AV_PIX_FMT_GRAY16LE: return "gray16le";

		// Planar RGB formats
	case AV_PIX_FMT_GBRP: return "gbrp";
	case AV_PIX_FMT_GBRP10LE: return "gbrp10le";
	case AV_PIX_FMT_GBRP12LE: return "gbrp12le";

		// 10/12 bit YUV formats
	case AV_PIX_FMT_YUV420P12LE: return "yuv420p12le";
	case AV_PIX_FMT_YUV422P12LE: return "yuv422p12le";
	case AV_PIX_FMT_YUV444P12LE: return "yuv444p12le";

		// Common video formats
	case AV_PIX_FMT_NV12: return "nv12";
	case AV_PIX_FMT_NV21: return "nv21";
	case AV_PIX_FMT_P010LE: return "p010le";
	case AV_PIX_FMT_P016LE: return "p016le";

		// Default case
	default:
		LOG_WARNING("Unknown pixel format: " + std::to_string(static_cast<int>(fmt)));
		return "yuv420p";  // Return a common default format instead of numeric value
	}
}

std::string FFmpegFilter::create_filter_args(const AVCodecContext* codec_context,
	const AVStream* stream) {
	char args[512];

	snprintf(args, sizeof(args),
		"video_size=%dx%d:pix_fmt=%s:time_base=%d/%d:pixel_aspect=%d/%d",
		codec_context->width,
		codec_context->height,
		get_pix_fmt_name(codec_context->pix_fmt).c_str(),
		stream->time_base.num,
		stream->time_base.den,
		codec_context->sample_aspect_ratio.num,
		codec_context->sample_aspect_ratio.den);

	LOG_DEBUG("Created filter args: " + std::string(args));
	return std::string(args);
}

void FFmpegFilter::setup_filter_contexts(FilterGraph& filter_graph,
	const char* filter_name,
	const char* filter_args,
	const std::string& buffer_args) {  // Added buffer_args parameter

	const AVFilter* buffersrc = avfilter_get_by_name("buffer");
	const AVFilter* buffersink = avfilter_get_by_name("buffersink");
	const AVFilter* filter = avfilter_get_by_name(filter_name);

	if (!buffersrc || !buffersink || !filter) {
		throw FFmpegException("Could not find necessary filters");
	}

	// Create buffer source filter with proper format arguments
	check_ff_result(
		avfilter_graph_create_filter(&filter_graph.buffer_src_context,
			buffersrc, "in",
			buffer_args.c_str(), nullptr, filter_graph.graph),
		"Cannot create buffer source"
	);

	// Create buffer sink filter
	check_ff_result(
		avfilter_graph_create_filter(&filter_graph.buffer_sink_context,
			buffersink, "out",
			nullptr, nullptr, filter_graph.graph),
		"Cannot create buffer sink"
	);

	// Create main filter
	AVFilterContext* filter_context = nullptr;
	check_ff_result(
		avfilter_graph_create_filter(&filter_context,
			filter, "filter",
			filter_args, nullptr, filter_graph.graph),
		"Cannot create filter"
	);

	// Link the filters together
	check_ff_result(
		avfilter_link(filter_graph.buffer_src_context, 0, filter_context, 0),
		"Cannot link buffer source -> filter"
	);

	check_ff_result(
		avfilter_link(filter_context, 0, filter_graph.buffer_sink_context, 0),
		"Cannot link filter -> buffer sink"
	);

	// Configure the filter graph
	check_ff_result(
		avfilter_graph_config(filter_graph.graph, nullptr),
		"Cannot configure filter graph"
	);
}

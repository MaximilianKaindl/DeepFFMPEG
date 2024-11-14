#include "ffmpeg_io.h"
#include "ffmpeg_logger.h"

FFmpegInputContext::FFmpegInputContext(const std::string& filename)
	: filename_(filename)
	, codec_(nullptr)
	, codec_parameters_(nullptr)
	, video_stream_index_(-1) {
	LOG_INFO("Creating input context for file: " + filename);
}

void FFmpegInputContext::initialize() {
	setup_input_format_context();
	setup_video_stream();
	setup_codec_context();
	setup_frame_buffers();
}

void FFmpegInputContext::setup_input_format_context() {
	format_context.reset(avformat_alloc_context());
	if (!format_context) {
		throw FFmpegException("Could not allocate memory for Format Context");
	}

	AVFormatContext* format_context_raw = format_context.get();
	check_ff_result(
		avformat_open_input(&format_context_raw, filename_.c_str(), nullptr, nullptr),
		"Could not open input file"
	);

	check_ff_result(
		avformat_find_stream_info(format_context.get(), nullptr),
		"Could not get stream info"
	);
}

void FFmpegInputContext::setup_video_stream() {
	for (unsigned int i = 0; i < format_context->nb_streams; i++) {
		AVCodecParameters* local_codec_params = format_context->streams[i]->codecpar;
		const AVCodec* local_codec = avcodec_find_decoder(local_codec_params->codec_id);

		if (!local_codec) {
			LOG_WARNING("Unsupported codec for stream " + std::to_string(i));
			continue;
		}

		if (local_codec_params->codec_type == AVMEDIA_TYPE_VIDEO) {
			if (video_stream_index_ > -1) {
				throw FFmpegException("Multiple video streams found in the input file");
			}
			video_stream_index_ = i;
			codec_ = local_codec;
			codec_parameters_ = local_codec_params;

			LOG_INFO("Found video stream: " + std::to_string(i) +
				" (" + std::to_string(local_codec_params->width) + "x" +
				std::to_string(local_codec_params->height) + ")");
			return;
		}
	}

	throw FFmpegException("No video stream found in the input file");
}

void FFmpegInputContext::setup_codec_context() {
	codec_context.reset(avcodec_alloc_context3(codec_));
	if (!codec_context) {
		throw FFmpegException("Failed to allocate codec context");
	}

	check_ff_result(
		avcodec_parameters_to_context(codec_context.get(), codec_parameters_),
		"Failed to copy codec parameters"
	);

	check_ff_result(
		avcodec_open2(codec_context.get(), codec_, nullptr),
		"Failed to open codec"
	);
}

void FFmpegInputContext::setup_frame_buffers() {
	frame.reset(av_frame_alloc());
	if (!frame) {
		throw FFmpegException("Failed to allocate frame");
	}

	packet.reset(av_packet_alloc());
	if (!packet) {
		throw FFmpegException("Failed to allocate packet");
	}
}

AVStream* FFmpegInputContext::get_video_stream() const {
	return format_context->streams[video_stream_index_];
}

void FFmpegOutputContext::initialize(const std::string& filename,
	const FFmpegInputContext& input_context) {

	AVFormatContext* output_ctx = nullptr;
	check_ff_result(
		avformat_alloc_output_context2(&output_ctx, nullptr, nullptr, filename.c_str()),
		"Could not create output context"
	);
	format_context.reset(output_ctx);

	setup_stream(input_context);
	setup_codec(input_context);
	open_output_file(filename);
}

void FFmpegOutputContext::setup_stream(const FFmpegInputContext& input_context) {
	stream = avformat_new_stream(format_context.get(), input_context.get_codec());
	if (!stream) {
		throw FFmpegException("Failed to create output stream");
	}
}

void FFmpegOutputContext::setup_codec(const FFmpegInputContext& input_context) {
	codec_context.reset(avcodec_alloc_context3(input_context.get_codec()));
	if (!codec_context) {
		throw FFmpegException("Could not allocate output codec context");
	}

	avcodec_parameters_to_context(codec_context.get(), input_context.get_codec_parameters());
	codec_context->time_base = input_context.get_video_stream()->time_base;

	check_ff_result(
		avcodec_open2(codec_context.get(), input_context.get_codec(), nullptr),
		"Could not open output codec"
	);

	check_ff_result(
		avcodec_parameters_from_context(stream->codecpar, codec_context.get()),
		"Could not copy codec params"
	);
}

void FFmpegOutputContext::open_output_file(const std::string& filename) const {
	if (!(format_context->oformat->flags & AVFMT_NOFILE)) {
		check_ff_result(
			avio_open(&format_context->pb, filename.c_str(), AVIO_FLAG_WRITE),
			"Could not open output file"
		);
	}

	check_ff_result(
		avformat_write_header(format_context.get(), nullptr),
		"Error writing header"
	);
}

void FFmpegOutputContext::write_frame(AVPacket* packet) const {
	check_ff_result(
		av_interleaved_write_frame(format_context.get(), packet),
		"Error writing frame"
	);
}

void FFmpegOutputContext::finalize() const {
	av_write_trailer(format_context.get());
}

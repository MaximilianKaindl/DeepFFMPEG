// ffmpeg_processor.cpp
#include "ffmpeg_processor.h"
#include "ffmpeg_logger.h"
extern "C" {
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
}
FFmpegProcessor::FFmpegProcessor(std::string input_file)
	: filename(std::move(input_file)) {
	LOG_INFO("Creating FFmpegProcessor for file: " + filename);
	input_context = std::make_unique<FFmpegInputContext>(filename);
}

FFmpegProcessor::~FFmpegProcessor() = default;

void FFmpegProcessor::initialize(const std::string& output_filename,
	const char* filter_name,
	const char* filter_args) {
	LOG_INFO("Initializing FFmpeg processor for: " + filename);

	InitConfig config(output_filename, filter_name, filter_args);

	try {
		// Initialize input
		input_context->initialize();

		// Initialize filter if needed
		if (config.needs_filter) {
			filter_graph = std::make_unique<FilterGraph>();
			FFmpegFilter::initialize_filter_graph(*filter_graph,
				filter_name,
				filter_args,
				input_context->codec_context.get(),
				input_context->get_video_stream());
		}

		// Initialize output if needed
		if (config.needs_output) {
			output_context = std::make_unique<FFmpegOutputContext>();
			output_context->initialize(output_filename, *input_context);
		}

		LOG_INFO("Initialization completed successfully");
	}
	catch (const FFmpegException& e) {
		LOG_ERROR("Initialization failed: " + std::string(e.what()));
		throw;
	}
}

void FFmpegProcessor::process_with_filter() const {
	std::unique_ptr<AVFrame, FrameDeleter> filtered_frame(av_frame_alloc());
	if (!filtered_frame) {
		throw FFmpegException("Could not allocate filtered frame");
	}

	process_frames_through_filter(filtered_frame.get());

	// Flush encoder and finalize output
	if (output_context) {
		flush_encoder();
		output_context->finalize();
	}
}

void FFmpegProcessor::process_frames_through_filter(AVFrame* filtered_frame) const {
	while (read_input_packet()) {
		if (input_context->packet->stream_index != input_context->get_video_stream_index()) {
			continue;  // Skip non-video packets
		}

		decode_and_filter_packet(filtered_frame);
	}
}

bool FFmpegProcessor::read_input_packet() const {
	const int ret = av_read_frame(input_context->format_context.get(), input_context->packet.get());
	if (ret < 0 && ret != AVERROR_EOF) {
		// Only throw if it's an actual error, not end of file
		check_ff_result(ret, "Error reading input packet");
	}
	return ret >= 0;
}

void FFmpegProcessor::decode_and_filter_packet(AVFrame* filtered_frame) const {
	check_ff_result(
		avcodec_send_packet(input_context->codec_context.get(), input_context->packet.get()),
		"Error sending packet to decoder"
	);

	while (true) {
		const int ret = avcodec_receive_frame(input_context->codec_context.get(), input_context->frame.get());
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
			break;  // Need more packets or reached end
		}
		check_ff_result(ret, "Error receiving frame from decoder");

		filter_and_encode_frame(input_context->frame.get(), filtered_frame);
		av_frame_unref(input_context->frame.get());
	}
	av_packet_unref(input_context->packet.get());
}

void FFmpegProcessor::filter_and_encode_frame(AVFrame* input_frame, AVFrame* filtered_frame) const {
	// Push frame through filter graph
	check_ff_result(
		av_buffersrc_add_frame_flags(filter_graph->buffer_src_context, input_frame, 0),
		"Error feeding filter graph"
	);

	// Get all available filtered frames
	while (true) {
		const int ret = av_buffersink_get_frame(filter_graph->buffer_sink_context, filtered_frame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
			break;  // No more frames available right now
		}
		check_ff_result(ret, "Error getting filtered frame");


		encode_filtered_frame(filtered_frame);
		av_frame_unref(filtered_frame);
	}
}

void FFmpegProcessor::encode_filtered_frame(AVFrame* filtered_frame) const {
	// Set frame metadata
	filtered_frame->pict_type = AV_PICTURE_TYPE_NONE;
	filtered_frame->pts = input_context->frame->pts;

	// Send frame to encoder
	check_ff_result(
		avcodec_send_frame(output_context->codec_context.get(), filtered_frame),
		"Error sending frame to encoder"
	);

	// Verify the frame is writable (needed by some encoders)
	if (!av_frame_is_writable(filtered_frame)) {
		av_frame_make_writable(filtered_frame);
	}

	// Receive encoded packets
	while (true) {
		const int ret = avcodec_receive_packet(output_context->codec_context.get(), input_context->packet.get());
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
			break;  // Need more frames or done encoding
		}
		check_ff_result(ret, "Error receiving packet from encoder");

		// Prepare packet for output
		input_context->packet->stream_index = 0;
		av_packet_rescale_ts(input_context->packet.get(),
			output_context->codec_context->time_base,
			output_context->stream->time_base);

		// Write the packet
		output_context->write_frame(input_context->packet.get());
	}
}

void FFmpegProcessor::flush_encoder() const {
	// Send flush packet to encoder
	check_ff_result(
		avcodec_send_frame(output_context->codec_context.get(), nullptr),
		"Error sending flush frame to encoder"
	);

	while (true) {
		const int ret = avcodec_receive_packet(output_context->codec_context.get(), input_context->packet.get());
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
			break;  // No more packets to flush
		}
		check_ff_result(ret, "Error receiving packet during flush");

		// Prepare packet for output
		input_context->packet->stream_index = 0;
		av_packet_rescale_ts(input_context->packet.get(),
			output_context->codec_context->time_base,
			output_context->stream->time_base);

		// Write the packet
		output_context->write_frame(input_context->packet.get());
	}
}
// ffmpeg_processor.h
#ifndef FFMPEG_PROCESSOR_H
#define FFMPEG_PROCESSOR_H

#include <memory>
#include "ffmpeg_io.h"
#include "ffmpeg_filter.h"

class FFmpegProcessor {
public:
	explicit FFmpegProcessor(std::string input_file);
	~FFmpegProcessor();

	void initialize(const std::string& output_filename,
		const char* filter_name = nullptr,
		const char* filter_args = nullptr);
	void process_with_filter() const;

private:
	struct InitConfig {
		std::string output_filename;
		const char* filter_name;
		const char* filter_args;
		bool needs_filter;
		bool needs_output;

		InitConfig(const std::string& out_filename,
			const char* f_name,
			const char* f_args)
			: output_filename(out_filename)
			, filter_name(f_name)
			, filter_args(f_args)
			, needs_filter(f_name != nullptr)
			, needs_output(!out_filename.empty()) {}
	};

	void process_frames_through_filter(AVFrame* filtered_frame) const;
	bool read_input_packet() const;
	void decode_and_filter_packet(AVFrame* filtered_frame) const;
	void filter_and_encode_frame(AVFrame* input_frame, AVFrame* filtered_frame) const;
	void encode_filtered_frame(AVFrame* filtered_frame) const;
	void flush_encoder() const;

	std::string filename;
	std::unique_ptr<FFmpegInputContext> input_context;
	std::unique_ptr<FFmpegOutputContext> output_context;
	std::unique_ptr<FilterGraph> filter_graph;
};

#endif // FFMPEG_PROCESSOR_H
 
#include "libavcodec/avcodec.h"
#include "libavfilter/avfilter.h"
#include "libavutil/opt.h"

// Filter context that holds our filter's private data
typedef struct {
	const AVClass* class;
	float intensity;  // Filter parameter
} SimpleFilterContext;

// Filter options that can be set by user
static const AVOption simple_options[] = {
	{ "intensity", "Set the effect intensity", offsetof(SimpleFilterContext, intensity),
	  AV_OPT_TYPE_FLOAT, {.dbl = 1.0}, 0.0, 5.0, AV_OPT_FLAG_FILTERING_PARAM | AV_OPT_FLAG_VIDEO_PARAM },
	{ NULL }
};

// Filter class definition
static const AVClass simple_class = {
	.class_name = "simple",
	.item_name = av_default_item_name,
	.option = simple_options,
	.version = LIBAVUTIL_VERSION_INT,
};

// Initialize the filter
static av_cold int init(AVFilterContext* ctx) {
	SimpleFilterContext* s = (SimpleFilterContext*)ctx->priv;
	// Initialize any required resources
	return 0;
}

// Clean up the filter
static av_cold void uninit(AVFilterContext* ctx) {
	SimpleFilterContext* s = (SimpleFilterContext*)ctx->priv;
	// Free any allocated resources
}

// Process a video frame
static int filter_frame(AVFilterLink* inlink, AVFrame* in) {
	AVFilterContext* ctx = inlink->dst;
	AVFilterLink* outlink = ctx->outputs[0];
	SimpleFilterContext* s = (SimpleFilterContext*)ctx->priv;

	AVFrame* out = av_frame_alloc();
	if (!out) {
		av_frame_free(&in);
		return AVERROR(ENOMEM);
	}

	// Copy input frame properties to output frame
	av_frame_copy_props(out, in);

	// Allocate output frame buffers
	int ret = av_frame_get_buffer(out, 0);
	if (ret < 0) {
		av_frame_free(&out);
		av_frame_free(&in);
		return ret;
	}

	// Process frame data
	// This example just modifies pixel values based on intensity
	for (int y = 0; y < in->height; y++) {
		for (int x = 0; x < in->width; x++) {
			for (int c = 0; c < 3; c++) {  // Assuming RGB/YUV format
				int pos = y * in->linesize[0] + x * 3 + c;
				float value = in->data[0][pos] * s->intensity;
				out->data[0][pos] = av_clip_uint8(value);
			}
		}
	}

	av_frame_free(&in);
	return ff_filter_frame(outlink, out);
}

// Define input/output pad requirements
static const AVFilterPad simple_inputs[] = {
	{
		.name = "default",
		.type = AVMEDIA_TYPE_VIDEO,
		.filter_frame = filter_frame,
	},
};

static const AVFilterPad simple_outputs[] = {
	{
		.name = "default",
		.type = AVMEDIA_TYPE_VIDEO,
	},
};

// Filter definition
const AVFilter ff_vf_simple = {
	.name = "simple",
	.description = NULL_IF_CONFIG_SMALL("A simple video filter demo."),
	.priv_size = sizeof(SimpleFilterContext),
	.priv_class = &simple_class,
	.init = init,
	.uninit = uninit,
	.inputs = simple_inputs,
	.outputs = simple_outputs,
	.flags = AVFILTER_FLAG_SUPPORT_TIMELINE_GENERIC,
};
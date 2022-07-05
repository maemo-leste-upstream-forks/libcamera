/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * Copyright (C) 2020, Collabora Ltd.
 *     Author: Nicolas Dufresne <nicolas.dufresne@collabora.com>
 *
 * gstlibcamera-utils.c - GStreamer libcamera Utility Function
 */

#include "gstlibcamera-utils.h"

#include <libcamera/formats.h>

using namespace libcamera;

static struct {
	GstVideoFormat gst_format;
	PixelFormat format;
} format_map[] = {
	/* Compressed */
	{ GST_VIDEO_FORMAT_ENCODED, formats::MJPEG },
	{ GST_VIDEO_FORMAT_ENCODED, formats::JPEG },

	/* RGB */
	{ GST_VIDEO_FORMAT_RGB, formats::BGR888 },
	{ GST_VIDEO_FORMAT_BGR, formats::RGB888 },
	{ GST_VIDEO_FORMAT_ARGB, formats::BGRA8888 },

	/* YUV Semiplanar */
	{ GST_VIDEO_FORMAT_NV12, formats::NV12 },
	{ GST_VIDEO_FORMAT_NV21, formats::NV21 },
	{ GST_VIDEO_FORMAT_NV16, formats::NV16 },
	{ GST_VIDEO_FORMAT_NV61, formats::NV61 },
	{ GST_VIDEO_FORMAT_NV24, formats::NV24 },

	/* YUV Planar */
	{ GST_VIDEO_FORMAT_I420, formats::YUV420 },
	{ GST_VIDEO_FORMAT_YV12, formats::YVU420 },
	{ GST_VIDEO_FORMAT_Y42B, formats::YUV422 },

	/* YUV Packed */
	{ GST_VIDEO_FORMAT_UYVY, formats::UYVY },
	{ GST_VIDEO_FORMAT_VYUY, formats::VYUY },
	{ GST_VIDEO_FORMAT_YUY2, formats::YUYV },
	{ GST_VIDEO_FORMAT_YVYU, formats::YVYU },

	/* \todo NV42 is used in libcamera but is not mapped in GStreamer yet. */
};

static const std::vector<std::pair<ColorSpace, std::string>> ColorSpaceTocolorimetry = {
	{ ColorSpace::Srgb, GST_VIDEO_COLORIMETRY_SRGB },
	{ ColorSpace::Rec709, GST_VIDEO_COLORIMETRY_BT709 },
	{ ColorSpace::Rec2020, GST_VIDEO_COLORIMETRY_BT2020 },
};

static const std::map<ColorSpace::Primaries, GstVideoColorPrimaries> ToGstVideoColorPrimaries = {
	{ ColorSpace::Primaries::Smpte170m, GST_VIDEO_COLOR_PRIMARIES_SMPTE170M },
	{ ColorSpace::Primaries::Rec709, GST_VIDEO_COLOR_PRIMARIES_BT709 },
	{ ColorSpace::Primaries::Rec2020, GST_VIDEO_COLOR_PRIMARIES_BT2020 },
};

static const std::map<ColorSpace::TransferFunction, GstVideoTransferFunction> ToGstVideoTransferFunction = {
	{ ColorSpace::TransferFunction::Srgb, GST_VIDEO_TRANSFER_SRGB },
	{ ColorSpace::TransferFunction::Rec709, GST_VIDEO_TRANSFER_BT709 },
};

static const std::map<ColorSpace::YcbcrEncoding, GstVideoColorMatrix> ToGstVideoColorMatrix = {
	{ ColorSpace::YcbcrEncoding::Rec601, GST_VIDEO_COLOR_MATRIX_BT601 },
	{ ColorSpace::YcbcrEncoding::Rec709, GST_VIDEO_COLOR_MATRIX_BT709 },
	{ ColorSpace::YcbcrEncoding::Rec2020, GST_VIDEO_COLOR_MATRIX_BT2020 },
};

static const std::map<ColorSpace::Range, GstVideoColorRange> ToGstVideoColorRange = {
	{ ColorSpace::Range::Full, GST_VIDEO_COLOR_RANGE_0_255 },
	{ ColorSpace::Range::Limited, GST_VIDEO_COLOR_RANGE_16_235 },
};

static const std::map<std::string, ColorSpace> colorimetryToColorSpace = {
	{ GST_VIDEO_COLORIMETRY_SRGB, ColorSpace::Srgb },
	{ GST_VIDEO_COLORIMETRY_BT709, ColorSpace::Rec709 },
	{ GST_VIDEO_COLORIMETRY_BT2020, ColorSpace::Rec2020 },
};

static GstVideoFormat
pixel_format_to_gst_format(const PixelFormat &format)
{
	for (const auto &item : format_map) {
		if (item.format == format)
			return item.gst_format;
	}
	return GST_VIDEO_FORMAT_UNKNOWN;
}

static PixelFormat
gst_format_to_pixel_format(GstVideoFormat gst_format)
{
	if (gst_format == GST_VIDEO_FORMAT_ENCODED)
		return PixelFormat{};

	for (const auto &item : format_map)
		if (item.gst_format == gst_format)
			return item.format;
	return PixelFormat{};
}

static GstStructure *
bare_structure_from_format(const PixelFormat &format)
{
	GstVideoFormat gst_format = pixel_format_to_gst_format(format);

	if (gst_format == GST_VIDEO_FORMAT_UNKNOWN)
		return nullptr;

	if (gst_format != GST_VIDEO_FORMAT_ENCODED)
		return gst_structure_new("video/x-raw", "format", G_TYPE_STRING,
					 gst_video_format_to_string(gst_format), nullptr);

	switch (format) {
	case formats::MJPEG:
	case formats::JPEG:
		return gst_structure_new_empty("image/jpeg");
	default:
		return nullptr;
	}
}

static const gchar *
colorimerty_from_colorspace(std::optional<ColorSpace> colorSpace)
{
	const gchar *colorimetry_str;
	GstVideoColorimetry colorimetry;

	auto iterColorimetry = std::find_if(ColorSpaceTocolorimetry.begin(), ColorSpaceTocolorimetry.end(),
				    [&colorSpace](const auto &item) {
						    return colorSpace == item.first;
					    });
	if (iterColorimetry != ColorSpaceTocolorimetry.end()) {
		colorimetry_str = (gchar *)iterColorimetry->second.c_str();
		return colorimetry_str;
	}

	auto iterPrimaries = ToGstVideoColorPrimaries.find(colorSpace->primaries);
	if (iterPrimaries != ToGstVideoColorPrimaries.end())
		colorimetry.primaries = iterPrimaries->second;
	else
		colorimetry.primaries = GST_VIDEO_COLOR_PRIMARIES_UNKNOWN;

	auto iterTransferFunction = ToGstVideoTransferFunction.find(colorSpace->transferFunction);
	if (iterTransferFunction != ToGstVideoTransferFunction.end())
		colorimetry.transfer = iterTransferFunction->second;
	else
		colorimetry.transfer = GST_VIDEO_TRANSFER_UNKNOWN;

	auto iterYcbcrEncoding = ToGstVideoColorMatrix.find(colorSpace->ycbcrEncoding);
	if (iterYcbcrEncoding != ToGstVideoColorMatrix.end())
		colorimetry.matrix = iterYcbcrEncoding->second;
	else
		colorimetry.matrix = GST_VIDEO_COLOR_MATRIX_UNKNOWN;

	auto iterRange = ToGstVideoColorRange.find(colorSpace->range);
	if (iterRange != ToGstVideoColorRange.end())
		colorimetry.range = iterRange->second;
	else
		colorimetry.range = GST_VIDEO_COLOR_RANGE_UNKNOWN;

	colorimetry_str = gst_video_colorimetry_to_string(&colorimetry);
	return colorimetry_str;
}

void colorspace_form_colorimetry(std::optional<ColorSpace> &colorspace, const gchar *colorimetry)
{
	auto iterColorSpace = colorimetryToColorSpace.find(colorimetry);
	if (iterColorSpace != colorimetryToColorSpace.end()) {
		colorspace = iterColorSpace->second;
		return;
	}
}

GstCaps *
gst_libcamera_stream_formats_to_caps(const StreamFormats &formats)
{
	GstCaps *caps = gst_caps_new_empty();

	for (PixelFormat pixelformat : formats.pixelformats()) {
		g_autoptr(GstStructure) bare_s = bare_structure_from_format(pixelformat);

		if (!bare_s) {
			GST_WARNING("Unsupported DRM format %" GST_FOURCC_FORMAT,
				    GST_FOURCC_ARGS(pixelformat));
			continue;
		}

		for (const Size &size : formats.sizes(pixelformat)) {
			GstStructure *s = gst_structure_copy(bare_s);
			gst_structure_set(s,
					  "width", G_TYPE_INT, size.width,
					  "height", G_TYPE_INT, size.height,
					  nullptr);
			gst_caps_append_structure(caps, s);
		}

		const SizeRange &range = formats.range(pixelformat);
		if (range.hStep && range.vStep) {
			GstStructure *s = gst_structure_copy(bare_s);
			GValue val = G_VALUE_INIT;

			g_value_init(&val, GST_TYPE_INT_RANGE);
			gst_value_set_int_range_step(&val, range.min.width, range.max.width, range.hStep);
			gst_structure_set_value(s, "width", &val);
			gst_value_set_int_range_step(&val, range.min.height, range.max.height, range.vStep);
			gst_structure_set_value(s, "height", &val);
			g_value_unset(&val);

			gst_caps_append_structure(caps, s);
		}
	}

	return caps;
}

GstCaps *
gst_libcamera_stream_configuration_to_caps(const StreamConfiguration &stream_cfg)
{
	GstCaps *caps = gst_caps_new_empty();
	GstStructure *s = bare_structure_from_format(stream_cfg.pixelFormat);
	const gchar *colorimetry;
	std::optional<ColorSpace> colorspace = stream_cfg.colorSpace;
	if (colorspace)
		colorimetry = colorimerty_from_colorspace(colorspace);
	else
		colorimetry = g_strdup("Unset");

	gst_structure_set(s,
			  "width", G_TYPE_INT, stream_cfg.size.width,
			  "height", G_TYPE_INT, stream_cfg.size.height,
			  "colorimetry", G_TYPE_STRING, colorimetry,
			  nullptr);
	gst_caps_append_structure(caps, s);

	return caps;
}

void
gst_libcamera_configure_stream_from_caps(StreamConfiguration &stream_cfg,
					 GstCaps *caps)
{
	GstVideoFormat gst_format = pixel_format_to_gst_format(stream_cfg.pixelFormat);
	guint i;
	gint best_fixed = -1, best_in_range = -1;
	GstStructure *s;

	/*
	 * These are delta weight computed from:
	 *   ABS(width - stream_cfg.size.width) * ABS(height - stream_cfg.size.height)
	 */
	guint best_fixed_delta = G_MAXUINT;
	guint best_in_range_delta = G_MAXUINT;

	/* First fixate the caps using default configuration value. */
	g_assert(gst_caps_is_writable(caps));

	/* Lookup the structure for a close match to the stream_cfg.size */
	for (i = 0; i < gst_caps_get_size(caps); i++) {
		s = gst_caps_get_structure(caps, i);
		gint width, height;
		guint delta;

		if (gst_structure_has_field_typed(s, "width", G_TYPE_INT) &&
		    gst_structure_has_field_typed(s, "height", G_TYPE_INT)) {
			gst_structure_get_int(s, "width", &width);
			gst_structure_get_int(s, "height", &height);

			delta = ABS(width - (gint)stream_cfg.size.width) * ABS(height - (gint)stream_cfg.size.height);

			if (delta < best_fixed_delta) {
				best_fixed_delta = delta;
				best_fixed = i;
			}
		} else {
			gst_structure_fixate_field_nearest_int(s, "width", stream_cfg.size.width);
			gst_structure_fixate_field_nearest_int(s, "height", stream_cfg.size.height);
			gst_structure_get_int(s, "width", &width);
			gst_structure_get_int(s, "height", &height);

			delta = ABS(width - (gint)stream_cfg.size.width) * ABS(height - (gint)stream_cfg.size.height);

			if (delta < best_in_range_delta) {
				best_in_range_delta = delta;
				best_in_range = i;
			}
		}
	}

	/* Prefer reliable fixed value over ranges */
	if (best_fixed >= 0)
		s = gst_caps_get_structure(caps, best_fixed);
	else
		s = gst_caps_get_structure(caps, best_in_range);

	if (gst_structure_has_name(s, "video/x-raw")) {
		const gchar *format = gst_video_format_to_string(gst_format);
		gst_structure_fixate_field_string(s, "format", format);
	}

	/* Then configure the stream with the result. */
	if (gst_structure_has_name(s, "video/x-raw")) {
		const gchar *format = gst_structure_get_string(s, "format");
		gst_format = gst_video_format_from_string(format);
		stream_cfg.pixelFormat = gst_format_to_pixel_format(gst_format);
	} else if (gst_structure_has_name(s, "image/jpeg")) {
		stream_cfg.pixelFormat = formats::MJPEG;
	} else {
		g_critical("Unsupported media type: %s", gst_structure_get_name(s));
	}

	gint width, height;
	gst_structure_get_int(s, "width", &width);
	gst_structure_get_int(s, "height", &height);
	stream_cfg.size.width = width;
	stream_cfg.size.height = height;
}

void
gst_libcamera_resume_task(GstTask *task)
{
	/* We only want to resume the task if it's paused. */
	GLibLocker lock(GST_OBJECT(task));
	if (GST_TASK_STATE(task) == GST_TASK_PAUSED) {
		GST_TASK_STATE(task) = GST_TASK_STARTED;
		GST_TASK_SIGNAL(task);
	}
}

G_LOCK_DEFINE_STATIC(cm_singleton_lock);
static std::weak_ptr<CameraManager> cm_singleton_ptr;

std::shared_ptr<CameraManager>
gst_libcamera_get_camera_manager(int &ret)
{
	std::shared_ptr<CameraManager> cm;

	G_LOCK(cm_singleton_lock);

	cm = cm_singleton_ptr.lock();
	if (!cm) {
		cm = std::make_shared<CameraManager>();
		cm_singleton_ptr = cm;
		ret = cm->start();
	} else {
		ret = 0;
	}

	G_UNLOCK(cm_singleton_lock);

	return cm;
}

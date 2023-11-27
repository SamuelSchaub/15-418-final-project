#include "framebuffer.h"
#include "../util/hdr_image.h"
#include "sample_pattern.h"

Framebuffer::Framebuffer(uint32_t width_, uint32_t height_, SamplePattern const& sample_pattern_)
	: width(width_), height(height_), sample_pattern(sample_pattern_) {

	// check that framebuffer isn't larger than allowed:
	if (width > MaxWidth || height > MaxHeight) {
		throw std::runtime_error("Framebuffer size (" + std::to_string(width) + "x" +
		                         std::to_string(height) + ") exceeds maximum allowed (" +
		                         std::to_string(MaxWidth) + "x" + std::to_string(MaxHeight) + ").");
	}
	// check that framebuffer size is even:
	if (width % 2 != 0 || height % 2 != 0) {
		throw std::runtime_error("Framebuffer size (" + std::to_string(width) + "x" +
		                         std::to_string(height) + ") is not even.");
	}

	uint32_t samples =
		width * height * static_cast<uint32_t>(sample_pattern.centers_and_weights.size());

	// allocate storage for color and depth samples:
	colors.assign(samples, Spectrum{0.0f, 0.0f, 0.0f});
	depths.assign(samples, 1.0f);
}

HDR_Image Framebuffer::resolve_colors() const {
	// A1T7: resolve_colors
	// TODO: update to support sample patterns with more than one sample.

	HDR_Image image(width, height);

	for (uint32_t i = 0; i < height; i++) {
		for (uint32_t j = 0; j < width; j++) {
			Spectrum sum = Spectrum();
			float weight_sum = 0.f;
			for (uint32_t s = 0; s < sample_pattern.centers_and_weights.size(); s++) {
				float weight = sample_pattern.centers_and_weights[s].z;
				sum += weight * color_at(j, i, s);
				weight_sum += weight;
			}
			image.at(j, i) = sum / weight_sum;
		}
	}

	return image;
}

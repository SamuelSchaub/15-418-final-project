
#include "texture.h"

#include <iostream>

namespace Textures {


Spectrum sample_nearest(HDR_Image const &image, Vec2 uv) {
	//clamp texture coordinates, convert to [0,w]x[0,h] pixel space:
	float x = image.w * std::clamp(uv.x, 0.0f, 1.0f);
	float y = image.h * std::clamp(uv.y, 0.0f, 1.0f);

	//the pixel with the nearest center is the pixel that contains (x,y):
	int32_t ix = int32_t(std::floor(x));
	int32_t iy = int32_t(std::floor(y));

	//texture coordinates of (1,1) map to (w,h), and need to be reduced:
	ix = std::min(ix, int32_t(image.w) - 1);
	iy = std::min(iy, int32_t(image.h) - 1);

	return image.at(ix, iy);
}

Spectrum sample_bilinear(HDR_Image const &image, Vec2 uv) {
	// A1T6: sample_bilinear
	//TODO: implement bilinear sampling strategy on texture 'image'

    float x = image.w * std::clamp(uv.x, 0.0f, 1.0f);
    float y = image.h * std::clamp(uv.y, 0.0f, 1.0f);

    float xPrime = floor(x - 0.5f);
    float yPrime = floor(y - 0.5f);

    float deltaX = x - xPrime - 0.5f;
    float deltaY = y - yPrime - 0.5f;

	if (xPrime + 1 == image.w && yPrime + 1 == image.h) {
		return image.at(xPrime, yPrime);
	} else if (xPrime + 1 == image.w) {
		Spectrum topLeft = image.at(xPrime, yPrime);
		Spectrum bottomLeft = image.at(xPrime, yPrime + 1);
		return (1 - deltaY) * topLeft + deltaY * bottomLeft;
	} else if (yPrime + 1 == image.h) {
		Spectrum topLeft = image.at(xPrime, yPrime);
		Spectrum topRight = image.at(xPrime + 1, yPrime);
		return (1 - deltaX) * topLeft + deltaX * topRight;
	}
	
    Spectrum topLeft = image.at(xPrime, yPrime);
    Spectrum topRight = image.at(xPrime + 1, yPrime);
    Spectrum bottomLeft = image.at(xPrime, yPrime + 1);
    Spectrum bottomRight = image.at(xPrime + 1, yPrime + 1);

	Spectrum tX = (1 - deltaX) * topLeft + deltaX * topRight;
	Spectrum tY = (1 - deltaX) * bottomLeft + deltaX * bottomRight;

	return (1 - deltaY) * tX + deltaY * tY;
}


Spectrum sample_trilinear(HDR_Image const &base, std::vector< HDR_Image > const &levels, Vec2 uv, float lod) {
	// A1T6: sample_trilinear
	//TODO: implement trilinear sampling strategy on using mip-map 'levels'

	// case 1) lod is smaller or equal to 0 => bilinear on base
	// case 2) greater or equal to size of level => bilinear on smallest level
	// case 3) between 0 and 1 => sample on base and level 0
	// case 4) between 1 and size - 1 => normal

	if (lod <= 0.f) {
		return sample_bilinear(base, uv);
	} else if (lod >= levels.size()) {
	    return sample_bilinear((levels[levels.size() - 1]), uv);
	} else if (0.f < lod && lod < 1.f) {
        Spectrum tD = sample_bilinear(base, uv);
        Spectrum tD1 = sample_bilinear(levels[0], uv);
        return (1.f - lod) * tD + lod * tD1;
    }

	float d = lod - 1.f;
	float dPrime = floor(d);
	float deltaD = d - dPrime;

	Spectrum tD = sample_bilinear(levels[dPrime], uv);
	Spectrum tD1 = sample_bilinear(levels[dPrime + 1], uv);
	
	return (1 - deltaD) * tD + deltaD * tD1;
}

/*
 * generate_mipmap- generate mipmap levels from a base image.
 *  base: the base image
 *  levels: pointer to vector of levels to fill (must not be null)
 *
 * generates a stack of levels [1,n] of sizes w_i, h_i, where:
 *   w_i = max(1, floor(w_{i-1})/2)
 *   h_i = max(1, floor(h_{i-1})/2)
 *  with:
 *   w_0 = base.w
 *   h_0 = base.h
 *  and n is the smalles n such that w_n = h_n = 1
 *
 * each level should be calculated by downsampling a blurred version
 * of the previous level to remove high-frequency detail.
 *
 */
void generate_mipmap(HDR_Image const &base, std::vector< HDR_Image > *levels_) {
	assert(levels_);
	auto &levels = *levels_;


	{ // allocate sublevels sufficient to scale base image all the way to 1x1:
		int32_t num_levels = static_cast<int32_t>(std::log2(std::max(base.w, base.h)));
		assert(num_levels >= 0);

		levels.clear();
		levels.reserve(num_levels);

		uint32_t width = base.w;
		uint32_t height = base.h;
		for (int32_t i = 0; i < num_levels; ++i) {
			assert(!(width == 1 && height == 1)); //would have stopped before this if num_levels was computed correctly

			width = std::max(1u, width / 2u);
			height = std::max(1u, height / 2u);

			levels.emplace_back(width, height);
		}
		assert(width == 1 && height == 1);
		assert(levels.size() == uint32_t(num_levels));
	}

	//now fill in the levels using a helper:
	//downsample:
	// fill in dst to represent the low-frequency component of src
	auto downsample = [](HDR_Image const &src, HDR_Image &dst) {
		//dst is half the size of src in each dimension:
		assert(std::max(1u, src.w / 2u) == dst.w);
		assert(std::max(1u, src.h / 2u) == dst.h);

		// A1T6: generate
		//TODO: Write code to fill the levels of the mipmap hierarchy by downsampling

		//Be aware that the alignment of the samples in dst and src will be different depending on whether the image is even or odd.
		for (uint32_t i = 0; i < dst.w; i++) {
			for (uint32_t j = 0; j < dst.h; j++) {
				if (src.w % 2 == 0 && src.h % 2 == 0) {
					Spectrum topLeft = src.at(2 * i, 2 * j);
					Spectrum topRight = src.at(2 * i + 1, 2 * j);
					Spectrum bottomLeft = src.at(2 * i, 2 * j + 1);
					Spectrum bottomRight = src.at(2 * i + 1, 2 * j + 1);

					dst.at(i, j) = (topLeft + topRight + bottomLeft + bottomRight) / 4.f;
				} else if (src.w % 2 == 0) {
					Spectrum topLeft = src.at(2 * i, 2 * j);
					Spectrum bottomLeft = src.at(2 * i, 2 * j + 1);

					dst.at(i, j) = (topLeft + bottomLeft) / 2.f;
				} else if (src.h % 2 == 0) {
					Spectrum topLeft = src.at(2 * i, 2 * j);
					Spectrum topRight = src.at(2 * i + 1, 2 * j);

					dst.at(i, j) = (topLeft + topRight) / 2.f;
				} else {
					dst.at(i, j) = src.at(2 * i, 2 * j);
				}
			}
		}
	};

	std::cout << "Regenerating mipmap (" << levels.size() << " levels): [" << base.w << "x" << base.h << "]";
	std::cout.flush();
	for (uint32_t i = 0; i < levels.size(); ++i) {
		HDR_Image const &src = (i == 0 ? base : levels[i-1]);
		HDR_Image &dst = levels[i];
		std::cout << " -> [" << dst.w << "x" << dst.h << "]"; std::cout.flush();

		downsample(src, dst);
	}
	std::cout << std::endl;
	
}

Image::Image(Sampler sampler_, HDR_Image const &image_) {
	sampler = sampler_;
	image = image_.copy();
	update_mipmap();
}

Spectrum Image::evaluate(Vec2 uv, float lod) const {
	if (sampler == Sampler::nearest) {
		return sample_nearest(image, uv);
	} else if (sampler == Sampler::bilinear) {
		return sample_bilinear(image, uv);
	} else {
		return sample_trilinear(image, levels, uv, lod);
	}
}

void Image::update_mipmap() {
	if (sampler == Sampler::trilinear) {
		generate_mipmap(image, &levels);
	} else {
		levels.clear();
	}
}

GL::Tex2D Image::to_gl() const {
	return image.to_gl(1.0f);
}

void Image::make_valid() {
	update_mipmap();
}

Spectrum Constant::evaluate(Vec2 uv, float lod) const {
	return color * scale;
}

} // namespace Textures
bool operator!=(const Textures::Constant& a, const Textures::Constant& b) {
	return a.color != b.color || a.scale != b.scale;
}

bool operator!=(const Textures::Image& a, const Textures::Image& b) {
	return a.image != b.image;
}

bool operator!=(const Texture& a, const Texture& b) {
	if (a.texture.index() != b.texture.index()) return false;
	return std::visit(
		[&](const auto& data) { return data != std::get<std::decay_t<decltype(data)>>(b.texture); },
		a.texture);
}

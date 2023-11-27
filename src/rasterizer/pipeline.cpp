// clang-format off
#include "pipeline.h"

#include <iostream>

#include "../lib/log.h"
#include "../lib/mathlib.h"
#include "framebuffer.h"
#include "sample_pattern.h"
template<PrimitiveType primitive_type, class Program, uint32_t flags>
void Pipeline<primitive_type, Program, flags>::run(std::vector<Vertex> const& vertices,
                                                   typename Program::Parameters const& parameters,
                                                   Framebuffer* framebuffer_) {
	// Framebuffer must be non-null:
	assert(framebuffer_);
	auto& framebuffer = *framebuffer_;

	// A1T7: sample loop
	// TODO: update this function to rasterize to *all* sample locations in the framebuffer.
	//  	 This will probably involve inserting a loop of the form:
	// 		 	std::vector< Vec3 > const &samples = framebuffer.sample_pattern.centers_and_weights;
	//      	for (uint32_t s = 0; s < samples.size(); ++s) { ... }
	//   	 around some subset of the code.
	// 		 You will also need to transform the input and output of the rasterize_* functions to
	// 	     account for the fact they deal with pixels centered at (0.5,0.5).

	std::vector<ShadedVertex> shaded_vertices;
	shaded_vertices.reserve(vertices.size());

	//--------------------------
	// shade vertices:
	for (auto const& v : vertices) {
		ShadedVertex sv;
		Program::shade_vertex(parameters, v.attributes, &sv.clip_position, &sv.attributes);
		shaded_vertices.emplace_back(sv);
	}

	//--------------------------
	// assemble + clip + homogeneous divide vertices:
	std::vector<ClippedVertex> clipped_vertices;

	// reserve some space to avoid reallocations later:
	if constexpr (primitive_type == PrimitiveType::Lines) {
		// clipping lines can never produce more than one vertex per input vertex:
		clipped_vertices.reserve(shaded_vertices.size());
	} else if constexpr (primitive_type == PrimitiveType::Triangles) {
		// clipping triangles can produce up to 8 vertices per input vertex:
		clipped_vertices.reserve(shaded_vertices.size() * 8);
	}
	// clang-format off

	//coefficients to map from clip coordinates to framebuffer (i.e., "viewport") coordinates:
	//x: [-1,1] -> [0,width]
	//y: [-1,1] -> [0,height]
	//z: [-1,1] -> [0,1] (OpenGL-style depth range)
	Vec3 const clip_to_fb_scale = Vec3{
		framebuffer.width / 2.0f,
		framebuffer.height / 2.0f,
		0.5f
	};
	Vec3 const clip_to_fb_offset = Vec3{
		0.5f * framebuffer.width,
		0.5f * framebuffer.height,
		0.5f
	};

	// helper used to put output of clipping functions into clipped_vertices:
	auto emit_vertex = [&](ShadedVertex const& sv) {
		ClippedVertex cv;
		float inv_w = 1.0f / sv.clip_position.w;
		cv.fb_position = clip_to_fb_scale * inv_w * sv.clip_position.xyz() + clip_to_fb_offset;
		cv.inv_w = inv_w;
		cv.attributes = sv.attributes;
		clipped_vertices.emplace_back(cv);
	};

	// actually do clipping:
	if constexpr (primitive_type == PrimitiveType::Lines) {
		for (uint32_t i = 0; i + 1 < shaded_vertices.size(); i += 2) {
			clip_line(shaded_vertices[i], shaded_vertices[i + 1], emit_vertex);
		}
	} else if constexpr (primitive_type == PrimitiveType::Triangles) {
		for (uint32_t i = 0; i + 2 < shaded_vertices.size(); i += 3) {
			clip_triangle(shaded_vertices[i], shaded_vertices[i + 1], shaded_vertices[i + 2], emit_vertex);
		}
	} else {
		static_assert(primitive_type == PrimitiveType::Lines, "Unsupported primitive type.");
	}

	//--------------------------
	// rasterize primitives:

	std::vector<Fragment> fragments;

	// helper used to put output of rasterization functions into fragments:
	auto emit_fragment = [&](Fragment const& f) { fragments.emplace_back(f); };

	std::vector<Vec3> const &samples = framebuffer.sample_pattern.centers_and_weights;
	for (uint32_t s = 0; s < samples.size(); s++) {
		// actually do rasterization:
		if constexpr (primitive_type == PrimitiveType::Lines) {
			for (uint32_t i = 0; i + 1 < clipped_vertices.size(); i += 2) {
				ClippedVertex sample1 = clipped_vertices[i];
				ClippedVertex sample2 = clipped_vertices[i + 1];
				sample1.fb_position.x = sample1.fb_position.x - 0.5f + samples[s].x;
				sample1.fb_position.y = sample1.fb_position.y - 0.5f + samples[s].y;
				sample2.fb_position.x = sample2.fb_position.x - 0.5f + samples[s].x;
				sample2.fb_position.y = sample2.fb_position.y - 0.5f + samples[s].y;
				rasterize_line(sample1, sample2, emit_fragment);
			}
		} else if constexpr (primitive_type == PrimitiveType::Triangles) {
			for (uint32_t i = 0; i + 2 < clipped_vertices.size(); i += 3) {
				ClippedVertex sample1 = clipped_vertices[i];
				ClippedVertex sample2 = clipped_vertices[i + 1];
				ClippedVertex sample3 = clipped_vertices[i + 2];
				sample1.fb_position.x = sample1.fb_position.x - 0.5f + samples[s].x;
				sample1.fb_position.y = sample1.fb_position.y - 0.5f + samples[s].y;
				sample2.fb_position.x = sample2.fb_position.x - 0.5f + samples[s].x;
				sample2.fb_position.y = sample2.fb_position.y - 0.5f + samples[s].y;
				sample3.fb_position.x = sample3.fb_position.x - 0.5f + samples[s].x;
				sample3.fb_position.y = sample3.fb_position.y - 0.5f + samples[s].y;
				rasterize_triangle(sample1, sample2, sample3, emit_fragment);
			}
		} else {
			static_assert(primitive_type == PrimitiveType::Lines, "Unsupported primitive type.");
		}

		//--------------------------
		// depth test + shade + blend fragments:
		uint32_t out_of_range = 0; // check if rasterization produced fragments outside framebuffer 
								// (indicates something is wrong with clipping)
		for (auto const& f : fragments) {

			// fragment location (in pixels):
			int32_t x = (int32_t)std::floor(f.fb_position.x);
			int32_t y = (int32_t)std::floor(f.fb_position.y);

			// if clipping is working properly, this condition shouldn't be needed;
			// however, it prevents crashes while you are working on your clipping functions,
			// so we suggest leaving it in place:
			if (x < 0 || (uint32_t)x >= framebuffer.width || 
				y < 0 || (uint32_t)y >= framebuffer.height) {
				++out_of_range;
				continue;
			}

			// local names that refer to destination sample in framebuffer:
			float& fb_depth = framebuffer.depth_at(x, y, s);
			Spectrum& fb_color = framebuffer.color_at(x, y, s);
			// float& fb_depth = framebuffer.depth_at(x, y, 0);
			// Spectrum& fb_color = framebuffer.color_at(x, y, 0);


			// depth test:
			if constexpr ((flags & PipelineMask_Depth) == Pipeline_Depth_Always) {
				// "Always" means the depth test always passes.
			} else if constexpr ((flags & PipelineMask_Depth) == Pipeline_Depth_Never) {
				// "Never" means the depth test never passes.
				continue; //discard this fragment
			} else if constexpr ((flags & PipelineMask_Depth) == Pipeline_Depth_Less) {
				// "Less" means the depth test passes when the new fragment has depth less than the stored depth.
				// A1T4: Depth_Less
				// TODO: implement depth test! We want to only emit fragments that have a depth less than the stored depth, hence "Depth_Less".
				if (f.fb_position.z >= fb_depth) {
					continue;
				}
			} else {
				static_assert((flags & PipelineMask_Depth) <= Pipeline_Depth_Always, "Unknown depth test flag.");
			}

			// if depth test passes, and depth writes aren't disabled, write depth to depth buffer:
			if constexpr (!(flags & Pipeline_DepthWriteDisableBit)) {
				fb_depth = f.fb_position.z;
			}

			// shade fragment:
			ShadedFragment sf;
			sf.fb_position = f.fb_position;
			Program::shade_fragment(parameters, f.attributes, f.derivatives, &sf.color, &sf.opacity);

			// write color to framebuffer if color writes aren't disabled:
			if constexpr (!(flags & Pipeline_ColorWriteDisableBit)) {
				// blend fragment:
				if constexpr ((flags & PipelineMask_Blend) == Pipeline_Blend_Replace) {
					fb_color = sf.color;
				} else if constexpr ((flags & PipelineMask_Blend) == Pipeline_Blend_Add) {
					// A1T4: Blend_Add
					// TODO: framebuffer color should have fragment color multiplied by fragment opacity added to it.
					fb_color += sf.color * sf.opacity;
				} else if constexpr ((flags & PipelineMask_Blend) == Pipeline_Blend_Over) {
					// A1T4: Blend_Over
					// TODO: set framebuffer color to the result of "over" blending (also called "alpha blending") the fragment color over the framebuffer color, using the fragment's opacity
					// 		 You may assume that the framebuffer color has its alpha premultiplied already, and you just want to compute the resulting composite color
					fb_color = sf.color + fb_color * (1 - sf.opacity);
				} else {
					static_assert((flags & PipelineMask_Blend) <= Pipeline_Blend_Over, "Unknown blending flag.");
				}
			}
		}
		if (out_of_range > 0) {
			if constexpr (primitive_type == PrimitiveType::Lines) {
				warn("Produced %d fragments outside framebuffer; this indicates something is likely "
					"wrong with the clip_line function.",
					out_of_range);
			} else if constexpr (primitive_type == PrimitiveType::Triangles) {
				warn("Produced %d fragments outside framebuffer; this indicates something is likely "
					"wrong with the clip_triangle function.",
					out_of_range);
			}
		}
	}
}

// -------------------------------------------------------------------------
// clipping functions

// helper to interpolate between vertices:
template<PrimitiveType p, class P, uint32_t F>
auto Pipeline<p, P, F>::lerp(ShadedVertex const& a, ShadedVertex const& b, float t) -> ShadedVertex {
	ShadedVertex ret;
	ret.clip_position = (b.clip_position - a.clip_position) * t + a.clip_position;
	for (uint32_t i = 0; i < ret.attributes.size(); ++i) {
		ret.attributes[i] = (b.attributes[i] - a.attributes[i]) * t + a.attributes[i];
	}
	return ret;
}

/*
 * clip_line - clip line to portion with -w <= x,y,z <= w, emit vertices of clipped line (if non-empty)
 *  	va, vb: endpoints of line
 *  	emit_vertex: call to produce truncated line
 *
 * If clipping shortens the line, attributes of the shortened line should respect the pipeline's interpolation mode.
 * 
 * If no portion of the line remains after clipping, emit_vertex will not be called.
 *
 * The clipped line should have the same direction as the full line.
 */
template<PrimitiveType p, class P, uint32_t flags>
void Pipeline<p, P, flags>::clip_line(ShadedVertex const& va, ShadedVertex const& vb,
                                      std::function<void(ShadedVertex const&)> const& emit_vertex) {
	// Determine portion of line over which:
	// 		pt = (b-a) * t + a
	//  	-pt.w <= pt.x <= pt.w
	//  	-pt.w <= pt.y <= pt.w
	//  	-pt.w <= pt.z <= pt.w
	// ... as a range [min_t, max_t]:

	float min_t = 0.0f;
	float max_t = 1.0f;

	// want to set range of t for a bunch of equations like:
	//    a.x + t * ba.x <= a.w + t * ba.w
	// so here's a helper:
	auto clip_range = [&min_t, &max_t](float l, float dl, float r, float dr) {
		// restrict range such that:
		// l + t * dl <= r + t * dr
		// re-arranging:
		//  l - r <= t * (dr - dl)
		if (dr == dl) {
			// want: l - r <= 0
			if (l - r > 0.0f) {
				// works for none of range, so make range empty:
				min_t = 1.0f;
				max_t = 0.0f;
			}
		} else if (dr > dl) {
			// since dr - dl is positive:
			// want: (l - r) / (dr - dl) <= t
			min_t = std::max(min_t, (l - r) / (dr - dl));
		} else { // dr < dl
			// since dr - dl is negative:
			// want: (l - r) / (dr - dl) >= t
			max_t = std::min(max_t, (l - r) / (dr - dl));
		}
	};

	// local names for clip positions and their difference:
	Vec4 const& a = va.clip_position;
	Vec4 const& b = vb.clip_position;
	Vec4 const ba = b - a;

	// -a.w - t * ba.w <= a.x + t * ba.x <= a.w + t * ba.w
	clip_range(-a.w, -ba.w, a.x, ba.x);
	clip_range(a.x, ba.x, a.w, ba.w);
	// -a.w - t * ba.w <= a.y + t * ba.y <= a.w + t * ba.w
	clip_range(-a.w, -ba.w, a.y, ba.y);
	clip_range(a.y, ba.y, a.w, ba.w);
	// -a.w - t * ba.w <= a.z + t * ba.z <= a.w + t * ba.w
	clip_range(-a.w, -ba.w, a.z, ba.z);
	clip_range(a.z, ba.z, a.w, ba.w);

	if (min_t < max_t) {
		if (min_t == 0.0f) {
			emit_vertex(va);
		} else {
			ShadedVertex out = lerp(va, vb, min_t);
			// don't interpolate attributes if in flat shading mode:
			if constexpr ((flags & PipelineMask_Interp) == Pipeline_Interp_Flat) {
				out.attributes = va.attributes;
			}
			emit_vertex(out);
		}
		if (max_t == 1.0f) {
			emit_vertex(vb);
		} else {
			ShadedVertex out = lerp(va, vb, max_t);
			// don't interpolate attributes if in flat shading mode:
			if constexpr ((flags & PipelineMask_Interp) == Pipeline_Interp_Flat) {
				out.attributes = va.attributes;
			}
			emit_vertex(out);
		}
	}
}

/*
 * clip_triangle - clip triangle to portion with -w <= x,y,z <= w, emit resulting shape as triangles (if non-empty)
 *  	va, vb, vc: vertices of triangle
 *  	emit_vertex: call to produce clipped triangles (three calls per triangle)
 *
 * If clipping truncates the triangle, attributes of the new vertices should respect the pipeline's interpolation mode.
 * 
 * If no portion of the triangle remains after clipping, emit_vertex will not be called.
 *
 * The clipped triangle(s) should have the same winding order as the full triangle.
 */
template<PrimitiveType p, class P, uint32_t flags>
void Pipeline<p, P, flags>::clip_triangle(
	ShadedVertex const& va, ShadedVertex const& vb, ShadedVertex const& vc,
	std::function<void(ShadedVertex const&)> const& emit_vertex) {
	// A1EC: clip_triangle
	// TODO: correct code!
	emit_vertex(va);
	emit_vertex(vb);
	emit_vertex(vc);
}

// -------------------------------------------------------------------------
// rasterization functions

/*
 * rasterize_line:
 * calls emit_fragment( frag ) for every pixel "covered" by the line (va.fb_position.xy, vb.fb_position.xy).
 *
 *    a pixel (x,y) is "covered" by the line if it exits the inscribed diamond:
 * 
 *        (x+0.5,y+1)
 *        /        \
 *    (x,y+0.5)  (x+1,y+0.5)
 *        \        /
 *         (x+0.5,y)
 *
 *    to avoid ambiguity, we consider diamonds to contain their left and bottom points
 *    but not their top and right points. 
 * 
 * 	  since 45 degree lines breaks this rule, our rule in general is to rasterize the line as if its
 *    endpoints va and vb were at va + (e, e^2) and vb + (e, e^2) where no smaller nonzero e produces 
 *    a different rasterization result. 
 *    We will not explicitly check for 45 degree lines along the diamond edges (this will be extra credit),
 *    but you should be able to handle 45 degree lines in every other case (such as starting from pixel centers)
 *
 * for each such diamond, pass Fragment frag to emit_fragment, with:
 *  - frag.fb_position.xy set to the center (x+0.5,y+0.5)
 *  - frag.fb_position.z interpolated linearly between va.fb_position.z and vb.fb_position.z
 *  - frag.attributes set to va.attributes (line will only be used in Interp_Flat mode)
 *  - frag.derivatives set to all (0,0)
 *
 * when interpolating the depth (z) for the fragments, you may use any depth the line takes within the pixel
 * (i.e., you don't need to interpolate to, say, the closest point to the pixel center)
 *
 * If you wish to work in fixed point, check framebuffer.h for useful information about the framebuffer's dimensions.
 */
template<PrimitiveType p, class P, uint32_t flags>
void Pipeline<p, P, flags>::rasterize_line(
	ClippedVertex const& va, ClippedVertex const& vb,
	std::function<void(Fragment const&)> const& emit_fragment) {
	if constexpr ((flags & PipelineMask_Interp) != Pipeline_Interp_Flat) {
		assert(0 && "rasterize_line should only be invoked in flat interpolation mode.");
	}
	// A1T2: rasterize_line

	// TODO: Check out the block comment above this function for more information on how to fill in
	// this function!
	// The OpenGL specification section 3.5 may also come in handy.

	{
		ClippedVertex& a = const_cast<ClippedVertex&>(va);
		ClippedVertex& b = const_cast<ClippedVertex&>(vb);

		float deltaX = abs(a.fb_position.x - b.fb_position.x);
		float deltaY = abs(b.fb_position.y - a.fb_position.y);
		int i;
		int j;
		if (deltaX > deltaY) {
			i = 0;
			j = 1;
		} else {
			i = 1;
			j = 0;
		}

		if (a.fb_position[i] > b.fb_position[i]) {
			std::swap(a, b);
		}
		
		float t1 = floor(a.fb_position[i]);
		float t2 = floor(b.fb_position[i]);

		float u = t1;
		float w;
        float v;
		while (u <= t2) {
			Fragment frag;
			w = ((u + 0.5f) - a.fb_position[i]) / (b.fb_position[i] - a.fb_position[i]);
			v = w * (b.fb_position[j] - a.fb_position[j]) + a.fb_position[j];
			frag.attributes = a.attributes;
			Vec2 pr = Vec2(u + 0.5f, v + 0.5f);
			Vec2 pa = Vec2(a.fb_position[i], a.fb_position[j]);
			Vec2 pb = Vec2(b.fb_position[i], b.fb_position[j]);
			float za = a.fb_position.z;
			float zb = b.fb_position.z;

			Vec2 diff = pb - pa;
			float t = dot(pr - pa, pb - pa) / (diff[i] * diff[i] + diff[j] * diff[j]);
			float z = (1 - t) * za + t * zb;

			if (deltaX > deltaY) {
				frag.fb_position = Vec3(floor(u) + 0.5f, floor(v) + 0.5f, z);
			} else {
				frag.fb_position = Vec3(floor(v) + 0.5f, floor(u) + 0.5f, z);
			}
			frag.derivatives.fill(Vec2(0.0f, 0.0f));

			if (u == t2) {	
				float distanceToCenterI = b.fb_position[i] - floor(u) - 0.5f;
				float distanceToCenterJ = b.fb_position[j] - floor(v) - 0.5f;
				float uFloored = floor(u);
				float vFloored = floor(v);
				Vec2 topDiamond = Vec2(uFloored + 0.5f, vFloored + 1.0f);
				Vec2 leftDiamond = Vec2(uFloored, vFloored + 0.5f);
				Vec2 rightDiamond = Vec2(uFloored + 1.0f, vFloored + 0.5f);
				Vec2 bottomDiamond = Vec2(uFloored + 0.5f, vFloored);
				Vec2 lineToA = Vec2(a.fb_position[i] - uFloored, a.fb_position[j] - v);
				Vec2 lineToLeft = Vec2(leftDiamond[i] - uFloored, leftDiamond[j] - v);	
				Vec2 lineToTop = Vec2(topDiamond[i] - uFloored, topDiamond[j] - v);
				Vec2 lineToRight = Vec2(rightDiamond[i] - uFloored, rightDiamond[j] - v);
				Vec2 lineToBottom = Vec2(bottomDiamond[i] - uFloored, bottomDiamond[j] - v);
				if (distanceToCenterI <= 0 && distanceToCenterJ <= 0) { // bottom left
					float bottomCross = cross(lineToBottom, lineToA);
					float leftCross = cross(lineToA, lineToLeft);
					if ((bottomCross > 0 && leftCross > 0) || (bottomCross < 0 && leftCross < 0)) {
						emit_fragment(frag);
					}
				} else if (distanceToCenterI <= 0 && distanceToCenterJ > 0) { // top left
					float leftCross = cross(lineToLeft, lineToA);
					float topCross = cross(lineToA, lineToTop);
					if ((leftCross > 0 && topCross > 0) || (leftCross < 0 && topCross < 0)) {
						emit_fragment(frag);
					}
				} else if (distanceToCenterI > 0 && distanceToCenterJ > 0) { // top right
					float topCross = cross(lineToTop, lineToA);
					float rightCross = cross(lineToA, lineToRight);
					if ((topCross > 0 && rightCross > 0) || (topCross < 0 && rightCross < 0)) {
						emit_fragment(frag);
					}	
				} else if (distanceToCenterI > 0 && distanceToCenterJ <= 0) { // bottom right
					float rightCross = cross(lineToRight, lineToA);
					float bottomCross = cross(lineToA, lineToBottom);
					if ((rightCross > 0 && bottomCross > 0) || (rightCross < 0 && bottomCross < 0)) {
						emit_fragment(frag);
					}
				}
			} else if (u == t1) {
				float distanceToCenterI = b.fb_position[i] - floor(u) - 0.5f;
				float distanceToCenterJ = b.fb_position[j] - floor(v) - 0.5f;
				float uFloored = floor(u);
				float vFloored = floor(v);
				Vec2 topDiamond = Vec2(uFloored + 0.5f, vFloored + 1.0f);
				Vec2 leftDiamond = Vec2(uFloored, vFloored + 0.5f);
				Vec2 rightDiamond = Vec2(uFloored + 1.0f, vFloored + 0.5f);
				Vec2 bottomDiamond = Vec2(uFloored + 0.5f, vFloored);
				Vec2 lineToB = Vec2(b.fb_position[i] - uFloored, b.fb_position[j] - v);
				Vec2 lineToLeft = Vec2(leftDiamond[i] - uFloored, leftDiamond[j] - v);	
				Vec2 lineToTop = Vec2(topDiamond[i] - uFloored, topDiamond[j]- v);
				Vec2 lineToRight = Vec2(rightDiamond[i] - uFloored, rightDiamond[j] - v);
				Vec2 lineToBottom = Vec2(bottomDiamond[i] - uFloored, bottomDiamond[j] - v);
				if (distanceToCenterI <= 0 && distanceToCenterJ <= 0) { // bottom left
					float bottomCross = cross(lineToBottom, lineToB);
					float leftCross = cross(lineToB, lineToLeft);
					if ((bottomCross > 0 && leftCross > 0) || (bottomCross < 0 && leftCross < 0)) {
						emit_fragment(frag);
					}
				} else if (distanceToCenterI <= 0 && distanceToCenterJ > 0) { // top left
					float leftCross = cross(lineToLeft, lineToB);
					float topCross = cross(lineToB, lineToTop);
					if ((leftCross > 0 && topCross > 0) || (leftCross < 0 && topCross < 0)) {
						emit_fragment(frag);
					}
				} else if (distanceToCenterI > 0 && distanceToCenterJ > 0) { // top right
					float topCross = cross(lineToTop, lineToB);
					float rightCross = cross(lineToB, lineToRight);
					if ((topCross > 0 && rightCross > 0) || (topCross < 0 && rightCross < 0)) {
						emit_fragment(frag);
					}	
				} else if (distanceToCenterI > 0 && distanceToCenterJ <= 0) { // bottom right
					float rightCross = cross(lineToRight, lineToB);
					float bottomCross = cross(lineToB, lineToBottom);
					if ((rightCross > 0 && bottomCross > 0) || (rightCross < 0 && bottomCross < 0)) {
						emit_fragment(frag);
					}
				} else {
					emit_fragment(frag);
				}
			} else {			
				emit_fragment(frag);
			}
			u += 1.0f;
		}
	}

}

/*
 * rasterize_triangle(a,b,c,emit) calls 'emit(frag)' at every location
 *  	(x+0.5,y+0.5) (where x,y are integers) covered by triangle (a,b,c).
 *
 * The emitted fragment should have:
 * - frag.fb_position.xy = (x+0.5, y+0.5)
 * - frag.fb_position.z = linearly interpolated fb_position.z from a,b,c (NOTE: does not depend on Interp mode!)
 * - frag.attributes = depends on Interp_* flag in flags:
 *   - if Interp_Flat: copy from va.attributes
 *   - if Interp_Smooth: interpolate as if (a,b,c) is a 2D triangle flat on the screen
 *   - if Interp_Correct: use perspective-correct interpolation
 * - frag.derivatives = derivatives w.r.t. fb_position.x and fb_position.y of the first frag.derivatives.size() attributes.
 *
 * Notes on derivatives:
 * 	The derivatives are partial derivatives w.r.t. screen locations. That is:
 *    derivatives[i].x = d/d(fb_position.x) attributes[i]
 *    derivatives[i].y = d/d(fb_position.y) attributes[i]
 *  You may compute these derivatives analytically or numerically.
 *
 *  See section 8.12.1 "Derivative Functions" of the GLSL 4.20 specification for some inspiration. (*HOWEVER*, the spec is solving a harder problem, and also nothing in the spec is binding on your implementation)
 *
 *  One approach is to rasterize blocks of four fragments and use forward and backward differences to compute derivatives.
 *  To assist you in this approach, keep in mind that the framebuffer size is *guaranteed* to be even. (see framebuffer.h)
 *
 * Notes on coverage:
 *  If two triangles are on opposite sides of the same edge, and a
 *  fragment center lies on that edge, rasterize_triangle should
 *  make sure that exactly one of the triangles emits that fragment.
 *  (Otherwise, speckles or cracks can appear in the final render.)
 * 
 *  For degenerate (co-linear) triangles, you may consider them to not be on any side of an edge.
 * 	Thus, even if two degnerate triangles share an edge that contains a fragment center, you don't need to emit it.
 *  You will not lose points for doing something reasonable when handling this case
 *
 *  This is pretty tricky to get exactly right!
 *
 */
template<PrimitiveType p, class P, uint32_t flags>
void Pipeline<p, P, flags>::rasterize_triangle(
	ClippedVertex const& va, ClippedVertex const& vb, ClippedVertex const& vc,
	std::function<void(Fragment const&)> const& emit_fragment) {
	// NOTE: it is okay to restructure this function to allow these tasks to use the
	//  same code paths. Be aware, however, that all of them need to remain working!
	//  (e.g., if you break Flat while implementing Correct, you won't get points
	//   for Flat.)
	if constexpr ((flags & PipelineMask_Interp) == Pipeline_Interp_Flat) {
		// A1T3: flat triangles
		// TODO: rasterize triangle (see block comment above this function).
		ClippedVertex a = const_cast<ClippedVertex&>(va);
		ClippedVertex b = const_cast<ClippedVertex&>(vb);
		ClippedVertex c = const_cast<ClippedVertex&>(vc);

		Vec2 ac = Vec2(c.fb_position.x - a.fb_position.x, c.fb_position.y - a.fb_position.y);
		Vec2 ab = Vec2(b.fb_position.x - a.fb_position.x, b.fb_position.y - a.fb_position.y);
		if (cross(ac, ab) > 0) {
			std::swap(b, c);
		}

		float xMin = std::min(a.fb_position.x, std::min(b.fb_position.x, c.fb_position.x));
		float yMin = std::min(a.fb_position.y, std::min(b.fb_position.y, c.fb_position.y));
		float xMax = std::max(a.fb_position.x, std::max(b.fb_position.x, c.fb_position.x));
		float yMax = std::max(a.fb_position.y, std::max(b.fb_position.y, c.fb_position.y));

		ac = Vec2(c.fb_position.x - a.fb_position.x, c.fb_position.y - a.fb_position.y);
		ab = Vec2(b.fb_position.x - a.fb_position.x, b.fb_position.y - a.fb_position.y);
		Vec2 cb = Vec2(b.fb_position.x - c.fb_position.x, b.fb_position.y - c.fb_position.y);
		Vec2 ca = Vec2(a.fb_position.x - c.fb_position.x, a.fb_position.y - c.fb_position.y);
		Vec2 ba = Vec2(a.fb_position.x - b.fb_position.x, a.fb_position.y - b.fb_position.y);
		Vec2 bc = Vec2(c.fb_position.x - b.fb_position.x, c.fb_position.y - b.fb_position.y);
		float area = 0.5 * std::abs(cross(ab, ac));

		if (area == 0) {
			return;
		}

		for (int i = std::floor(yMin); i <= std::ceil(yMax); i++) {
			for (int j = std::floor(xMin); j <= std::ceil(xMax); j++) {
				float x = j + 0.5f;
				float y = i + 0.5f;
				Fragment frag;
				frag.attributes = a.attributes;
				frag.derivatives.fill(Vec2(0.0f, 0.0f));
				
				Vec2 aq = Vec2(x - a.fb_position.x, y - a.fb_position.y);
				Vec2 cq = Vec2(x - c.fb_position.x, y - c.fb_position.y);
				Vec2 bq = Vec2(x - b.fb_position.x, y - b.fb_position.y);

				float barycentricC = (0.5 * std::abs(cross(ab, aq))) / area;	
				float barycentricB = (0.5 * std::abs(cross(ac, aq))) / area;
				float barycentricA = 1 - barycentricC - barycentricB;
				float z = barycentricA * a.fb_position.z + barycentricB * b.fb_position.z + barycentricC * c.fb_position.z;
				frag.fb_position = Vec3(x, y, z);

				float k = cross(ab, ac) * cross(ac, aq);
				float l = cross(ca, cb) * cross(cb, cq);
				float m = cross(bc, ba) * cross(ba, bq);

				if (k > 0 || l > 0 || m > 0) {
					continue;
				}

				if (k < 0 && l < 0 && m < 0) {
					emit_fragment(frag);
				} else if (k == 0 && ((a.fb_position.y == c.fb_position.y && b.fb_position.y < a.fb_position.y) || ac.y > 0)) {
					emit_fragment(frag);
				} else if (l == 0 && ((c.fb_position.y == b.fb_position.y && a.fb_position.y < c.fb_position.y) || cb.y > 0)) {
					emit_fragment(frag);
				} else if (m == 0 && ((b.fb_position.y == a.fb_position.y && c.fb_position.y < b.fb_position.y) || ba.y > 0)) {
					emit_fragment(frag);
				}
			}
		}
	} else if constexpr ((flags & PipelineMask_Interp) == Pipeline_Interp_Smooth) {
		// A1T5: screen-space smooth triangles
		// TODO: rasterize triangle (see block comment above this function).
		
		ClippedVertex a = const_cast<ClippedVertex&>(va);
		ClippedVertex b = const_cast<ClippedVertex&>(vb);
		ClippedVertex c = const_cast<ClippedVertex&>(vc);

		Vec2 ac = Vec2(c.fb_position.x - a.fb_position.x, c.fb_position.y - a.fb_position.y);
		Vec2 ab = Vec2(b.fb_position.x - a.fb_position.x, b.fb_position.y - a.fb_position.y);
		if (cross(ac, ab) > 0) {
			std::swap(b, c);
		}

		int xMin = std::min(a.fb_position.x, std::min(b.fb_position.x, c.fb_position.x));
		int yMin = std::min(a.fb_position.y, std::min(b.fb_position.y, c.fb_position.y));
		int xMax = std::max(a.fb_position.x, std::max(b.fb_position.x, c.fb_position.x));
		int yMax = std::max(a.fb_position.y, std::max(b.fb_position.y, c.fb_position.y));

		ac = Vec2(c.fb_position.x - a.fb_position.x, c.fb_position.y - a.fb_position.y);
		ab = Vec2(b.fb_position.x - a.fb_position.x, b.fb_position.y - a.fb_position.y);
		Vec2 cb = Vec2(b.fb_position.x - c.fb_position.x, b.fb_position.y - c.fb_position.y);
		Vec2 ca = Vec2(a.fb_position.x - c.fb_position.x, a.fb_position.y - c.fb_position.y);
		Vec2 ba = Vec2(a.fb_position.x - b.fb_position.x, a.fb_position.y - b.fb_position.y);
		Vec2 bc = Vec2(c.fb_position.x - b.fb_position.x, c.fb_position.y - b.fb_position.y);
		float area = 0.5 * std::abs(cross(ab, ac));

		if (area == 0) {
			return;
		}

		auto getBarycentric = [&](float x, float y) {
			Vec2 aq = Vec2(x - a.fb_position.x, y - a.fb_position.y);
			Vec2 cq = Vec2(x - c.fb_position.x, y - c.fb_position.y);
			Vec2 bq = Vec2(x - b.fb_position.x, y - b.fb_position.y);

			float barycentricC = (0.5 * cross(ab, aq)) / area;	
			float barycentricB = (0.5 * cross(ca, cq)) / area;
			float barycentricA = 1 - barycentricC - barycentricB;
			return std::make_tuple(barycentricA, barycentricB, barycentricC);
		};

		for (int i = yMin; i <= yMax; i++) {
			for (int j = xMin; j <= xMax; j++) {
				float x = j + 0.5f;
				float y = i + 0.5f;
				Fragment frag;
				
				Vec2 aq = Vec2(x - a.fb_position.x, y - a.fb_position.y);
				Vec2 cq = Vec2(x - c.fb_position.x, y - c.fb_position.y);
				Vec2 bq = Vec2(x - b.fb_position.x, y - b.fb_position.y);

				auto [barycentricA, barycentricB, barycentricC] = getBarycentric(x, y);

				float z = barycentricA * a.fb_position.z + barycentricB * b.fb_position.z + barycentricC * c.fb_position.z;
				frag.fb_position = Vec3(x, y, z);

				for (uint32_t k = 0; k < a.attributes.size(); k++) {
					frag.attributes[k] = barycentricA * a.attributes[k] + barycentricB * b.attributes[k] + barycentricC * c.attributes[k];
				}

				for (uint32_t k = 0; k < frag.derivatives.size(); k++) {
					auto [belowBA, belowBB, belowBC] = getBarycentric(x, y + 1);
					float attributeBelow = belowBA * a.attributes[k] + belowBB * b.attributes[k] + belowBC * c.attributes[k];
					frag.derivatives[k].y = attributeBelow - frag.attributes[k];

					auto [rightBA, rightBB, rightBC] = getBarycentric(x + 1, y);
					float attributeRight = rightBA * a.attributes[k] + rightBB * b.attributes[k] + rightBC * c.attributes[k];
					frag.derivatives[k].x = attributeRight - frag.attributes[k];

				}

				float k = cross(ab, ac) * cross(ac, aq);
				float l = cross(ca, cb) * cross(cb, cq);
				float m = cross(bc, ba) * cross(ba, bq);

				if (k > 0 || l > 0 || m > 0) {
					continue;
				}

				if (k < 0 && l < 0 && m < 0) {
					emit_fragment(frag);
				} else if (k == 0 && ((a.fb_position.y == c.fb_position.y && b.fb_position.y < a.fb_position.y) || ac.y > 0)) {
					emit_fragment(frag);
				} else if (l == 0 && ((c.fb_position.y == b.fb_position.y && a.fb_position.y < c.fb_position.y) || cb.y > 0)) {
					emit_fragment(frag);
				} else if (m == 0 && ((b.fb_position.y == a.fb_position.y && c.fb_position.y < b.fb_position.y) || ba.y > 0)) {
					emit_fragment(frag);
				}
			}
		}
	} else if constexpr ((flags & PipelineMask_Interp) == Pipeline_Interp_Correct) {
		// A1T5: perspective correct triangles
		// TODO: rasterize triangle (block comment above this function).

		ClippedVertex a = const_cast<ClippedVertex&>(va);
		ClippedVertex b = const_cast<ClippedVertex&>(vb);
		ClippedVertex c = const_cast<ClippedVertex&>(vc);

		Vec2 ac = Vec2(c.fb_position.x - a.fb_position.x, c.fb_position.y - a.fb_position.y);
		Vec2 ab = Vec2(b.fb_position.x - a.fb_position.x, b.fb_position.y - a.fb_position.y);
		if (cross(ac, ab) > 0) {
			std::swap(b, c);
		}

		int xMin = std::min(a.fb_position.x, std::min(b.fb_position.x, c.fb_position.x));
		int yMin = std::min(a.fb_position.y, std::min(b.fb_position.y, c.fb_position.y));
		int xMax = std::max(a.fb_position.x, std::max(b.fb_position.x, c.fb_position.x));
		int yMax = std::max(a.fb_position.y, std::max(b.fb_position.y, c.fb_position.y));

		ac = Vec2(c.fb_position.x - a.fb_position.x, c.fb_position.y - a.fb_position.y);
		ab = Vec2(b.fb_position.x - a.fb_position.x, b.fb_position.y - a.fb_position.y);
		Vec2 cb = Vec2(b.fb_position.x - c.fb_position.x, b.fb_position.y - c.fb_position.y);
		Vec2 ca = Vec2(a.fb_position.x - c.fb_position.x, a.fb_position.y - c.fb_position.y);
		Vec2 ba = Vec2(a.fb_position.x - b.fb_position.x, a.fb_position.y - b.fb_position.y);
		Vec2 bc = Vec2(c.fb_position.x - b.fb_position.x, c.fb_position.y - b.fb_position.y);
		float area = 0.5 * std::abs(cross(ab, ac));

		if (area == 0) {
			return;
		}

		auto getBarycentric = [&](float x, float y) {
			Vec2 aq = Vec2(x - a.fb_position.x, y - a.fb_position.y);
			Vec2 cq = Vec2(x - c.fb_position.x, y - c.fb_position.y);
			Vec2 bq = Vec2(x - b.fb_position.x, y - b.fb_position.y);

			float barycentricC = (0.5 * cross(ab, aq)) / area;	
			float barycentricB = (0.5 * cross(ca, cq)) / area;
			float barycentricA = 1 - barycentricC - barycentricB;
			return std::make_tuple(barycentricA, barycentricB, barycentricC);
		};

		for (int i = yMin; i <= yMax; i++) {
			for (int j = xMin; j <= xMax; j++) {
				float x = j + 0.5f;
				float y = i + 0.5f;
				Fragment frag;
				
				Vec2 aq = Vec2(x - a.fb_position.x, y - a.fb_position.y);
				Vec2 cq = Vec2(x - c.fb_position.x, y - c.fb_position.y);
				Vec2 bq = Vec2(x - b.fb_position.x, y - b.fb_position.y);

				auto [barycentricA, barycentricB, barycentricC] = getBarycentric(x, y);

				float z = barycentricA * a.fb_position.z + barycentricB * b.fb_position.z + barycentricC * c.fb_position.z;
				frag.fb_position = Vec3(x, y, z);

				float interpZ = barycentricA * a.inv_w + barycentricB * b.inv_w + barycentricC * c.inv_w;
				for (uint32_t k = 0; k < a.attributes.size(); k++) {
					frag.attributes[k] = (a.attributes[k] * a.inv_w * barycentricA + b.attributes[k] * b.inv_w * barycentricB + c.attributes[k] * c.inv_w * barycentricC) / interpZ;
				}

				for (uint32_t k = 0; k < frag.derivatives.size(); k++) {
					auto [belowBA, belowBB, belowBC] = getBarycentric(x, y + 1);
					float attributeBelow = (belowBA * a.attributes[k] * a.inv_w + belowBB * b.attributes[k] * b.inv_w + belowBC * c.attributes[k] * c.inv_w) / interpZ;
					frag.derivatives[k].y = attributeBelow - frag.attributes[k];

					auto [rightBA, rightBB, rightBC] = getBarycentric(x + 1, y);
					float attributeRight = (rightBA * a.attributes[k] * a.inv_w + rightBB * b.attributes[k] * b.inv_w + rightBC * c.attributes[k] * c.inv_w) / interpZ;
					frag.derivatives[k].x = attributeRight - frag.attributes[k];
				}

				float k = cross(ab, ac) * cross(ac, aq);
				float l = cross(ca, cb) * cross(cb, cq);
				float m = cross(bc, ba) * cross(ba, bq);

				if (k > 0 || l > 0 || m > 0) {
					continue;
				}

				if (k < 0 && l < 0 && m < 0) {
					emit_fragment(frag);
				} else if (k == 0 && ((a.fb_position.y == c.fb_position.y && b.fb_position.y < a.fb_position.y) || ac.y > 0)) {
					emit_fragment(frag);
				} else if (l == 0 && ((c.fb_position.y == b.fb_position.y && a.fb_position.y < c.fb_position.y) || cb.y > 0)) {
					emit_fragment(frag);
				} else if (m == 0 && ((b.fb_position.y == a.fb_position.y && c.fb_position.y < b.fb_position.y) || ba.y > 0)) {
					emit_fragment(frag);
				}
			}
		}
	}
}

//-------------------------------------------------------------------------
// compile instantiations for all programs and blending and testing types:

#include "programs.h"

template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Replace | Pipeline_Depth_Always | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Replace | Pipeline_Depth_Always | Pipeline_Interp_Smooth>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Replace | Pipeline_Depth_Always | Pipeline_Interp_Correct>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Replace | Pipeline_Depth_Never | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Replace | Pipeline_Depth_Never | Pipeline_Interp_Smooth>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Replace | Pipeline_Depth_Never | Pipeline_Interp_Correct>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Replace | Pipeline_Depth_Less | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Replace | Pipeline_Depth_Less | Pipeline_Interp_Smooth>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Replace | Pipeline_Depth_Less | Pipeline_Interp_Correct>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Add | Pipeline_Depth_Always | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Add | Pipeline_Depth_Always | Pipeline_Interp_Smooth>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Add | Pipeline_Depth_Always | Pipeline_Interp_Correct>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Add | Pipeline_Depth_Never | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Add | Pipeline_Depth_Never | Pipeline_Interp_Smooth>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Add | Pipeline_Depth_Never | Pipeline_Interp_Correct>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Add | Pipeline_Depth_Less | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Add | Pipeline_Depth_Less | Pipeline_Interp_Smooth>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Add | Pipeline_Depth_Less | Pipeline_Interp_Correct>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Over | Pipeline_Depth_Always | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Over | Pipeline_Depth_Always | Pipeline_Interp_Smooth>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Over | Pipeline_Depth_Always | Pipeline_Interp_Correct>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Over | Pipeline_Depth_Never | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Over | Pipeline_Depth_Never | Pipeline_Interp_Smooth>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Over | Pipeline_Depth_Never | Pipeline_Interp_Correct>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Over | Pipeline_Depth_Less | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Over | Pipeline_Depth_Less | Pipeline_Interp_Smooth>;
template struct Pipeline<PrimitiveType::Triangles, Programs::Lambertian,
                         Pipeline_Blend_Over | Pipeline_Depth_Less | Pipeline_Interp_Correct>;
template struct Pipeline<PrimitiveType::Lines, Programs::Lambertian,
                         Pipeline_Blend_Replace | Pipeline_Depth_Always | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Lines, Programs::Lambertian,
                         Pipeline_Blend_Replace | Pipeline_Depth_Never | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Lines, Programs::Lambertian,
                         Pipeline_Blend_Replace | Pipeline_Depth_Less | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Lines, Programs::Lambertian,
                         Pipeline_Blend_Add | Pipeline_Depth_Always | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Lines, Programs::Lambertian,
                         Pipeline_Blend_Add | Pipeline_Depth_Never | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Lines, Programs::Lambertian,
                         Pipeline_Blend_Add | Pipeline_Depth_Less | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Lines, Programs::Lambertian,
                         Pipeline_Blend_Over | Pipeline_Depth_Always | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Lines, Programs::Lambertian,
                         Pipeline_Blend_Over | Pipeline_Depth_Never | Pipeline_Interp_Flat>;
template struct Pipeline<PrimitiveType::Lines, Programs::Lambertian,
                         Pipeline_Blend_Over | Pipeline_Depth_Less | Pipeline_Interp_Flat>;
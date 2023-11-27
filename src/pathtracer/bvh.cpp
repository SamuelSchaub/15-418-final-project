
#include "bvh.h"
#include "aggregate.h"
#include "instance.h"
#include "tri_mesh.h"

#include <stack>
#include <limits>

namespace PT {

struct BVHBuildData {
	BVHBuildData(size_t start, size_t range, size_t dst) : start(start), range(range), node(dst) {
	}
	size_t start; ///< start index into the primitive array
	size_t range; ///< range of index into the primitive array
	size_t node;  ///< address to update
};

struct SAHBucketData {
	BBox bb;          ///< bbox of all primitives
	size_t num_prims; ///< number of primitives in the bucket
};

template<typename Primitive>
void BVH<Primitive>::build(std::vector<Primitive>&& prims, size_t max_leaf_size) {
	//A3T3 - build a bvh

	// Keep these
    nodes.clear();
    primitives = std::move(prims);

    // Construct a BVH from the given vector of primitives and maximum leaf
    // size configuration.

	//TODO

	root_idx = buildHelper(0, primitives.size(), max_leaf_size);
}

template<typename Primitive> Trace BVH<Primitive>::hit(const Ray& ray) const {
	//A3T3 - traverse your BVH

    // Implement ray - BVH intersection test. A ray intersects
    // with a BVH aggregate if and only if it intersects a primitive in
    // the BVH that is not an aggregate.

    // The starter code simply iterates through all the primitives.
    // Again, remember you can use hit() on any Primitive value.

	//TODO: replace this code with a more efficient traversal:
    Trace ret;
	ret.distance = FLT_MAX;

	hitHelper(ray, root_idx, ret);

    return ret;
}


template<typename Primitive>
void BVH<Primitive>::hitHelper(const Ray& ray, size_t nodeIndex, Trace& best) const {
	if (nodeIndex >= nodes.size()) {
		return;
	}

	auto node = nodes[nodeIndex];
	auto times = Vec2{FLT_MIN, FLT_MAX};
	auto hit = node.bbox.hit(ray, times);	

	if (!hit || times.x > best.distance) {
		return;
	}

	if (node.is_leaf()) {
		for (size_t i = node.start; i < node.start + node.size; i++) {
			auto trace = primitives[i].hit(ray);
			if (trace.hit && trace.distance < best.distance) {
				best = trace;
			}
		}
	} else {
		// hitHelper(ray, node.l, best);
		// hitHelper(ray, node.r, best);

		auto timesLeft = Vec2{FLT_MIN, FLT_MAX};
		auto left = nodes[node.l];

		auto timesRight = Vec2{FLT_MIN, FLT_MAX};
		auto right = nodes[node.r];

		auto hitLeft = left.bbox.hit(ray, timesLeft);
		auto hitRight = right.bbox.hit(ray, timesRight);

		if (hitLeft && hitRight) {
			auto first = timesLeft.x <= timesRight.x ? node.l : node.r;
			auto second = timesLeft.x <= timesRight.x ? node.r : node.l;
			auto secondTimes = timesLeft.x <= timesRight.x ? timesLeft : timesRight;

			hitHelper(ray, first, best);

			if (secondTimes.x <= best.distance) {
				hitHelper(ray, second, best);
			}

		} else if (hitLeft) {
			hitHelper(ray, node.l, best);
		} else if (hitRight) {
			hitHelper(ray, node.r, best);
		}
	}
}

template<typename Primitive>
BVH<Primitive>::BVH(std::vector<Primitive>&& prims, size_t max_leaf_size) {
	build(std::move(prims), max_leaf_size);
}

template<typename Primitive> std::vector<Primitive> BVH<Primitive>::destructure() {
	nodes.clear();
	return std::move(primitives);
}

template<typename Primitive>
template<typename P>
typename std::enable_if<std::is_copy_assignable_v<P>, BVH<P>>::type BVH<Primitive>::copy() const {
	BVH<Primitive> ret;
	ret.nodes = nodes;
	ret.primitives = primitives;
	ret.root_idx = root_idx;
	return ret;
}

template<typename Primitive> Vec3 BVH<Primitive>::sample(RNG &rng, Vec3 from) const {
	if (primitives.empty()) return {};
	int32_t n = rng.integer(0, static_cast<int32_t>(primitives.size()));
	return primitives[n].sample(rng, from);
}

template<typename Primitive>
float BVH<Primitive>::pdf(Ray ray, const Mat4& T, const Mat4& iT) const {
	if (primitives.empty()) return 0.0f;
	float ret = 0.0f;
	for (auto& prim : primitives) ret += prim.pdf(ray, T, iT);
	return ret / primitives.size();
}

template<typename Primitive> void BVH<Primitive>::clear() {
	nodes.clear();
	primitives.clear();
}

template<typename Primitive> bool BVH<Primitive>::Node::is_leaf() const {
	// A node is a leaf if l == r, since all interior nodes must have distinct children
	return l == r;
}

template<typename Primitive>
size_t BVH<Primitive>::new_node(BBox box, size_t start, size_t size, size_t l, size_t r) {
	Node n;
	n.bbox = box;
	n.start = start;
	n.size = size;
	n.l = l;
	n.r = r;
	nodes.push_back(n);
	return nodes.size() - 1;
}

template<typename Primitive>
BBox BVH<Primitive>::calculateBbox(size_t left, size_t right) {
	auto start = primitives.begin() + left;
	auto end = primitives.begin() + right;
	BBox box = {};

	for (auto it = start; it != end; it++) {
		box.enclose(it->bbox());
	}
	
	return box;
}

template<typename Primitive>
float BVH<Primitive>::calculateSAH(size_t left, size_t right, int axis, float split, float sc) {
	auto start = primitives.begin() + left;
	auto end = primitives.begin() + right;

	auto predicate = [&split, &axis](Primitive& prim) {
		return prim.bbox().center()[axis] < split;
	};

	auto middle = std::partition(start, end, predicate);
	size_t middleIndex = middle - primitives.begin();

	size_t nA = middleIndex - left;
	size_t nB = right - middleIndex;

	auto bboxA = calculateBbox(left, middleIndex);
	float sA = bboxA.surface_area() / sc;

	auto bboxB = calculateBbox(middleIndex, right);
	float sB = bboxB.surface_area() / sc;

	return sA * nA + sB * nB;
}

template<typename Primitive>
size_t BVH<Primitive>::buildHelper(size_t start, size_t end, size_t max_leaf_size) {
	size_t size = end - start;

	if (size <= max_leaf_size) {
		return new_node(calculateBbox(start, end), start, size, 0, 0);
	}

	const int numPartitions = 10;

	auto currentBbox = calculateBbox(start, end);
	auto surfaceArea = currentBbox.surface_area();

	auto axises = { 0, 1, 2 };

	float bestSAH = std::numeric_limits<float>::max();
	int bestAxis = 0;
	float bestSplit = 0.0f;

	for (auto axis : axises) {
		auto min = currentBbox.min[axis];
		auto max = currentBbox.max[axis];
		auto range = max - min;
		for (int i = 1; i <= numPartitions; i++) {
			auto split = min + range * ((float)i / (float)numPartitions);	

			float SAH = calculateSAH(start, end, axis, split, surfaceArea);

			if (SAH < bestSAH) {
				bestSAH = SAH;
				bestAxis = axis;
				bestSplit = split;
			}
		}
	}

	auto bestPredicate = [&](const Primitive& prim) {
		return prim.bbox().center()[bestAxis] < bestSplit;
	};

	auto middle = std::partition(primitives.begin() + start, primitives.begin() + end, bestPredicate);
	size_t middleIndex = middle - primitives.begin();

	size_t leftNode = buildHelper(start, middleIndex, max_leaf_size);
	size_t rightNode = buildHelper(middleIndex, end, max_leaf_size);

	return new_node(currentBbox, start, size, leftNode, rightNode);
}

template<typename Primitive> BBox BVH<Primitive>::bbox() const {
	if(nodes.empty()) return BBox{Vec3{0.0f}, Vec3{0.0f}};
	return nodes[root_idx].bbox;
}

template<typename Primitive> size_t BVH<Primitive>::n_primitives() const {
	return primitives.size();
}

template<typename Primitive>
uint32_t BVH<Primitive>::visualize(GL::Lines& lines, GL::Lines& active, uint32_t level,
                                   const Mat4& trans) const {

	std::stack<std::pair<size_t, uint32_t>> tstack;
	tstack.push({root_idx, 0u});
	uint32_t max_level = 0u;

	if (nodes.empty()) return max_level;

	while (!tstack.empty()) {

		auto [idx, lvl] = tstack.top();
		max_level = std::max(max_level, lvl);
		const Node& node = nodes[idx];
		tstack.pop();

		Spectrum color = lvl == level ? Spectrum(1.0f, 0.0f, 0.0f) : Spectrum(1.0f);
		GL::Lines& add = lvl == level ? active : lines;

		BBox box = node.bbox;
		box.transform(trans);
		Vec3 min = box.min, max = box.max;

		auto edge = [&](Vec3 a, Vec3 b) { add.add(a, b, color); };

		edge(min, Vec3{max.x, min.y, min.z});
		edge(min, Vec3{min.x, max.y, min.z});
		edge(min, Vec3{min.x, min.y, max.z});
		edge(max, Vec3{min.x, max.y, max.z});
		edge(max, Vec3{max.x, min.y, max.z});
		edge(max, Vec3{max.x, max.y, min.z});
		edge(Vec3{min.x, max.y, min.z}, Vec3{max.x, max.y, min.z});
		edge(Vec3{min.x, max.y, min.z}, Vec3{min.x, max.y, max.z});
		edge(Vec3{min.x, min.y, max.z}, Vec3{max.x, min.y, max.z});
		edge(Vec3{min.x, min.y, max.z}, Vec3{min.x, max.y, max.z});
		edge(Vec3{max.x, min.y, min.z}, Vec3{max.x, max.y, min.z});
		edge(Vec3{max.x, min.y, min.z}, Vec3{max.x, min.y, max.z});

		if (!node.is_leaf()) {
			tstack.push({node.l, lvl + 1});
			tstack.push({node.r, lvl + 1});
		} else {
			for (size_t i = node.start; i < node.start + node.size; i++) {
				uint32_t c = primitives[i].visualize(lines, active, level - lvl, trans);
				max_level = std::max(c + lvl, max_level);
			}
		}
	}
	return max_level;
}

template class BVH<Triangle>;
template class BVH<Instance>;
template class BVH<Aggregate>;
template BVH<Triangle> BVH<Triangle>::copy<Triangle>() const;

} // namespace PT

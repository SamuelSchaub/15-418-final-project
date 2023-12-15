#pragma once

#include "raytracer.h"
#include <algorithm>
#include <cfloat>
#include <chrono>
#include <iostream>
#include <limits>

ispc::float3 add(ispc::float3 a, ispc::float3 b) {
    ispc::float3 c;
    c.v[0] = a.v[0] + b.v[0];
    c.v[1] = a.v[1] + b.v[1];
    c.v[2] = a.v[2] + b.v[2];
    return c;
}

float intervalSize(ispc::interval i) { return i.max - i.min; }

ispc::interval expandInterval(ispc::interval i, float delta) {
    float padding = delta / 2.0f;
    return ispc::interval{i.min - padding, i.max + padding};
}

ispc::aabb createAABB(ispc::aabb a, ispc::aabb b) {
    ispc::aabb bbox;
    bbox.x = ispc::interval{std::min(a.x.min, b.x.min), std::max(a.x.max, b.x.max)};
    bbox.y = ispc::interval{std::min(a.y.min, b.y.min), std::max(a.y.max, b.y.max)};
    bbox.z = ispc::interval{std::min(a.z.min, b.z.min), std::max(a.z.max, b.z.max)};
    return bbox;
}

ispc::aabb createAABB(ispc::float3 a, ispc::float3 b) {
    ispc::aabb bbox;
    bbox.x = ispc::interval{std::min(a.v[0], b.v[0]), std::max(a.v[0], b.v[0])};
    bbox.y = ispc::interval{std::min(a.v[1], b.v[1]), std::max(a.v[1], b.v[1])};
    bbox.z = ispc::interval{std::min(a.v[2], b.v[2]), std::max(a.v[2], b.v[2])};
    return bbox;
}

ispc::aabb padAABB(ispc::aabb a) {
    float delta = 0.0001f;
    ispc::interval newX = (intervalSize(a.x) >= delta) ? a.x : expandInterval(a.x, delta);
    ispc::interval newY = (intervalSize(a.y) >= delta) ? a.y : expandInterval(a.y, delta);
    ispc::interval newZ = (intervalSize(a.z) >= delta) ? a.z : expandInterval(a.z, delta);
    return ispc::aabb{newX, newY, newZ};
}

ispc::aabb getAABB(const ispc::Hittable& object) {
    switch (object.type) {
    case ispc::HittableType::SPHERE: {
        ispc::Sphere* sphere = (ispc::Sphere*)object.object;
        return sphere->bbox;
    }
    case ispc::HittableType::NODE: {
        ispc::Node* node = (ispc::Node*)object.object;
        return node->bbox;
    }
    case ispc::HittableType::QUAD: {
        ispc::Quad* quad = (ispc::Quad*)object.object;
        return quad->bbox;
    }
    case ispc::HittableType::BVH: {
        ispc::Bvh* bvh = (ispc::Bvh*)object.object;
        return bvh->nodes[bvh->root].bbox;
    }
    }
}

ispc::interval getAxis(ispc::aabb bbox, int axis) {
    switch (axis) {
    case 0:
        return bbox.x;
    case 1:
        return bbox.y;
    case 2:
        return bbox.z;
    }

    return bbox.x;
}

size_t newNode(std::vector<ispc::Node>& nodes, ispc::aabb box, size_t start, size_t size, size_t left, size_t right) {
    ispc::Node node;
    node.bbox = box;
    node.start = start;
    node.size = size;
    node.left = left;
    node.right = right;
    nodes.push_back(node);
    return nodes.size() - 1;
}

ispc::aabb calculateBbox(std::vector<ispc::Hittable>& objects, size_t left, size_t right) {
    auto start = objects.begin() + left;
    auto end = objects.begin() + right;
    ispc::aabb box;
    box.x = ispc::interval{FLT_MAX, -FLT_MAX};
    box.y = ispc::interval{FLT_MAX, -FLT_MAX};
    box.z = ispc::interval{FLT_MAX, -FLT_MAX};
    for (auto it = start; it != end; it++) {
        ispc::Hittable hittable = *it;
        box = createAABB(box, getAABB(hittable));
    }

    return box;
}

ispc::float3 center(ispc::aabb box) {
    return ispc::float3{(box.x.min + box.x.max) / 2.0f, (box.y.min + box.y.max) / 2.0f, (box.z.min + box.z.max) / 2.0f};
}

float surfaceArea(ispc::aabb box) {
    auto x = intervalSize(box.x);
    auto y = intervalSize(box.y);
    auto z = intervalSize(box.z);
    return 2.0f * (x * y + y * z + z * x);
}

float calculateSAH(std::vector<ispc::Hittable>& objects, size_t left, size_t right, int axis, float split, float sc) {
    auto start = objects.begin() + left;
    auto end = objects.begin() + right;

    auto predicate = [&split, &axis](ispc::Hittable& obj) { return center(getAABB(obj)).v[axis] < split; };

    auto middle = std::partition(start, end, predicate);
    size_t middleIndex = middle - objects.begin();

    size_t nA = middleIndex - left;
    size_t nB = right - middleIndex;

    auto bboxA = calculateBbox(objects, left, middleIndex);
    float sA = surfaceArea(bboxA) / sc;

    auto bboxB = calculateBbox(objects, middleIndex, right);
    float sB = surfaceArea(bboxB) / sc;

    return sA * nA + sB * nB;
}

size_t constructBVH(std::vector<ispc::Hittable>& objects, std::vector<ispc::Node>& nodes, size_t start, size_t end, const uint32_t maxLeafSize) {
    size_t size = end - start;

    if (size <= maxLeafSize) {
        return newNode(nodes, calculateBbox(objects, start, end), start, size, 0, 0);
    }

    const int numPartitions = 10;

    auto currentBbox = calculateBbox(objects, start, end);
    float sa = surfaceArea(currentBbox);

    float bestSAH = std::numeric_limits<float>::max();
    int bestAxis = 0;
    float bestSplit = 0.0f;

    for (int axis = 0; axis < 3; axis++) {
        float min = getAxis(currentBbox, axis).min;
        float max = getAxis(currentBbox, axis).max;
        float range = max - min;
        for (int i = 1; i <= numPartitions; i++) {
            float split = min + range * ((float)i / (float)numPartitions);
            float SAH = calculateSAH(objects, start, end, axis, split, sa);

            if (SAH < bestSAH) {
                bestSAH = SAH;
                bestAxis = axis;
                bestSplit = split;
            }
        }
    }

    auto bestPredicate = [&bestSplit, &bestAxis](ispc::Hittable& obj) {
        return center(getAABB(obj)).v[bestAxis] < bestSplit;
    };

    auto middle = std::partition(objects.begin() + start, objects.begin() + end, bestPredicate);
    size_t middleIndex = middle - objects.begin();

    size_t left = constructBVH(objects, nodes, start, middleIndex, maxLeafSize);
    size_t right = constructBVH(objects, nodes, middleIndex, end, maxLeafSize);

    return newNode(nodes, currentBbox, start, size, left, right);
}

ispc::Bvh* createBVH(std::vector<ispc::Hittable>& objects, std::vector<ispc::Node>& nodes, const uint32_t maxLeafSize) {
    ispc::Bvh* bvh = new ispc::Bvh;
    bvh->objects = objects.data();
    bvh->numObjects = objects.size();
    bvh->root = constructBVH(objects, nodes, 0, objects.size(), maxLeafSize);
    bvh->nodes = nodes.data();
    bvh->numNodes = nodes.size();
    return bvh;
}

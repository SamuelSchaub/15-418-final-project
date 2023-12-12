#include "raytracer.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>

std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<> dist(0, 2);

ispc::float3 add(ispc::float3 a, ispc::float3 b) {
    ispc::float3 c;
    c.v[0] = a.v[0] + b.v[0];
    c.v[1] = a.v[1] + b.v[1];
    c.v[2] = a.v[2] + b.v[2];
    return c;
}

float intervalSize(ispc::interval& i) { return i.max - i.min; }

ispc::interval expandInterval(ispc::interval& i, float delta) {
    float padding = delta / 2.0f;
    return ispc::interval{i.min - padding, i.max + padding};
}

ispc::aabb createAABB(ispc::aabb& a, ispc::aabb& b) {
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

ispc::Sphere* createSphere(ispc::float3 center, float radius, ispc::Material* material) {
    ispc::Sphere* sphere = new ispc::Sphere;
    sphere->center = center;
    sphere->radius = radius;
    sphere->mat = *material;
    ispc::float3 rvec = ispc::float3{radius, radius, radius};
    ispc::float3 negativeRvec = ispc::float3{-radius, -radius, -radius};
    sphere->bbox = createAABB(add(center, negativeRvec), add(center, rvec));
    return sphere;
}

ispc::Quad* createQuad(ispc::float3 Q, ispc::float3 u, ispc::float3 v, ispc::Material* material) {
    ispc::Quad* quad = new ispc::Quad;
    quad->Q = Q;
    quad->u = u;
    quad->v = v;
    quad->mat = *material;
    quad->bbox = padAABB(createAABB(Q, add(add(Q, u), v)));
    ispc::initQuad(*quad);
    return quad;
}

void createHittable(ispc::HittableType type, void* object, std::vector<ispc::Hittable*>& objects) {
    ispc::Hittable* hittable = new ispc::Hittable;
    hittable->type = type;
    hittable->object = object;
    objects.emplace_back(hittable);
}

ispc::aabb getAABB(const ispc::Hittable* object) {
    switch (object->type) {
    case ispc::HittableType::SPHERE: {
        ispc::Sphere* sphere = (ispc::Sphere*)object->object;
        return sphere->bbox;
    }
    case ispc::HittableType::BVH_NODE: {
        ispc::BVH_Node* node = (ispc::BVH_Node*)object->object;
        return node->bbox;
    }
    case ispc::HittableType::QUAD: {
        ispc::Quad* quad = (ispc::Quad*)object->object;
        return quad->bbox;
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

static bool boxCompare(const ispc::Hittable* a, const ispc::Hittable* b, int axis) {
    ispc::aabb boxA = getAABB(a);
    ispc::aabb boxB = getAABB(b);
    return getAxis(boxA, axis).min < getAxis(boxB, axis).min;
}

static bool boxCompareX(const ispc::Hittable* a, const ispc::Hittable* b) {
    return boxCompare(a, b, 0);
}

static bool boxCompareY(const ispc::Hittable* a, const ispc::Hittable* b) {
    return boxCompare(a, b, 1);
}

static bool boxCompareZ(const ispc::Hittable* a, const ispc::Hittable* b) {
    return boxCompare(a, b, 2);
}

ispc::Hittable* constructBVH(std::vector<ispc::Hittable*>& srcObjects, size_t start, size_t end) {
    auto objects = srcObjects;

    ispc::Hittable* bvhNode = new ispc::Hittable;
    bvhNode->type = ispc::HittableType::BVH_NODE;
    ispc::BVH_Node* node = new ispc::BVH_Node;
    bvhNode->object = (void*)node;

    auto axis = dist(gen);
    auto comparator = (axis == 0) ? boxCompareX : (axis == 1) ? boxCompareY : boxCompareZ;

    size_t objectSpan = end - start;
    if (objectSpan == 1) {
        node->left = node->right = objects[start];
    } else if (objectSpan == 2) {
        if (comparator(objects[start], objects[start + 1])) {
            node->left = objects[start];
            node->right = objects[start + 1];
        } else {
            node->left = objects[start + 1];
            node->right = objects[start];
        }
    } else {
        std::sort(objects.begin() + start, objects.begin() + end, comparator);
        auto mid = start + objectSpan / 2.0f;
        node->left = constructBVH(objects, start, mid);
        node->right = constructBVH(objects, mid, end);
    }

    auto leftBbox = getAABB(node->left);
    auto rightBbox = getAABB(node->right);
    node->bbox = createAABB(leftBbox, rightBbox);

    return bvhNode;
}

ispc::Hittable* createBVH(std::vector<ispc::Hittable*>& objects) {
    return constructBVH(objects, 0, objects.size());
}

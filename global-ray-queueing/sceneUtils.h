#pragma once

ispc::Camera* initializeCamera(int imageWidth, int samplesPerPixel, int maxDepth, float vfov, float aspectRatio,
                               ispc::float3 lookfrom, ispc::float3 lookat, ispc::float3 vup, ispc::float3 background) {
    ispc::Camera* camera = new ispc::Camera;
    camera->aspectRatio = aspectRatio;
    camera->imageWidth = imageWidth;
    camera->samplesPerPixel = samplesPerPixel;
    camera->maxDepth = maxDepth;
    camera->vfov = vfov;
    camera->lookfrom = lookfrom;
    camera->lookat = lookat;
    camera->vup = vup;
    camera->background = background;
    ispc::initialize(*camera);
    return camera;
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

void createHittable(ispc::HittableType type, void* object, std::vector<ispc::Hittable>& objects) {
    ispc::Hittable* hittable = new ispc::Hittable;
    hittable->type = type;
    hittable->object = object;
    objects.emplace_back(*hittable);
}

ispc::HittableList* createHittableList(std::vector<ispc::Hittable>& objects) {
    ispc::HittableList* hittableList = new ispc::HittableList;
    hittableList->objects = objects.data();
    hittableList->numObjects = objects.size();

    for (size_t i = 0; i < objects.size(); i++) {
        switch (objects[i].type) {
        case ispc::HittableType::SPHERE: {
            ispc::Sphere* sphere = (ispc::Sphere*)objects[i].object;
            hittableList->bbox = createAABB(hittableList->bbox, sphere->bbox);
            break;
        }
        case ispc::HittableType::QUAD: {
            ispc::Quad* quad = (ispc::Quad*)objects[i].object;
            hittableList->bbox = createAABB(hittableList->bbox, quad->bbox);
            break;
        }
        case ispc::HittableType::NODE: {
            ispc::Node* node = (ispc::Node*)objects[i].object;
            hittableList->bbox = createAABB(hittableList->bbox, node->bbox);
            break;
        }
        case ispc::HittableType::BVH: {
            ispc::Bvh* bvh = (ispc::Bvh*)objects[i].object;
            hittableList->bbox = createAABB(hittableList->bbox, bvh->nodes[bvh->root].bbox);
            break;
        }
        }
    }

    return hittableList;
}

ispc::HittableList* createHittableList(ispc::Hittable object) {
    ispc::HittableList* hittableList = new ispc::HittableList;

    ispc::Hittable* objs = new ispc::Hittable[1];
    objs[0] = object;

    hittableList->objects = objs;
    hittableList->numObjects = 1;

    auto bbox = getAABB(object);
    hittableList->bbox = createAABB(hittableList->bbox, bbox);

    return hittableList;
}

ispc::Material* createMaterial(ispc::MaterialType type, ispc::float3 albedo) {
    ispc::Material* material = new ispc::Material;
    material->type = type;
    material->albedo = albedo;
    return material;
}

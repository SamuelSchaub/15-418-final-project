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

ispc::Material* createMaterial(ispc::MaterialType type, ispc::float3 albedo) {
    ispc::Material* material = new ispc::Material;
    material->type = type;
    material->albedo = albedo;
    return material;
}

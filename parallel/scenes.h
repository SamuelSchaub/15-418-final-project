#pragma once
#include "render.h"
#include "sceneUtils.h"

// Scenes

void randomSpheres(int imageWidth, int samplesPerPixel, int maxDepth, float vfov, bool useBVH, int bvhMaxLeafSize, bool usePackets,
                   int numSpheres = 11, float zoom = 3.0f) {
    vfov = 20; // constant for random spheres

    auto lookfrom = ispc::float3{13, 2, zoom};
    auto lookat = ispc::float3{0, 0, 0};
    auto vup = ispc::float3{0, 1, 0};
    auto background = ispc::float3{0.7f, 0.8f, 1.0f};

    ispc::Camera* camera =
        initializeCamera(imageWidth, samplesPerPixel, maxDepth, vfov, 16.0f / 9.0f, lookfrom, lookat, vup, background);

    std::vector<ispc::Hittable> objects = std::vector<ispc::Hittable>();

    // ISPC Build Scene code
    ispc::Material* groundMaterial = createMaterial(ispc::MaterialType::LAMBERTIAN, ispc::float3{0.5f, 0.5f, 0.5f});
    ispc::Sphere* groundSphere = createSphere(ispc::float3{0.0f, -1000.0f, 0.0f}, 1000.0f, groundMaterial);
    createHittable(ispc::HittableType::SPHERE, (void*)groundSphere, objects);

    for (int a = -numSpheres / 2; a < numSpheres / 2; a++) {
        for (int b = -numSpheres / 2; b < numSpheres / 2; b++) {
            float choose_mat = (float)rand() / RAND_MAX;
            ispc::float3 center =
                ispc::float3{a + 0.9f * ((float)rand() / RAND_MAX), 0.2f, b + 0.9f * ((float)rand() / RAND_MAX)};

            auto result = add(center, ispc::float3{-4.0f, -0.2f, 0.0f});
            if (sqrtf(result.v[0] * result.v[0] + result.v[1] * result.v[1] + result.v[2] * result.v[2]) > 0.9f) {
                ispc::Material* sphereMaterial;
                if (choose_mat < 0.8) {
                    // diffuse
                    ispc::float3 rand1 =
                        ispc::float3{(float)rand() / RAND_MAX, (float)rand() / RAND_MAX, (float)rand() / RAND_MAX};
                    ispc::float3 rand2 =
                        ispc::float3{(float)rand() / RAND_MAX, (float)rand() / RAND_MAX, (float)rand() / RAND_MAX};
                    ispc::float3 albedo =
                        ispc::float3{rand1.v[0] * rand2.v[0], rand1.v[1] * rand2.v[1], rand1.v[2] * rand2.v[2]};
                    sphereMaterial = createMaterial(ispc::MaterialType::LAMBERTIAN, albedo);
                    ispc::Sphere* sphere = createSphere(center, 0.2f, sphereMaterial);
                    createHittable(ispc::HittableType::SPHERE, (void*)sphere, objects);
                } else if (choose_mat < 0.95) {
                    // mirror
                    ispc::float3 albedo = ispc::float3{1.0f, 1.0f, 1.0f};
                    sphereMaterial = createMaterial(ispc::MaterialType::MIRROR, albedo);
                    ispc::Sphere* sphere = createSphere(center, 0.2f, sphereMaterial);
                    createHittable(ispc::HittableType::SPHERE, (void*)sphere, objects);
                } else {
                    // glass
                    sphereMaterial = createMaterial(ispc::MaterialType::GLASS, ispc::float3{1.0f, 1.0f, 1.0f});
                    ispc::Sphere* sphere = createSphere(center, 0.2f, sphereMaterial);
                    createHittable(ispc::HittableType::SPHERE, (void*)sphere, objects);
                }
            }
        }
    }

    ispc::Material* material1 = createMaterial(ispc::MaterialType::GLASS, ispc::float3{1.0f, 1.0f, 1.0f});
    ispc::Sphere* sphere1 = createSphere(ispc::float3{0.0f, 1.0f, 0.0f}, 1.0f, material1);
    createHittable(ispc::HittableType::SPHERE, (void*)sphere1, objects);

    ispc::Material* material2 = createMaterial(ispc::MaterialType::LAMBERTIAN, ispc::float3{0.4f, 0.2f, 0.1f});
    ispc::Sphere* sphere2 = createSphere(ispc::float3{-4.0f, 1.0f, 0.0f}, 1.0f, material2);
    createHittable(ispc::HittableType::SPHERE, (void*)sphere2, objects);

    ispc::Material* material3 = createMaterial(ispc::MaterialType::MIRROR, ispc::float3{0.7f, 0.6f, 0.5f});
    ispc::Sphere* sphere3 = createSphere(ispc::float3{4.0f, 1.0f, 0.0f}, 1.0f, material3);
    createHittable(ispc::HittableType::SPHERE, (void*)sphere3, objects);

    ispc::HittableList* hittableList;
    ispc::Hittable root;
    std::vector<ispc::Node> nodes;
    if (useBVH) {
        ispc::Bvh* bvh = createBVH(objects, nodes, bvhMaxLeafSize);
        root.type = ispc::HittableType::BVH;
        root.object = (void*)bvh;
        hittableList = createHittableList(root);
    } else {
        hittableList = createHittableList(objects);
    }

    render(camera, hittableList, usePackets);
}

void cornellBox(int imageWidth, int samplesPerPixel, int maxDepth, float vfov, bool useBVH, int bvhMaxLeafSize, bool usePackets) {
    vfov = 40; // constant for cornell box

    auto lookfrom = ispc::float3{278, 278, -800};
    auto lookat = ispc::float3{278, 278, 0};
    auto vup = ispc::float3{0, 1, 0};
    auto background = ispc::float3{0.0f, 0.0f, 0.0f};

    ispc::Camera* camera =
        initializeCamera(imageWidth, samplesPerPixel, maxDepth, vfov, 1.0f, lookfrom, lookat, vup, background);

    ispc::Material* red = createMaterial(ispc::MaterialType::LAMBERTIAN, ispc::float3{0.65f, 0.05f, 0.05f});
    ispc::Material* white = createMaterial(ispc::MaterialType::LAMBERTIAN, ispc::float3{0.73f, 0.73f, 0.73f});
    ispc::Material* green = createMaterial(ispc::MaterialType::LAMBERTIAN, ispc::float3{0.12f, 0.45f, 0.15f});
    ispc::Material* light = createMaterial(ispc::MaterialType::DIFFUSE_LIGHT, ispc::float3{7.0f, 7.0f, 7.0f});

    ispc::Material* glass = createMaterial(ispc::MaterialType::GLASS, ispc::float3{1.0f, 1.0f, 1.0f});
    ispc::Material* mirror = createMaterial(ispc::MaterialType::MIRROR, ispc::float3{1.0f, 1.0f, 1.0f});

    ispc::Quad* quad1 = createQuad(ispc::float3{555, 0, 0}, ispc::float3{0, 555, 0}, ispc::float3{0, 0, 555}, green);
    ispc::Quad* quad2 = createQuad(ispc::float3{0, 0, 0}, ispc::float3{0, 555, 0}, ispc::float3{0, 0, 555}, red);

    // Light
    // ispc::Quad* quad3 = createQuad(ispc::float3{383, 554, 332}, ispc::float3{-200, 0, 0},
    //                                ispc::float3{0, 0, -200}, light);

    ispc::Quad* quad3 =
        createQuad(ispc::float3{113, 554, 127}, ispc::float3{330, 0, 0}, ispc::float3{0, 0, 305}, light);

    ispc::Quad* quad4 = createQuad(ispc::float3{0, 0, 0}, ispc::float3{555, 0, 0}, ispc::float3{0, 0, 555}, white);
    ispc::Quad* quad5 =
        createQuad(ispc::float3{555, 555, 555}, ispc::float3{-555, 0, 0}, ispc::float3{0, 0, -555}, white);
    ispc::Quad* quad6 = createQuad(ispc::float3{0, 0, 555}, ispc::float3{555, 0, 0}, ispc::float3{0, 555, 0}, white);

    // Spheres
    ispc::Sphere* sphere1 = createSphere(ispc::float3{152.5f, 120.0f, 147.5f}, 120.0f, glass);
    ispc::Sphere* sphere2 = createSphere(ispc::float3{387.5f, 120.0f, 377.5f}, 120.0f, mirror);

    std::vector<ispc::Hittable> objects = std::vector<ispc::Hittable>();

    // Spheres
    createHittable(ispc::HittableType::SPHERE, (void*)sphere1, objects);
    createHittable(ispc::HittableType::SPHERE, (void*)sphere2, objects);

    createHittable(ispc::HittableType::QUAD, (void*)quad1, objects);
    createHittable(ispc::HittableType::QUAD, (void*)quad2, objects);
    createHittable(ispc::HittableType::QUAD, (void*)quad3, objects);
    createHittable(ispc::HittableType::QUAD, (void*)quad4, objects);
    createHittable(ispc::HittableType::QUAD, (void*)quad5, objects);
    createHittable(ispc::HittableType::QUAD, (void*)quad6, objects);

    ispc::HittableList* hittableList;
    ispc::Hittable root;
    std::vector<ispc::Node> nodes;
    if (useBVH) {
        ispc::Bvh* bvh = createBVH(objects, nodes, bvhMaxLeafSize);
        root.type = ispc::HittableType::BVH;
        root.object = (void*)bvh;
        hittableList = createHittableList(root);
    } else {
        hittableList = createHittableList(objects);
    }

    render(camera, hittableList, usePackets);
}

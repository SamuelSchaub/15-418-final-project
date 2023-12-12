#include "bvh.h"
#include "raytracer.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>

// Helper Functions

void writePPMImage(ispc::Image& image, int width, int height, const char* filename) {
    FILE* fp = fopen(filename, "wb");

    // write ppm header
    fprintf(fp, "P3\n%d %d\n255\n", width, height);

    for (int i = 0; i < width * height; i++) {
        fprintf(fp, "%d %d %d \n", image.R[i], image.G[i], image.B[i]);
    }

    fclose(fp);
    std::cout << "Image written to " << filename << std::endl;
}

ispc::Camera* initializeCamera(int imageWidth, int samplesPerPixel, int maxDepth, float vfov,
                      float aspectRatio, ispc::float3 lookfrom, ispc::float3 lookat,
                      ispc::float3 vup, ispc::float3 background) {
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

ispc::HittableList* createHittableList(std::vector<ispc::Hittable*>& objects) {
    ispc::HittableList* hittableList = new ispc::HittableList;
    hittableList->objects = objects.data();
    hittableList->numObjects = objects.size();

    for (size_t i = 0; i < objects.size(); i++) {
        switch (objects[i]->type) {
        case ispc::HittableType::SPHERE: {
            ispc::Sphere* sphere = (ispc::Sphere*)objects[i]->object;
            hittableList->bbox = createAABB(hittableList->bbox, sphere->bbox);
            break;
        }
        case ispc::HittableType::BVH_NODE: {
            ispc::BVH_Node* node = (ispc::BVH_Node*)objects[i]->object;
            hittableList->bbox = createAABB(hittableList->bbox, node->bbox);
            break;
        }
        case ispc::HittableType::QUAD: {
            ispc::Quad* quad = (ispc::Quad*)objects[i]->object;
            hittableList->bbox = createAABB(hittableList->bbox, quad->bbox);
            break;
        }
        }
    }

    return hittableList;
}

ispc::HittableList* createHittableList(ispc::Hittable* object) {
    ispc::HittableList* hittableList = new ispc::HittableList;

    ispc::Hittable** objs = new ispc::Hittable*[1];
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

// Render scene

void render(ispc::Camera* camera, ispc::HittableList* hittableList, bool usePackets) {
    ispc::Image image;
    image.R = new int[camera->imageWidth * camera->imageHeight];
    image.G = new int[camera->imageWidth * camera->imageHeight];
    image.B = new int[camera->imageWidth * camera->imageHeight];

    std::chrono::steady_clock::time_point start;
    std::chrono::steady_clock::time_point end;
    std::cout << "Rendering image..." << std::endl;
    if (usePackets) {
        start = std::chrono::high_resolution_clock::now();
        ispc::renderImageWithPackets(image, *camera, *hittableList);
        end = std::chrono::high_resolution_clock::now();
    } else {
        start = std::chrono::high_resolution_clock::now();
        ispc::renderImage(image, *camera, *hittableList);
        end = std::chrono::high_resolution_clock::now();
    }
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Time taken by function: " << duration.count() << " milliseconds" << std::endl;

    writePPMImage(image, camera->imageWidth, camera->imageHeight, "image.ppm");

    delete[] image.R;
    delete[] image.G;
    delete[] image.B;
    delete camera;
    delete hittableList;
}

// Scenes

void randomSpheres(int imageWidth, int samplesPerPixel, int maxDepth, float vfov, bool useBVH,
                   bool usePackets, int numSpheres = 11, float zoom = 3.0f) {
    vfov = 20; // constant for random spheres

    auto lookfrom = ispc::float3{13, 2, zoom};
    auto lookat = ispc::float3{0, 0, 0};
    auto vup = ispc::float3{0, 1, 0};
    auto background = ispc::float3{0.7f, 0.8f, 1.0f};

    ispc::Camera* camera = initializeCamera(imageWidth, samplesPerPixel, maxDepth, vfov,
                                            16.0f / 9.0f, lookfrom, lookat, vup, background);

    std::vector<ispc::Hittable*> objects = std::vector<ispc::Hittable*>();

    // ISPC Build Scene code
    ispc::Material* groundMaterial =
        createMaterial(ispc::MaterialType::LAMBERTIAN, ispc::float3{0.5f, 0.5f, 0.5f});
    ispc::Sphere* groundSphere =
        createSphere(ispc::float3{0.0f, -1000.0f, 0.0f}, 1000.0f, groundMaterial);
    createHittable(ispc::HittableType::SPHERE, (void*)groundSphere, objects);

    for (int a = -numSpheres / 2; a < numSpheres / 2; a++) {
        for (int b = -numSpheres / 2; b < numSpheres / 2; b++) {
            float choose_mat = (float)rand() / RAND_MAX;
            ispc::float3 center = ispc::float3{a + 0.9f * ((float)rand() / RAND_MAX), 0.2f, b + 0.9f * ((float)rand() / RAND_MAX)};

            auto result = add(center, ispc::float3{-4.0f, -0.2f, 0.0f});
            if (sqrtf(result.v[0] * result.v[0] + result.v[1] * result.v[1] +
                      result.v[2] * result.v[2]) > 0.9f) {
                ispc::Material* sphereMaterial;
                if (choose_mat < 0.8) {
                    // diffuse
                    ispc::float3 rand1 =
                        ispc::float3{(float)rand() / RAND_MAX, (float)rand() / RAND_MAX,
                                     (float)rand() / RAND_MAX};
                    ispc::float3 rand2 =
                        ispc::float3{(float)rand() / RAND_MAX, (float)rand() / RAND_MAX,
                                     (float)rand() / RAND_MAX};
                    ispc::float3 albedo = ispc::float3{
                        rand1.v[0] * rand2.v[0], rand1.v[1] * rand2.v[1], rand1.v[2] * rand2.v[2]};
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
                    sphereMaterial =
                        createMaterial(ispc::MaterialType::GLASS, ispc::float3{1.0f, 1.0f, 1.0f});
                    ispc::Sphere* sphere = createSphere(center, 0.2f, sphereMaterial);
                    createHittable(ispc::HittableType::SPHERE, (void*)sphere, objects);
                }
            }
        }
    }

    ispc::Material* material1 =
        createMaterial(ispc::MaterialType::GLASS, ispc::float3{1.0f, 1.0f, 1.0f});
    ispc::Sphere* sphere1 = createSphere(ispc::float3{0.0f, 1.0f, 0.0f}, 1.0f, material1);
    createHittable(ispc::HittableType::SPHERE, (void*)sphere1, objects);

    ispc::Material* material2 =
        createMaterial(ispc::MaterialType::LAMBERTIAN, ispc::float3{0.4f, 0.2f, 0.1f});
    ispc::Sphere* sphere2 = createSphere(ispc::float3{-4.0f, 1.0f, 0.0f}, 1.0f, material2);
    createHittable(ispc::HittableType::SPHERE, (void*)sphere2, objects);

    ispc::Material* material3 =
        createMaterial(ispc::MaterialType::MIRROR, ispc::float3{0.7f, 0.6f, 0.5f});
    ispc::Sphere* sphere3 = createSphere(ispc::float3{4.0f, 1.0f, 0.0f}, 1.0f, material3);
    createHittable(ispc::HittableType::SPHERE, (void*)sphere3, objects);

    ispc::HittableList* hittableList;
    if (useBVH) {
        ispc::Hittable* root = createBVH(objects);
        hittableList = createHittableList(root);
    } else {
        hittableList = createHittableList(objects);
    }
    render(camera, hittableList, usePackets);
}

void cornellBox(int imageWidth, int samplesPerPixel, int maxDepth, float vfov, bool useBVH,
                bool usePackets) {
    vfov = 40; // constant for cornell box

    auto lookfrom = ispc::float3{278, 278, -800};
    auto lookat = ispc::float3{278, 278, 0};
    auto vup = ispc::float3{0, 1, 0};
    auto background = ispc::float3{0.0f, 0.0f, 0.0f};

    ispc::Camera* camera = initializeCamera(imageWidth, samplesPerPixel, maxDepth, vfov, 1.0f,
                                            lookfrom, lookat, vup, background);

    ispc::Material* red =
        createMaterial(ispc::MaterialType::LAMBERTIAN, ispc::float3{0.65f, 0.05f, 0.05f});
    ispc::Material* white =
        createMaterial(ispc::MaterialType::LAMBERTIAN, ispc::float3{0.73f, 0.73f, 0.73f});
    ispc::Material* green =
        createMaterial(ispc::MaterialType::LAMBERTIAN, ispc::float3{0.12f, 0.45f, 0.15f});
    ispc::Material* light =
        createMaterial(ispc::MaterialType::DIFFUSE_LIGHT, ispc::float3{7.0f, 7.0f, 7.0f});

    ispc::Material* glass =
        createMaterial(ispc::MaterialType::GLASS, ispc::float3{1.0f, 1.0f, 1.0f});
    ispc::Material* mirror =
        createMaterial(ispc::MaterialType::MIRROR, ispc::float3{1.0f, 1.0f, 1.0f});

    ispc::Quad* quad1 = createQuad(ispc::float3{555, 0, 0}, ispc::float3{0, 555, 0},
                                   ispc::float3{0, 0, 555}, green);
    ispc::Quad* quad2 =
        createQuad(ispc::float3{0, 0, 0}, ispc::float3{0, 555, 0}, ispc::float3{0, 0, 555}, red);

    // Light
    // ispc::Quad* quad3 = createQuad(ispc::float3{383, 554, 332}, ispc::float3{-200, 0, 0},
    //                                ispc::float3{0, 0, -200}, light);

    ispc::Quad* quad3 = createQuad(ispc::float3{113, 554, 127}, ispc::float3{330, 0, 0},
                                   ispc::float3{0, 0, 305}, light);

    ispc::Quad* quad4 =
        createQuad(ispc::float3{0, 0, 0}, ispc::float3{555, 0, 0}, ispc::float3{0, 0, 555}, white);
    ispc::Quad* quad5 = createQuad(ispc::float3{555, 555, 555}, ispc::float3{-555, 0, 0},
                                   ispc::float3{0, 0, -555}, white);
    ispc::Quad* quad6 = createQuad(ispc::float3{0, 0, 555}, ispc::float3{555, 0, 0},
                                   ispc::float3{0, 555, 0}, white);

    // Spheres
    ispc::Sphere* sphere1 = createSphere(ispc::float3{152.5f, 120.0f, 147.5f}, 120.0f, glass);
    ispc::Sphere* sphere2 = createSphere(ispc::float3{387.5f, 120.0f, 377.5f}, 120.0f, mirror);

    std::vector<ispc::Hittable*> objects = std::vector<ispc::Hittable*>();

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
    if (useBVH) {
        ispc::Hittable* root = createBVH(objects);
        hittableList = createHittableList(root);
    } else {
        hittableList = createHittableList(objects);
    };

    render(camera, hittableList, usePackets);
}

int main(int argc, char* argv[]) {
    int imageWidth;
    int samplesPerPixel;
    int maxDepth;
    float vfov;
    bool usePackets;
    bool useBVH;
    int scene;

    if (argc != 8) {
        std::cout
            << "Usage: " << argv[0]
            << " <image width> <samples per pixel> <max depth> <vfov> <usePackets> <useBVH> <scene>"
            << std::endl;
        return 1;
    }
    
    imageWidth = atoi(argv[1]);
    samplesPerPixel = atoi(argv[2]);
    maxDepth = atoi(argv[3]);
    vfov = atoi(argv[4]);
    usePackets = atoi(argv[5]);
    useBVH = atoi(argv[6]);
    scene = atoi(argv[7]);
    
    // Print out parameters
    std::cout << "Image Width: " << imageWidth << std::endl;
    std::cout << "Samples per Pixel: " << samplesPerPixel << std::endl;
    std::cout << "Max Depth: " << maxDepth << std::endl;
    std::cout << "Vertical FOV: " << vfov << std::endl;
    std::cout << "Use Packets: " << usePackets << std::endl;
    std::cout << "Use BVH: " << useBVH << std::endl;

    // Set up Scene
    float ZOOM = 30.0f;
    int NUM_SPHERES = 44;

    switch (scene) {
    case 1:
        std::cout << "Scene: Cornell Box" << std::endl;
        cornellBox(imageWidth, samplesPerPixel, maxDepth, vfov, useBVH, usePackets);
        break;
    case 2:
        std::cout << "Scene: random spheres" << std::endl;
        randomSpheres(imageWidth, samplesPerPixel, maxDepth, vfov, useBVH, usePackets); // From book
        break;
    case 3:
        std::cout << "Scene: random spheres w/ extra spheres" << std::endl;
        randomSpheres(imageWidth, samplesPerPixel, maxDepth, vfov, useBVH, usePackets, NUM_SPHERES, ZOOM); // More Spheres
        break;
    default:
        std::cout << "Invalid scene number" << std::endl;
        return 1;
    }
}

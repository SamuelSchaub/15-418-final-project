#include "raytracer.h"
#include <chrono>
#include <iostream>

void writePPMImage(ispc::Image& image, int width, int height, const char* filename) {
    FILE* fp = fopen(filename, "wb");

    // write ppm header
    fprintf(fp, "P3\n%d %d\n255\n", width, height);

    for (int i = 0; i < width * height; i++) {
        fprintf(fp, "%d %d %d \n", image.R[i], image.G[i], image.B[i]);
    }

    fclose(fp);
    printf("Done. \n");
}

ispc::Camera* initializeCamera(int imageWidth, int samplesPerPixel, int maxDepth, float vfov,
                               ispc::float3 lookfrom, ispc::float3 lookat, ispc::float3 vup) {
    ispc::Camera* camera = new ispc::Camera;
    camera->aspectRatio = 16.0f / 9.0f;
    camera->imageWidth = imageWidth;
    camera->samplesPerPixel = samplesPerPixel;
    camera->maxDepth = maxDepth;
    camera->vfov = vfov;
    camera->lookfrom = lookfrom;
    camera->lookat = lookat;
    camera->vup = vup;
    ispc::initialize(*camera);
    return camera;
}

ispc::Sphere* createSphere(ispc::float3 center, float radius, ispc::Material* material) {
    ispc::Sphere* sphere = new ispc::Sphere;
    sphere->center = center;
    sphere->radius = radius;
    sphere->mat = *material;
    return sphere;
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
    return hittableList;
}

ispc::Material* createMaterial(ispc::MaterialType type, ispc::float3 albedo) {
    ispc::Material* material = new ispc::Material;
    material->type = type;
    material->albedo = albedo;
    return material;
}

int main(int argc, char* argv[]) {
    bool usePackets;
    int imageWidth;
    int samplesPerPixel;
    int maxDepth;
    float vfov;

    if (argc != 6) {
        std::cout << "Usage: " << argv[0]
                  << " <image width> <samples per pixel> <max depth> <usePackets> <vfov>" << std::endl;
        return 1;
    }

    imageWidth = atoi(argv[1]);
    samplesPerPixel = atoi(argv[2]);
    maxDepth = atoi(argv[3]);
    usePackets = atoi(argv[4]);
    vfov = atoi(argv[5]);

    auto lookfrom = ispc::float3{-2, 2, 1};
    auto lookat = ispc::float3{0, 0, -1};
    auto vup = ispc::float3{0, 1, 0};

    ispc::Camera* camera =
        initializeCamera(imageWidth, samplesPerPixel, maxDepth, vfov, lookfrom, lookat, vup);

    ispc::Material* materialGround =
        createMaterial(ispc::MaterialType::LAMBERTIAN, ispc::float3{0.8f, 0.8f, 0.0f});
    ispc::Material* materialCenter =
        createMaterial(ispc::MaterialType::LAMBERTIAN, ispc::float3{0.1f, 0.2f, 0.5f});
    ispc::Material* materialLeft =
        createMaterial(ispc::MaterialType::MIRROR, ispc::float3{0.8f, 0.8f, 0.8f});
    ispc::Material* materialRight =
        createMaterial(ispc::MaterialType::MIRROR, ispc::float3{0.8f, 0.6f, 0.2f});

    ispc::Sphere* sphere1 =
        createSphere(ispc::float3{0.0f, -100.5f, -1.0f}, 100.0f, materialGround);
    ispc::Sphere* sphere2 = createSphere(ispc::float3{0.0f, 0.0f, -1.0f}, 0.5f, materialCenter);
    ispc::Sphere* sphere3 = createSphere(ispc::float3{-1.0f, 0.0f, -1.0f}, 0.5f, materialLeft);
    ispc::Sphere* sphere4 = createSphere(ispc::float3{1.0f, 0.0f, -1.0f}, 0.5f, materialRight);

    std::vector<ispc::Hittable> objects = std::vector<ispc::Hittable>();

    createHittable(ispc::HittableType::SPHERE, (void*)sphere1, objects);
    createHittable(ispc::HittableType::SPHERE, (void*)sphere2, objects);
    createHittable(ispc::HittableType::SPHERE, (void*)sphere3, objects);
    createHittable(ispc::HittableType::SPHERE, (void*)sphere4, objects);

    ispc::HittableList* hittableList = createHittableList(objects);

    // int* out = new int[camera->imageWidth * camera->imageHeight * 3];
    ispc::Image image;
    image.R = new int[camera->imageWidth * camera->imageHeight];
    image.G = new int[camera->imageWidth * camera->imageHeight];
    image.B = new int[camera->imageWidth * camera->imageHeight];

    std::chrono::steady_clock::time_point start;
    std::chrono::steady_clock::time_point end;
    // chrono::steady_clock::time_point duration;
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

    // delete[] out;
    delete[] image.R;
    delete[] image.G;
    delete[] image.B;
    delete camera;
    delete hittableList;
    return 0;
}

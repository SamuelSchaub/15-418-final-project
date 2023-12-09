#include "raytracer.h"
#include <iostream>
#include <chrono>


void writePPMImage(int* r, int* g, int* b, int width, int height, const char *filename) {
    FILE *fp = fopen(filename, "wb");

    // write ppm header
    fprintf(fp, "P3\n%d %d\n255\n", width, height);

    for (int i = 0; i < width * height; i += 1) {
        fprintf(fp, "%d %d %d \n", r[i], g[i], b[i]);
    }

    fclose(fp);
    printf("Done. \n");
}


ispc::Camera* initializeCamera(int imageWidth, int samplesPerPixel, int maxDepth) {
    ispc::Camera* camera = new ispc::Camera;
    camera->aspectRatio = 16.0f / 9.0f;
    camera->imageWidth = imageWidth;
    camera->samplesPerPixel = samplesPerPixel;
    camera->maxDepth = maxDepth;
    ispc::initialize(*camera);
    return camera;
}


ispc::Sphere* createSphere(ispc::float3 center, float radius) {
    ispc::Sphere* sphere = new ispc::Sphere;
    sphere->center = center;
    sphere->radius = radius;
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


int main(int argc, char* argv[]) {
    bool usePackets;
    int imageWidth;
    int samplesPerPixel;
    int maxDepth;

    if (argc != 5) {
        std::cout << "Usage: " << argv[0] << " <image width> <samples per pixel> <max depth> <usePackets>" << std::endl;
        return 1;
    }

    imageWidth = atoi(argv[1]);
    samplesPerPixel = atoi(argv[2]);
    maxDepth = atoi(argv[3]);
    usePackets = atoi(argv[4]);

    ispc::Camera* camera = initializeCamera(imageWidth, samplesPerPixel, maxDepth);

    
    ispc::Sphere* sphere1 = createSphere(ispc::float3{0.0f, 0.0f, -1.0f}, 0.5f);
    ispc::Sphere* sphere2 = createSphere(ispc::float3{0.0f, -100.5f, -1.0f}, 100.0f);

    std::vector<ispc::Hittable> objects = std::vector<ispc::Hittable>();

    createHittable(ispc::HittableType::SPHERE, (void*)sphere1, objects);
    createHittable(ispc::HittableType::SPHERE, (void*)sphere2, objects);

    ispc::HittableList* hittableList = createHittableList(objects);

    // int* out = new int[camera->imageWidth * camera->imageHeight * 3];
    int* outR = new int[camera->imageWidth * camera->imageHeight];
    int* outG = new int[camera->imageWidth * camera->imageHeight];
    int* outB = new int[camera->imageWidth * camera->imageHeight];

    auto start = std::chrono::high_resolution_clock::now();
    if (usePackets)
        ispc::renderImageWithPackets(*camera, *hittableList, outR, outG, outB);
    else
        ispc::renderImage(*camera, *hittableList, outR, outG, outB);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Time taken by function: " << duration.count() << " milliseconds" << std::endl;

    writePPMImage(outR, outG, outB, camera->imageWidth, camera->imageHeight, "image.ppm");

    // delete[] out;
    delete[] outR;
    delete[] outG;
    delete[] outB;
    delete camera;
    delete hittableList;
    return 0;
}

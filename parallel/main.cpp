#include "raytracer.h"
#include <iostream>
#include <chrono>


void writePPMImage(int* data, int width, int height, const char *filename) {
    FILE *fp = fopen(filename, "wb");

    // write ppm header
    fprintf(fp, "P3\n%d %d\n255\n", width, height);

    for (int i = 0; i < width * height * 3; i += 3) {
        fprintf(fp, "%d %d %d \n", data[i], data[i + 1], data[i + 2]);
    }

    fclose(fp);
    printf("Done. \n");
}


int main() {
    ispc::Camera camera;
    camera.aspectRatio = 16.0f / 9.0f;
    camera.imageWidth = 400;
    camera.samplesPerPixel = 100;
    camera.maxDepth = 50;
    ispc::initialize(camera);
    
    ispc::Sphere* sphere1 = new ispc::Sphere;
    
    ispc::float3 center1;
    center1.v[0] = 0;
    center1.v[1] = 0;
    center1.v[2] = -1;

    sphere1->center = center1;
    sphere1->radius = 0.5f;

    ispc::Sphere* sphere2 = new ispc::Sphere;

    ispc::float3 center2;
    center2.v[0] = 0;
    center2.v[1] = -100.5f;
    center2.v[2] = -1;

    sphere2->center = center2;
    sphere2->radius = 100;

    ispc::Hittable sphereHittable1; 
    sphereHittable1.type = ispc::HittableType::SPHERE;
    sphereHittable1.object = (void*)(sphere1);
    
    ispc::Hittable sphereHittable2;
    sphereHittable2.type = ispc::HittableType::SPHERE;
    sphereHittable2.object = (void*)(sphere2);

    ispc::HittableList hl;

    hl.objects = new ispc::Hittable[2];
    hl.objects[0] = sphereHittable1;
    hl.objects[1] = sphereHittable2;
    hl.numObjects = 2;

    int* out = new int[camera.imageWidth * camera.imageHeight * 3];

    auto start = std::chrono::high_resolution_clock::now();
    ispc::renderImage(camera, hl, out);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Time taken by function: " << duration.count() << " milliseconds" << std::endl;

    writePPMImage(out, camera.imageWidth, camera.imageHeight, "image.ppm");

    delete[] out;
    delete sphere1;
    delete sphere2;
    delete[] hl.objects;
    return 0;
}
#include "raytracer.h"
// #include "ispcrt.h"
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

    ispc::Sphere spheres[2];
    
    ispc::float3 center1;
    center1.v[0] = 0;
    center1.v[1] = 0;
    center1.v[2] = -1;

    spheres[0].center = center1;
    spheres[0].radius = 0.5f;

    ispc::float3 center2;
    center2.v[0] = 0;
    center2.v[1] = -100.5f;
    center2.v[2] = -1;

    spheres[1].center = center2;
    spheres[1].radius = 100;

    int* out = new int[camera.imageWidth * camera.imageHeight * 3];

    std::cout << "Rendering image..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    ispc::renderImage(camera, spheres, 2, out);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Time taken by function: " << duration.count() << " milliseconds" << std::endl;

    writePPMImage(out, camera.imageWidth, camera.imageHeight, "image.ppm");

    delete[] out;
    return 0;
}
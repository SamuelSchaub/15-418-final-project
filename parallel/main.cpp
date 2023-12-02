#include "raytracer.h"
#include <iostream>

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
    spheres[0].radius = 0.5;

    ispc::float3 center2;
    center2.v[0] = 0;
    center2.v[1] = -100.5;
    center2.v[2] = -1;

    spheres[1].center = center2;
    spheres[1].radius = 100;

    int* out = new int[camera.imageWidth * camera.imageHeight * 3];

    ispc::renderPixel(camera, spheres, 2, out);

    std::cout << "P3\n" << camera.imageWidth << ' ' << camera.imageHeight << "\n255\n";
    for (int j = 0; j < camera.imageHeight; ++j) {
        std::clog << "\rScanlines remaining: " << (camera.imageHeight - j) << ' ' << std::flush;
        for (int i = 0; i < camera.imageWidth; ++i) {
            int k = (j * camera.imageWidth + i) * 3;
            std::cout << out[k] << ' ' << out[k + 1] << ' ' << out[k + 2] << '\n';
        }
    }
    std::clog << "\rDone.                 \n";

    delete[] out;
    return 0;
}
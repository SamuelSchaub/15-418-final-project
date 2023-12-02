#include "raytracer.h"
#include <iostream>

int main() {
    ispc::Camera camera;
    camera.aspectRatio = 16.0f / 9.0f;
    camera.imageWidth = 400;
    camera.samplesPerPixel = 64;
    camera.maxDepth = 15;
    ispc::initialize(camera);

    ispc::Sphere spheres[2];
    
    spheres[0].center = {0, 0, -1};
    spheres[0].radius = 0.5;
    
    spheres[1].center = {0, -100.5, -1};
    spheres[1].radius = 100;

    int* out = new int[camera.imageWidth * camera.imageHeight * 3];


    ispc::renderPixel(camera, spheres, 2, out);

    std::cout << "P3\n" << camera.imageWidth << ' ' << camera.imageHeight << "\n255\n";
    for (int j = 0; j < camera.imageHeight; j++) {
        for (int i = 0; i < camera.imageWidth; i++) {
            int index = (j * camera.imageWidth + i) * 3;
            std::cout << out[index] << ' ' << out[index + 1] << ' ' << out[index + 2] << '\n';
        }
    }

    delete[] out;
    
    return 0;
}
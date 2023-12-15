#include "bvh.h"
#include "bvh.h"
#include "raytracer.h"
#include "scenes.h"
#include <algorithm>
#include <cfloat>
#include <chrono>
#include <iostream>
#include <limits>
#include <random>


int main(int argc, char* argv[]) {
    int imageWidth;
    int samplesPerPixel;
    int maxDepth;
    float vfov;
    int bvhMaxLeafSize;
    int scene;

    if (argc != 7) {
        std::cout << "Usage: " << argv[0]
                  << " <image width> <samples per pixel> <max depth> <vfov> <BVH leaf size> <scene>" << std::endl;
        return 1;
    }

    imageWidth = atoi(argv[1]);
    samplesPerPixel = atoi(argv[2]);
    maxDepth = atoi(argv[3]);
    vfov = atoi(argv[4]);
    bvhMaxLeafSize = atoi(argv[5]);
    scene = atoi(argv[6]);

    // Print out parameters
    std::cout << "Image Width: " << imageWidth << std::endl;
    std::cout << "Samples per Pixel: " << samplesPerPixel << std::endl;
    std::cout << "Max Depth: " << maxDepth << std::endl;
    std::cout << "Vertical FOV: " << vfov << std::endl;
    std::cout << "BVH Leaf Size: " << bvhMaxLeafSize << std::endl;

    // Set up Scene
    float ZOOM = 30.0f;
    int NUM_SPHERES = 44;

    switch (scene) {
    case 1:
        std::cout << "Scene: Cornell Box" << std::endl;
        cornellBox(imageWidth, samplesPerPixel, maxDepth, vfov, bvhMaxLeafSize);
        break;
    case 2:
        std::cout << "Scene: random spheres" << std::endl;
        randomSpheres(imageWidth, samplesPerPixel, maxDepth, vfov, bvhMaxLeafSize); // From book
        break;
    case 3:
        std::cout << "Scene: random spheres w/ extra spheres" << std::endl;
        randomSpheres(imageWidth, samplesPerPixel, maxDepth, vfov, bvhMaxLeafSize, NUM_SPHERES, ZOOM); // More Spheres
        break;
    default:
        std::cout << "Invalid scene number" << std::endl;
        return 1;
        std::cout << "Invalid scene number" << std::endl;
        return 1;
    }
}
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

ispc::Quad* createQuad(ispc::float3 Q, ispc::float3 u, ispc::float3 v, ispc::Material* material) {
    ispc::Quad* quad = new ispc::Quad;
    quad->Q = Q;
    quad->u = u;
    quad->v = v;
    quad->mat = *material;
    ispc::initQuad(*quad);
    return quad;
}

void glassTest(int argc, char* argv[]) {
    bool usePackets;
    int imageWidth;
    int samplesPerPixel;
    int maxDepth;
    float vfov;

    if (argc != 6) {
        std::cout << "Usage: " << argv[0]
                  << " <image width> <samples per pixel> <max depth> <usePackets> <vfov>"
                  << std::endl;
        return;
    }

    imageWidth = atoi(argv[1]);
    samplesPerPixel = atoi(argv[2]);
    maxDepth = atoi(argv[3]);
    vfov = atoi(argv[4]);
    usePackets = atoi(argv[5]);

    auto lookfrom = ispc::float3{-2, 2, 1};
    auto lookat = ispc::float3{0, 0, -1};
    auto vup = ispc::float3{0, 1, 0};
    auto background = ispc::float3{0.7f, 0.8f, 1.0f};

    ispc::Camera* camera = initializeCamera(imageWidth, samplesPerPixel, maxDepth, vfov,
                                            (16.0f / 9.0f), lookfrom, lookat, vup, background);

    ispc::Material* materialGround =
        createMaterial(ispc::MaterialType::LAMBERTIAN, ispc::float3{0.8f, 0.8f, 0.0f});
    ispc::Material* materialCenter =
        createMaterial(ispc::MaterialType::LAMBERTIAN, ispc::float3{0.1f, 0.2f, 0.5f});
    ispc::Material* materialLeft =
        createMaterial(ispc::MaterialType::GLASS, ispc::float3{0.0f, 0.0f, 0.0f});
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
}

void quadTest(int argc, char* argv[]) {
    bool usePackets;
    int imageWidth;
    int samplesPerPixel;
    int maxDepth;
    float vfov;

    if (argc != 6) {
        std::cout << "Usage: " << argv[0]
                  << " <image width> <samples per pixel> <max depth> <usePackets> <vfov>"
                  << std::endl;
        return;
    }

    imageWidth = atoi(argv[1]);      // 400
    samplesPerPixel = atoi(argv[2]); // 100
    maxDepth = atoi(argv[3]);        // 50
    vfov = atoi(argv[4]);            // 80
    usePackets = atoi(argv[5]);

    auto lookfrom = ispc::float3{0, 0, 9};
    auto lookat = ispc::float3{0, 0, 0};
    auto vup = ispc::float3{0, 1, 0};
    auto background = ispc::float3{0.7f, 0.8f, 1.0f};

    ispc::Camera* camera = initializeCamera(imageWidth, samplesPerPixel, maxDepth, vfov,
                                            (16.0f / 9.0f), lookfrom, lookat, vup, background);

    // Scene Setup
    ispc::Material* materialLeft =
        createMaterial(ispc::MaterialType::LAMBERTIAN, ispc::float3{1.0f, 0.2f, 0.2f});
    ispc::Material* materialBack =
        createMaterial(ispc::MaterialType::LAMBERTIAN, ispc::float3{0.2f, 1.0f, 0.2f});
    ispc::Material* materialRight =
        createMaterial(ispc::MaterialType::LAMBERTIAN, ispc::float3{0.2f, 0.2f, 1.0f});
    ispc::Material* materialUpper =
        createMaterial(ispc::MaterialType::LAMBERTIAN, ispc::float3{1.0f, 0.5f, 0.0f});
    ispc::Material* materialLower =
        createMaterial(ispc::MaterialType::LAMBERTIAN, ispc::float3{0.2f, 0.8f, 0.8f});

    // Quads
    ispc::Quad* quad1 = createQuad(ispc::float3{-3, -2, 5}, ispc::float3{0, 0, -4},
                                   ispc::float3{0, 4, 0}, materialLeft);
    ispc::Quad* quad2 = createQuad(ispc::float3{-2, -2, 0}, ispc::float3{4, 0, 0},
                                   ispc::float3{0, 4, 0}, materialBack);
    ispc::Quad* quad3 = createQuad(ispc::float3{3, -2, 1}, ispc::float3{0, 0, 4},
                                   ispc::float3{0, 4, 0}, materialRight);
    ispc::Quad* quad4 = createQuad(ispc::float3{-2, 3, 1}, ispc::float3{4, 0, 0},
                                   ispc::float3{0, 0, 4}, materialUpper);
    ispc::Quad* quad5 = createQuad(ispc::float3{-2, -3, 5}, ispc::float3{4, 0, 0},
                                   ispc::float3{0, 0, -4}, materialLower);

    std::vector<ispc::Hittable> objects = std::vector<ispc::Hittable>();

    createHittable(ispc::HittableType::QUAD, (void*)quad1, objects);
    createHittable(ispc::HittableType::QUAD, (void*)quad2, objects);
    createHittable(ispc::HittableType::QUAD, (void*)quad3, objects);
    createHittable(ispc::HittableType::QUAD, (void*)quad4, objects);
    createHittable(ispc::HittableType::QUAD, (void*)quad5, objects);

    ispc::HittableList* hittableList = createHittableList(objects);

    // Render

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
}

void cornellBox(int argc, char* argv[]) {
    bool usePackets;
    int imageWidth;
    int samplesPerPixel;
    int maxDepth;
    float vfov;

    if (argc != 6) {
        std::cout << "Usage: " << argv[0]
                  << " <image width> <samples per pixel> <max depth> <usePackets> <vfov>"
                  << std::endl;
        return;
    }

    imageWidth = atoi(argv[1]);      // 400
    samplesPerPixel = atoi(argv[2]); // 200
    maxDepth = atoi(argv[3]);        // 50
    vfov = atoi(argv[4]);            // 40
    usePackets = atoi(argv[5]);

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

    ispc::HittableList* hittableList = createHittableList(objects);

    // Render

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
}

int main(int argc, char* argv[]) {
    switch (3) {
    case 1:
        glassTest(argc, argv);
        break;
    case 2:
        quadTest(argc, argv);
        break;
    case 3:
        cornellBox(argc, argv);
        break;
    default:
        break;
    }
    return 0;
}

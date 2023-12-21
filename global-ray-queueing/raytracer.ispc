// (!) DO NOT CHANGE ORDERING OF INCLUDE STATEMENTS 
#include "interval.isph"
#include "vec3.isph"
#include "random.isph"
#include "camera.isph"
#include "material.isph"
#include "hittable.isph"
#include "ray.isph"
#include "bvh.isph"
#include "quad.isph"
#include "sphere.isph"

export struct HittableList {
    Hittable* objects;
    struct aabb bbox;
    int numObjects;
};

bool hitHittableList(uniform HittableList& hittables, Ray* r) {
    bool hitAnything = false;
    float closestSoFar = r->ray_t.max;

    for (uniform int i = 0; i < hittables.numObjects; i++) {
        r->ray_t.max = closestSoFar;
        uniform Hittable& hittable = hittables.objects[i];
        if (hitHittable(hittable, r)) {
            hitAnything = true;
            closestSoFar = r->rec.t;
        }
    }
    return hitAnything;
}

void rayPacketTrace(varying RNGState *uniform *uniform stateList, uniform Camera& camera, 
                    uniform RayPacket *uniform packet, uniform HittableList& hittables) {
    foreach (i = 0 ... packet->size) {
        if (packet->active[i]) {
            HitRecord rec;
            Interval range = {0.001f, infinity};

            packet->rays[i].rec = rec;
            packet->rays[i].ray_t = range;

            Ray scattered;
            Vec3 attenuation;

            bool didHit = hitHittableList(hittables, &(packet->rays[i]));
            if (didHit) {
                packet->rays[i].lightEmitted += emitted(&(packet->rays[i])) * packet->rays[i].color;
            } else {
                packet->rays[i].lightEmitted += camera.background * packet->rays[i].color;
                packet->active[i] = false;
            }

            varying RNGState *uniform state = stateList[packet->rays[i].imageIndex];
            if (didHit && scatter(state, packet->rays[i], attenuation, scattered)) {
                packet->rays[i].color *= attenuation;
                packet->rays[i].origin = scattered.origin;
                packet->rays[i].direction = scattered.direction;
                packet->rays[i].depth -= 1;
                if (packet->rays[i].depth <= 0) {
                    packet->active[i] = false;
                }
            } else {
                packet->active[i] = false;
            }
        }
    }
}

task void renderImageTile(uniform Image& image, uniform Camera& cam,
                                     uniform int rowsPerTask, uniform HittableList& hittables) {
    uniform int ystart = taskIndex * rowsPerTask;
    uniform int yend = min(ystart + rowsPerTask, cam.imageHeight);

    uniform const uint32 numRays = (yend - ystart) * cam.imageWidth * cam.samplesPerPixel;
    uniform const uint32 batchSize = numRays;

    uniform RayPacket allRays;
    uniform Ray *uniform rays = uniform new uniform Ray[numRays];
    uniform bool *uniform active = uniform new uniform bool[numRays];

    typedef varying RNGState *uniform statePtr;
    statePtr *uniform stateList = uniform new statePtr[cam.imageWidth * cam.imageHeight];

    foreach (j = ystart... yend, i = 0 ... cam.imageWidth) {
        statePtr state = uniform new varying RNGState;
        seed_rng(state, randomSeedGenerator(i, j) + programIndex);
        stateList[j * cam.imageWidth + i] = state;
        for (int sample = 0; sample < cam.samplesPerPixel; sample++) {
            int rowWidth = cam.imageWidth * cam.samplesPerPixel;
            int rayIndex = (j - ystart) * rowWidth + i * cam.samplesPerPixel + sample;
            Ray r = getRay(state, cam, i, j, rayIndex);
            rays[rayIndex] = r;
            active[rayIndex] = true;
        }
    }

    allRays.rays = rays;
    allRays.active = active;
    allRays.size = numRays;

    uniform RayPacket* uniform batchPacket = uniform new uniform RayPacket;
    for (uniform int batchStart = 0; batchStart < numRays; batchStart += batchSize) {
        batchPacket->rays = allRays.rays + batchStart;
        batchPacket->active = allRays.active + batchStart;
        batchPacket->size = batchSize;
        while (anyActive(batchPacket)) {
            rayPacketTrace(stateList, cam, batchPacket, hittables);
        }
    }

    // Write batch packet color data to image
    uint32 rowWidth = cam.imageWidth * cam.samplesPerPixel;
    for (uniform int j = ystart; j < yend; j++) {
        for (uniform int i = 0; i < cam.imageWidth; i++) {
            uniform Vec3 finalColor = {0.0f, 0.0f, 0.0f};
            Vec3 localColor = {0.0f, 0.0f, 0.0f};
            foreach (sample = 0 ... cam.samplesPerPixel) {
                uint32 rayIndex = (j - ystart) * rowWidth + i * cam.samplesPerPixel + sample;
                localColor += allRays.rays[rayIndex].lightEmitted;
            }
            finalColor.x += reduce_add(localColor.x);
            finalColor.y += reduce_add(localColor.y);
            finalColor.z += reduce_add(localColor.z);
            uniform int k = j * cam.imageWidth + i;
            writeColor(image, finalColor, cam.samplesPerPixel, k);
        }
    }

    delete[] rays;
    delete[] active;
    delete batchPacket;
    foreach (j = ystart ... yend, i = 0 ... cam.imageWidth) {
        delete stateList[j * cam.imageWidth + i];
    }
}

export void renderImage(uniform Image& image, uniform Camera& cam, uniform HittableList& hittables) {
    uniform int threadCount = 8;
    uniform int rowsPerTask = cam.imageHeight / threadCount;
    if (rowsPerTask * threadCount < cam.imageHeight) {
        rowsPerTask++;
    }

    launch[threadCount] renderImageTile(image, cam, rowsPerTask, hittables);
}
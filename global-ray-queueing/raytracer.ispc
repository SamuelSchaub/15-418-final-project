// =================================================================================================
// Constants

const uniform float pi = 3.1415926535897932385f;
const uniform float infinity = 340282346638528859811704183484516925440.0f;

// =================================================================================================
// Utility Functions

inline float degreesToRadians(float degrees) { return degrees * pi / 180.0f; }

inline uniform float degreesToRadians(uniform float degrees) { return degrees * pi / 180.0f; }

// =================================================================================================
// Vec3

typedef float<3> Vec3;

export Vec3;

inline float dot(const Vec3 u, const Vec3 v) { return u.x * v.x + u.y * v.y + u.z * v.z; }

inline uniform float dot(uniform const Vec3 u, uniform const Vec3 v) {
    return u.x * v.x + u.y * v.y + u.z * v.z;
}

inline float lengthSquared(const Vec3 v) { return dot(v, v); }

inline uniform float lengthSquared(uniform const Vec3 v) { return dot(v, v); }

inline float length(const Vec3 v) { return sqrt(lengthSquared(v)); }

inline uniform float length(uniform const Vec3 v) { return sqrt(lengthSquared(v)); }

inline Vec3 cross(const Vec3 u, const Vec3 v) {
    Vec3 product = {u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
    return product;
}

inline uniform Vec3 cross(uniform const Vec3 u, uniform const Vec3 v) {
    uniform Vec3 product = {u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
    return product;
}

inline Vec3 unitVector(const Vec3 v) { return v / length(v); }

inline uniform Vec3 unitVector(uniform const Vec3 v) { return v / length(v); }

inline Vec3 reflect(const Vec3 v, const Vec3 n) { return v - 2 * dot(v, n) * n; }

inline Vec3 refract(const Vec3 uv, const Vec3 n, float etaiOverEtat) {
    float cosTheta = min(dot(-uv, n), 1.0);
    Vec3 rOutPerp = etaiOverEtat * (uv + cosTheta * n);
    Vec3 rOutParallel = -sqrt(abs(1.0 - lengthSquared(rOutPerp))) * n;
    return rOutPerp + rOutParallel;
}

bool nearZero(const Vec3 v) {
    const float s = 1e-8f;
    return (abs(v.x) < s) && (abs(v.y) < s) && (abs(v.z) < s);
}

// =================================================================================================
// Interval

export struct interval {
    float min;
    float max;
};

interval intervalUnion(const interval& a, const interval& b) {
    interval result;
    result.min = min(a.min, b.min);
    result.max = max(a.max, b.max);
    return result;
}

bool contains(const interval a, const float x) { return (a.min <= x) && (x <= a.max); }

bool surrounds(const interval a, const float x) { return (a.min < x) && (x < a.max); }

float size(const interval& a) { return a.max - a.min; }

interval expand(const interval& a, const float delta) {
    float paddding = delta / 2.0f;
    interval result = {a.min - paddding, a.max + paddding};
    return result;
}

// =================================================================================================
// Material

export enum MaterialType {
    LAMBERTIAN,
    MIRROR,
    GLASS,
    DIFFUSE_LIGHT,
};

export struct Material {
    MaterialType type;
    Vec3 albedo;
};

// Hit Record

struct HitRecord {
    Material mat;
    Vec3 p;
    Vec3 normal;
    float t;
    bool frontFace;
};

// =================================================================================================
// Ray

export struct Ray {
    Vec3 origin;
    Vec3 direction;
    Vec3 color;
    Vec3 lightEmitted;
    interval ray_t;
    HitRecord rec;
    uint32 imageIndex;
    uint32 rayIndex;
    RNGState state;
    int depth;
};

typedef soa<8> Ray soaRay;

export struct RayPacket {
    Ray** rays;
    bool* active;
    uint32 size;
};

Vec3 rayAt(Ray* ray, float t) { return ray->origin + t * ray->direction; }

inline bool anyActive(uniform RayPacket *uniform packet) {
    bool localActive = false;
    foreach (i = 0 ... packet->size) {
        localActive |= packet->active[i];
    }
    return any(localActive);
}

// =================================================================================================
// AABB

export struct aabb {
    interval x;
    interval y;
    interval z;
};

aabb aabbFromPoints(const Vec3& a, const Vec3& b) {
    aabb result;
    interval x = {min(a.x, b.x), max(a.x, b.x)};
    interval y = {min(a.y, b.y), max(a.y, b.y)};
    interval z = {min(a.z, b.z), max(a.z, b.z)};
    result.x = x;
    result.y = y;
    result.z = z;
    return result;
}

aabb aabbFromAABBs(const aabb& a, const aabb& b) {
    aabb result;
    result.x = intervalUnion(a.x, b.x);
    result.y = intervalUnion(a.y, b.y);
    result.z = intervalUnion(a.z, b.z);
    return result;
}

interval getAxis(const uniform aabb& bbox, const int axis) {
    switch (axis) {
    case 0:
        return bbox.x;
    case 1:
        return bbox.y;
    case 2:
        return bbox.z;
    default:
        return bbox.x;
    }
}

bool aabbHit(const uniform aabb& bbox, Ray* r) {
    for (int a = 0; a < 3; a++) {
        float invD = 1.0f / r->direction[a];
        float orig = r->origin[a];

        interval axis = getAxis(bbox, a);
        float t0 = (axis.min - orig) * invD;
        float t1 = (axis.max - orig) * invD;

        if (invD < 0.0f) {
            float temp = t0;
            t0 = t1;
            t1 = temp;
        }

        if (t0 > r->ray_t.min) {
            r->ray_t.min = t0;
        }

        if (t1 < r->ray_t.max) {
            r->ray_t.max = t1;
        }

        if (r->ray_t.max <= r->ray_t.min) {
            return false;
        }
    }

    return true;
}

// =================================================================================================
// Random

// From (https://en.wikipedia.org/wiki/Tiny_Encryption_Algorithm)
uint32 randomSeedGenerator(const uint32 x, const uint32 y) {
    uint32 v = x;
    uint32 w = y;
    uint32 s = 0;

    uint32 k1 = 0xa341316c;
    uint32 k2 = 0xc8013ea4;
    uint32 k3 = 0xad90777d;
    uint32 k4 = 0x7e95761e;

    for (uint32 i = 0; i < 16; i++) {
        s += 0x9e3779b9;
        v += ((w << 4) + k1) ^ (w + s) ^ ((w >> 5) + k2);
        w += ((v << 4) + k3) ^ (v + s) ^ ((v >> 5) + k4);
    }

    return v;
}

RNGState randomState(varying const int x, varying const int y) {
    RNGState state;
    seed_rng(&state, randomSeedGenerator(x, y) + programIndex);
    return state;
}

float randomFloat(RNGState state) { return frandom(&state); }

float randomFloat(const RNGState state, const float minVal, const float maxVal) {
    return minVal + (maxVal - minVal) * randomFloat(state);
}

// =================================================================================================
// Random vector functions

Vec3 randomVec(const RNGState& state) {
    Vec3 v = {randomFloat(state), randomFloat(state), randomFloat(state)};
    return v;
}

Vec3 randomVec(const RNGState& state, const float minVal, const float maxVal) {
    Vec3 v = {randomFloat(state, minVal, maxVal), randomFloat(state, minVal, maxVal),
              randomFloat(state, minVal, maxVal)};
    return v;
}

Vec3 randomVecInUnitSphere(const RNGState& state) {
    while (true) {
        RNGState state = randomState(clock(), clock() + programIndex);
        Vec3 p = randomVec(state, -1, 1);
        if (lengthSquared(p) < 1)
            return p;
    }
}

Vec3 randomUnitVec(const RNGState state) { return unitVector(randomVecInUnitSphere(state)); }

Vec3 randomVecOnHemisphere(const RNGState& state, const Vec3& normal) {
    Vec3 onUnitSphere = randomUnitVec(state);
    if (dot(onUnitSphere, normal) > 0.0) {
        return onUnitSphere;
    } else {
        return (-1.0f * onUnitSphere);
    }
}

// =================================================================================================
// Image

export struct Image {
    uniform int* R;
    uniform int* G;
    uniform int* B;
};

// =================================================================================================
// Color

float linearToGamma(const float linearComponenet) { return sqrt(linearComponenet); }

uniform float linearToGamma(const uniform float linearComponenet) { return sqrt(linearComponenet); }

void writeColor(uniform Image& image, uniform Vec3 pixelColor, const uniform int samplesPerPixel,
                const uniform int k) {
    pixelColor = (1.0f / samplesPerPixel) * pixelColor;
    pixelColor.x = linearToGamma(pixelColor.x);
    pixelColor.y = linearToGamma(pixelColor.y);
    pixelColor.z = linearToGamma(pixelColor.z);

    uniform float low = 0.000f;
    uniform float high = 0.999f;

    uniform int r = (uniform int)(256 * clamp(pixelColor.x, low, high));
    uniform int g = (uniform int)(256 * clamp(pixelColor.y, low, high));
    uniform int b = (uniform int)(256 * clamp(pixelColor.z, low, high));

    image.R[k] = r;
    image.G[k] = g;
    image.B[k] = b;
}

void setFaceNormal(Ray *r, const Vec3& outwardNormal) {
    r->rec.frontFace = dot(r->direction, outwardNormal) < 0;
    r->rec.normal = r->rec.frontFace ? outwardNormal : -1.0f * outwardNormal;
}

// =================================================================================================
// Sphere Hittable

export struct Sphere {
    Vec3 center;
    Material mat;
    aabb bbox;
    float radius;
};

// ispc compiler bug
export void dummySphere(uniform Sphere& sphere) { return; }

bool hitSphere(Sphere sphere, Ray* r) {
    Vec3 oc = r->origin - sphere.center;
    float a = lengthSquared(r->direction);
    float halfB = dot(oc, r->direction);
    float c = lengthSquared(oc) - sphere.radius * sphere.radius;

    float discriminant = (halfB * halfB) - (a * c);
    if (discriminant < 0)
        return false;
    float sqrtd = sqrt(discriminant);

    float root = (-halfB - sqrtd) / a;
    if (!(surrounds(r->ray_t, root))) {
        root = (-halfB + sqrtd) / a;
        if (!(surrounds(r->ray_t, root))) {
            return false;
        }
    }

    r->rec.t = root;
    r->rec.p = rayAt(r, r->rec.t);
    r->rec.mat = sphere.mat;
    Vec3 outwardNormal = (r->rec.p - sphere.center) / sphere.radius;
    setFaceNormal(r, outwardNormal);
    return true;
}

// =================================================================================================
// Quad Hittable

export struct Quad {
    Vec3 Q;
    Vec3 u;
    Vec3 v;
    Material mat;
    Vec3 normal;
    float D;
    Vec3 w;
    aabb bbox;
};

// ispc compiler bug
export void dummyQuad(uniform Quad& quad) { return; }

export void initQuad(uniform Quad& quad) {
    uniform Vec3 n = cross(quad.u, quad.v);
    quad.normal = unitVector(n);
    quad.D = dot(quad.normal, quad.Q);
    quad.w = n / dot(n, n);
}

bool isInterior(const float a, const float b) {
    if ((a < 0) || (1 < a) || (b < 0) || (1 < b)) {
        return false;
    }
    return true;
}

bool hitQuad(Quad quad, Ray* r) {

    float denom = dot(quad.normal, r->direction);

    // No hit if the ray is parallel to the plane
    float epsilon = 1e-8;
    if (abs(denom) < epsilon) {
        return false;
    }

    // Return false if the hit point parameter t is outside the ray interval.
    float t = (quad.D - dot(quad.normal, r->origin)) / denom;
    if (!(contains(r->ray_t, t))) {
        return false;
    }

    Vec3 intersection = rayAt(r, t);
    Vec3 planarHitptVector = intersection - quad.Q;
    float alpha = dot(quad.w, cross(planarHitptVector, quad.v));
    float beta = dot(quad.w, cross(quad.u, planarHitptVector));

    if (!isInterior(alpha, beta)) {
        return false;
    }

    r->rec.t = t;
    r->rec.p = intersection;
    r->rec.mat = quad.mat;
    setFaceNormal(r, quad.normal);

    return true;
}

// =================================================================================================
// Scatter and Reflect Functions

bool lambertianScatter(Ray* rIn, Vec3& attenuation, Ray* scattered) {
    Vec3 scatterDirection = rIn->rec.normal + randomUnitVec(rIn->state);
    if (nearZero(scattered->direction)) {
        scattered->direction = rIn->rec.normal;
    }
    Ray newRay = {rIn->rec.p, scatterDirection};
    *scattered = newRay;
    attenuation = rIn->rec.mat.albedo;
    return true;
}

bool mirrorScatter(Ray* rIn, Vec3& attenuation, Ray* scattered) {
    Vec3 reflected = reflect(unitVector(rIn->direction), rIn->rec.normal);
    Ray newRay = {rIn->rec.p, reflected};
    *scattered = newRay;
    attenuation = rIn->rec.mat.albedo;
    return (dot(scattered->direction, rIn->rec.normal) > 0);
}

float glassReflectance(const float cosine, const float refIdx) {
    float r0 = (1 - refIdx) / (1 + refIdx);
    r0 = r0 * r0;
    return r0 + (1 - r0) * pow((1 - cosine), 5);
}

bool glassScatter(Ray* rIn, Vec3& attenuation, Ray* scattered) {
    float indexOfRefraction = 1.5; // constant

    Vec3 atten = {1.0f, 1.0f, 1.0f};
    attenuation = atten;

    float refractionRatio = rIn->rec.frontFace ? (1.0f / indexOfRefraction) : indexOfRefraction;

    Vec3 unitDirection = unitVector(rIn->direction);
    float cosTheta = min(dot(-unitDirection, rIn->rec.normal), 1.0);
    float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

    bool cannotRefract = (refractionRatio * sinTheta) > 1.0f;
    Vec3 direction;

    if (cannotRefract || glassReflectance(cosTheta, refractionRatio) > randomFloat(rIn->state)) {
        direction = reflect(unitDirection, rIn->rec.normal);
    } else {
        direction = refract(unitDirection, rIn->rec.normal, refractionRatio);
    }

    Ray newRay = {rIn->rec.p, direction};
    *scattered = newRay;

    return true;
}

Vec3 emitted(Ray* rIn) {
    switch (rIn->rec.mat.type) {
    case DIFFUSE_LIGHT:
        return rIn->rec.mat.albedo;
        break;
    default:
        Vec3 black = {0.0f, 0.0f, 0.0f};
        return black;
        break;
    }
}

bool scatter(Ray* rIn, Vec3& attenuation, Ray* scattered) {
    switch (rIn->rec.mat.type) {
    case LAMBERTIAN:
        return lambertianScatter(rIn, attenuation, scattered);
    case MIRROR:
        return mirrorScatter(rIn, attenuation, scattered);
    case GLASS:
        return glassScatter(rIn, attenuation, scattered);
    case DIFFUSE_LIGHT:
        return false;
    default:
        return false;
    }
}

// =================================================================================================
// Hittable

export enum HittableType { SPHERE, QUAD, NODE, BVH };

export struct Hittable {
    HittableType type;
    void* object;
};

// =================================================================================================
// BVH

export struct Node {
    aabb bbox;
    uint32 start;
    uint32 size;
    uint32 left;
    uint32 right;
};

bool isLeaf(const uniform Node& node) { return node.left == node.right; }

struct RayQueue {
    uniform bool *uniform inQueue;
    uniform uint32 size;
};

export struct Bvh {
    uniform Hittable* objects;
    uniform Node* nodes;
    uniform RayQueue *uniform rayQueues;
    uniform uint32 numNodes;
    uniform uint32 numObjects;
    uniform uint32 root;
};

export void dummyBVH(uniform Bvh& bvh) { return; }

export void dummyNode(uniform Node& node) { return; }


void createQueues(uniform Bvh& bvh, const uniform uint32 BATCH_SIZE) {
    bvh.rayQueues = uniform new uniform RayQueue[bvh.numNodes];
    for (uniform int i = 0; i < bvh.numNodes; i++) {
        uniform Node& node = bvh.nodes[i];
        if (isLeaf(node)) {
            bvh.rayQueues[i].size = BATCH_SIZE;
            bvh.rayQueues[i].inQueue = uniform new uniform bool[BATCH_SIZE];

            // initialize inQueue to false
            foreach (index = 0 ... BATCH_SIZE) {
                bvh.rayQueues[i].inQueue[index] = false;
            }
        }
    }
}

bool hitHittable(uniform Hittable& hittable, Ray* r);

void releaseQueue(uniform RayPacket *uniform packet, const uniform Bvh& bvh, const uniform uint32 nodeIndex,
                  const uniform Vec3 background) {
    uniform RayQueue queue = bvh.rayQueues[nodeIndex];
    uniform Node& node = bvh.nodes[nodeIndex];
    
    // print("releasing queue % with size: % \n", nodeIndex, queue.size);
    foreach (i = 0 ... queue.size) {
        if (queue.inQueue[i]) {
            Ray* ray = packet->rays[i];
            ray->ray_t.min = 0.001f;
            ray->ray_t.max = infinity;

            bool hitAnything = false;
            float closestSoFar = infinity;
            // print("hitting nodes \n");
            for (uniform int j = node.start; j < node.start + node.size; j++) {
                ray->ray_t.max = closestSoFar;
                uniform Hittable& hittable = bvh.objects[j];
                if (hitHittable(hittable, ray)) {
                    hitAnything = true;
                    closestSoFar = ray->rec.t;
                }
            }
            // print("finished hitting nodes \n");

            Ray* scattered = new Ray;
            Vec3 attenuation;
            // print("scattering ray \n");
            if (hitAnything) {
                // print("hit something, adding emitted color\n");
                ray->lightEmitted += emitted(ray) * ray->color;
            } else {
                ray->lightEmitted += background * ray->color;
                packet->active[i] = false;
            }
            // print("scattered ray\n");

            if (hitAnything && scatter(ray, attenuation, scattered)) {
                ray->color *= attenuation;
                ray->origin = scattered->origin;
                ray->direction = scattered->direction;
                ray->depth -= 1;
                if (ray->depth <= 0) {
                    packet->active[i] = false;
                }
                // print("hit something and scattered\n");
            } else {
                packet->active[i] = false;
                // print("set to false \n");
            }
            delete scattered;
            // print("finished scattering ray\n");
        }
    }
}

void releaseAllQueues(uniform RayPacket *uniform packet, uniform Bvh& bvh, uniform Vec3 background) {
    for (uniform int i = 0; i < bvh.numNodes; i++) {
        uniform Node& node = bvh.nodes[i];
        if (isLeaf(node)) {
            // print("releasing queue %\n", i);
            releaseQueue(packet, bvh, i, background);
        }
    }
}

void hitBVH(uniform Bvh& bvh, uniform RayPacket *uniform packet, Ray* r, int batchIndex, uniform Vec3 background) {
    for (uniform int i = 0; i < bvh.numNodes; i++) {
        uniform Node& node = bvh.nodes[i];
        if (isLeaf(node)) {
            if (aabbHit(node.bbox, r)) {
                bvh.rayQueues[i].inQueue[batchIndex] = true;
            }
        }
    }
}

bool hitHittable(uniform Hittable& hittable, Ray* r) {
    switch (hittable.type) {
    case SPHERE:
        Sphere* sphere = (Sphere*)(hittable.object);
        return hitSphere(*sphere, r);
        break;
    case QUAD:
        Quad* quad = (Quad*)(hittable.object);
        return hitQuad(*quad, r);
    default:
        return false;
    }
}

// =================================================================================================
// Camera

export struct Camera {
    uniform float aspectRatio;   // Ratio of image width over heigh
    uniform int imageWidth;      // Rendered image width in pixel count
    uniform int samplesPerPixel; // Count of random samples for each pixel
    uniform int maxDepth;        // Maximum number of ray bounces into scene
    uniform float vfov;
    uniform Vec3 lookfrom;
    uniform Vec3 lookat;
    uniform Vec3 vup;
    uniform int imageHeight;      // Rendered image height
    uniform Vec3 center;          // Camera center
    uniform Vec3 pixel00Location; // Location of pixel (0,0)
    uniform Vec3 pixelDeltaU;     // Offset to pixel to the right
    uniform Vec3 pixelDeltaV;     // Offset to pixel below
    uniform Vec3 u;
    uniform Vec3 v;
    uniform Vec3 w;
    uniform Vec3 background;
};

export void initialize(uniform Camera& cam) {
    cam.imageHeight = (int)(cam.imageWidth / cam.aspectRatio);
    cam.imageHeight = (cam.imageHeight < 1) ? 1 : cam.imageHeight;

    cam.center = cam.lookfrom;

    // Determine viewport dimensions.
    uniform float focalLength = length(cam.lookfrom - cam.lookat);
    uniform float theta = degreesToRadians(cam.vfov);
    uniform float h = tan(theta / 2);
    uniform float viewportHeight = 2 * h * focalLength;
    uniform float viewportWidth = viewportHeight * (float)cam.imageWidth / (float)cam.imageHeight;

    // Calculate the u,v,w unit basis vectors for the camera coordinate frame.
    cam.w = unitVector(cam.lookfrom - cam.lookat);
    cam.u = unitVector(cross(cam.vup, cam.w));
    cam.v = cross(cam.w, cam.u);

    // Calculate the vectors across the horizontal and down the vertical
    // viewport edges.
    uniform Vec3 viewportU = viewportWidth * cam.u;
    uniform Vec3 viewportV = viewportHeight * -cam.v;

    // Calculate the horizontal and vertical delta vectors from pixel to pixel.
    cam.pixelDeltaU = viewportU / (float)cam.imageWidth;
    cam.pixelDeltaV = viewportV / (float)cam.imageHeight;

    // Calculate the location of the upper left pixel.
    uniform Vec3 viewportUpperLeft =
        cam.center - (focalLength * cam.w) - viewportU / 2 - viewportV / 2;
    cam.pixel00Location = viewportUpperLeft + 0.5f * cam.pixelDeltaU + 0.5f * cam.pixelDeltaV;
}

// =================================================================================================
// Main functions

Vec3 pixelSampleSquare(RNGState& state, uniform Camera& cam) {
    float px = -0.5f + randomFloat(state);
    float py = -0.5f + randomFloat(state);
    return (px * cam.pixelDeltaU) + (py * cam.pixelDeltaV);
}

Ray getRay(RNGState state, uniform Camera& cam, int i, int j, uint32 rayIndex) {
    // Get a randomly sampled camera ray for the pixel at location i,j.
    Ray r;
    r.state = state;

    Vec3 pixelCenter = cam.pixel00Location + (i * cam.pixelDeltaU) + (j * cam.pixelDeltaV);
    Vec3 pixelSample = pixelCenter + pixelSampleSquare(state, cam);

    Vec3 rayOrigin = cam.center;
    Vec3 rayDirection = pixelSample - rayOrigin;

    r.origin = rayOrigin;
    r.direction = rayDirection;
    r.imageIndex = j * cam.imageWidth + i;

    Vec3 color = {1.0f, 1.0f, 1.0f};
    r.color = color;

    Vec3 lightEmitted = {0.0f, 0.0f, 0.0f};
    r.lightEmitted = lightEmitted;

    r.depth = cam.maxDepth;
    r.rayIndex = rayIndex;
    interval range = {0.001f, infinity};
    r.ray_t = range;
    HitRecord rec;
    r.rec = rec;

    return r;
}

task void renderImageTileWithPackets(uniform Image& image, uniform Camera& cam,
                                     uniform int rowsPerTask, uniform Bvh& bvh) {
    uniform int ystart = taskIndex * rowsPerTask;
    uniform int yend = min(ystart + rowsPerTask, cam.imageHeight);

    uniform uint32 NUM_RAYS = (yend - ystart) * cam.imageWidth * cam.samplesPerPixel; // total num of rays
    uniform uint32 BATCH_SIZE = NUM_RAYS;
    createQueues(bvh, BATCH_SIZE);

    uniform RayPacket allRays;
    typedef soaRay* soaRayPtr;
    uniform soaRay** uniform rays = uniform new uniform soaRayPtr[BATCH_SIZE];
    foreach (i = 0 ... BATCH_SIZE) {
        rays[i] = uniform new uniform soaRay;
    }
    uniform bool* uniform active = uniform new uniform bool[BATCH_SIZE];

    foreach (j = ystart ... yend, i = 0 ... cam.imageWidth) {
        for (int sample = 0; sample < cam.samplesPerPixel; sample++) {
            int k = (j * cam.imageWidth + i) * cam.samplesPerPixel + sample;
            RNGState state = randomState(k, k + programIndex);
            Ray r = getRay(state, cam, i, j, k);
            *(rays[k]) = r;
            active[k] = true;
        }
    }

    allRays.rays = rays;
    allRays.active = active;
    allRays.size = NUM_RAYS;

    // Loop over batches 
    for (uniform int batchStart = 0; batchStart < NUM_RAYS; batchStart += BATCH_SIZE) {
        uniform RayPacket *uniform batchPacket = uniform new uniform RayPacket;
        batchPacket->rays = allRays.rays + batchStart;
        batchPacket->active = allRays.active + batchStart;
        batchPacket->size = BATCH_SIZE;

        int iter = 0;
        // Trace vectors in batch to completion
        // while (anyActive(batchPacket)) {
        for (uniform int i = 0; i < cam.maxDepth; i++) {
            // print("iter: %", iter);
            // Hit packet on entire BVH vectorized
            foreach (batchIndex = 0 ... batchPacket->size) {
                // print("active: %d\n", batchPacket->active[batchIndex]);
                if (batchPacket->active[batchIndex]) {
                    hitBVH(bvh, batchPacket, batchPacket->rays[batchIndex], batchIndex, cam.background);
                }
            }
            // print("releasing all queues\n");
            // Release all queues, updating rays in packet
            releaseAllQueues(batchPacket, bvh, cam.background);
            iter++;
        }
    }
    // Write packet color data to image
    for (uniform int j = ystart; j < yend; j++) {
        for (uniform int i = 0; i < cam.imageWidth; i++) {
            uniform Vec3 finalColor = {0.0f, 0.0f, 0.0f};
            foreach (sample = 0 ... cam.samplesPerPixel) {
                uint32 rayIndex = (j * cam.imageWidth + i) * cam.samplesPerPixel + sample;
                Ray* r = allRays.rays[rayIndex];
                finalColor.x += reduce_add(r->lightEmitted.x);
                finalColor.y += reduce_add(r->lightEmitted.y);
                finalColor.z += reduce_add(r->lightEmitted.z);
            }
            uniform int k = (j * cam.imageWidth + i);
            writeColor(image, finalColor, cam.samplesPerPixel, k);
        }
    }
}

export void renderImageWithPackets(uniform Image& image, uniform Camera& cam, uniform Bvh& bvh) {
    uniform int threadCount = 1;
    uniform int rowsPerTask = cam.imageHeight / threadCount;
    if (rowsPerTask * threadCount < cam.imageHeight) {
        rowsPerTask++;
    }

    launch[threadCount] renderImageTileWithPackets(image, cam, rowsPerTask, bvh);
}
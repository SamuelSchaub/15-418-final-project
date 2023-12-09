// Constants

const float pi = 3.1415926535897932385f;
const float infinity = 340282346638528859811704183484516925440.0f;

// Utility Functions

inline float degreesToRadians(float degrees) {
  return degrees * pi / 180.0f;
}

// ===============================================================================

// Vec3

typedef float<3> Vec3;
export Vec3;

inline float dot(const Vec3 u, const Vec3 v) {
    return u.x * v.x + u.y * v.y + u.z * v.z;
}

inline float lengthSquared(const Vec3 v) {
    return dot(v, v);
}

inline float length(const Vec3 v) {
    return sqrt(lengthSquared(v));
}

inline Vec3 cross(const Vec3 u, const Vec3 v) {
    Vec3 product = { u.y * v.z - u.z * v.y, 
                             u.z * v.x - u.x * v.z,
                             u.x * v.y - u.y * v.x };
    return product;
}

inline Vec3 unitVector(const Vec3 v) {
    return v / length(v);
}

// ===============================================================================

// Random


// From (https://en.wikipedia.org/wiki/Tiny_Encryption_Algorithm)
uint32 randomSeedGenerator(uint32 x, uint32 y) {
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


RNGState randomState(int x, int y) {
    RNGState state;
    seed_rng(&state, randomSeedGenerator(x, y) + programIndex);
    return state;
}

float randomFloat(RNGState& state) {
    return frandom(&state);
}

float randomFloat(RNGState& state, float minVal, float maxVal) {
    return minVal + (maxVal - minVal) * randomFloat(state);
}

// Random vector functions

Vec3 randomVec(RNGState& state) {
    Vec3 v = { randomFloat(state), randomFloat(state), randomFloat(state) };
    return v;
}

Vec3 randomVec(RNGState& state, float minVal, float maxVal) {
    Vec3 v = { randomFloat(state, minVal, maxVal), randomFloat(state, minVal, maxVal), randomFloat(state, minVal, maxVal) };
    return v;
}

inline Vec3 randomVecInUnitSphere(RNGState& state) {
    while (true) {
        Vec3 p = randomVec(state, -1, 1);
        if (lengthSquared(p) < 1) return p;
    }
}

inline Vec3 randomUnitVec(RNGState& state) {
    return unitVector(randomVecInUnitSphere(state));
}

inline Vec3 randomVecOnHemisphere(RNGState& state, Vec3& normal) {
    Vec3 onUnitSphere = randomUnitVec(state);
    if (dot(onUnitSphere, normal) > 0.0) {
        return onUnitSphere;
    } else {
        return (-1.0f * onUnitSphere);
    }
}

// ===============================================================================

// Color

float linearToGamma(float linearComponenet) {
    return sqrt(linearComponenet);
}

void writeColor(Vec3& pixelColor, int samplesPerPixel, uniform int outR[], uniform int outG[], uniform int outB[], int k) {
    pixelColor = (1.0f / samplesPerPixel) * pixelColor;
    pixelColor.x = linearToGamma(pixelColor.x); 
    pixelColor.y = linearToGamma(pixelColor.y); 
    pixelColor.z = linearToGamma(pixelColor.z); 
    
    float low = 0.000f;
    float high = 0.999f;

    int r = (int)(256 * clamp(pixelColor.x, low, high));
    int g = (int)(256 * clamp(pixelColor.y, low, high));
    int b = (int)(256 * clamp(pixelColor.z, low, high));

    outR[k] = r;
    outG[k] = g;
    outB[k] = b;
}

// ===============================================================================

// Ray

struct Ray {
    Vec3 origin;
    Vec3 direction;
};

typedef Ray* rayPtr;

Vec3 rayAt(const Ray ray, float t) {
    return ray.origin + t * ray.direction;
}

bool anyActive(uniform bool* active, uniform int spp) {
    bool localActive = false;
    foreach (i = 0 ... spp) {
        localActive |= active[i];
    }

    return any(localActive);
}


// ===============================================================================

// Hit Record 

struct HitRecord {
    Vec3 p;
    Vec3 normal;
    float t;
    bool frontFace;
};

void setFaceNormal(HitRecord& rec, const Ray r, const Vec3& outwardNormal) {
    rec.frontFace = dot(r.direction, outwardNormal) < 0;
    rec.normal = rec.frontFace ? outwardNormal : -1.0f * outwardNormal;
}

// ===============================================================================

// Sphere Hittable

export struct Sphere {
    Vec3 center;
    float radius;
};


// ispc compiler bug
export void dummySphere(uniform Sphere& sphere) {
    return;
}


bool hitSphere(Sphere sphere, const Ray r, float rayTmin, float rayTmax, HitRecord& rec) {
    Vec3 oc = r.origin - sphere.center;
    float a = lengthSquared(r.direction);
    float halfB = dot(oc, r.direction);
    float c = lengthSquared(oc) - sphere.radius * sphere.radius;

    float discriminant = (halfB * halfB) - (a * c);
    if (discriminant < 0) return false;
    float sqrtd = sqrt(discriminant);
    
    float root = (-halfB - sqrtd) / a;
    if (!(rayTmin < root && root < rayTmax)) {
        root = (-halfB + sqrtd) / a;
        if (!(rayTmin < root && root < rayTmax)) {
            return false;
        }
    }
    
    rec.t = root;
    rec.p = rayAt(r, rec.t);
    Vec3 outwardNormal = (rec.p - sphere.center) / sphere.radius;
    setFaceNormal(rec, r, outwardNormal);
    return true;
}


// ===============================================================================

// Hittable
export enum HittableType {
    SPHERE
};

export struct Hittable {
    HittableType type;
    void* object;
};


// ===============================================================================

// Hittable List

export struct HittableList {
    int numObjects;
    Hittable* objects;
};


bool hitHittableList(uniform const HittableList& hittables, Ray r, float rayTmin, float rayTmax, HitRecord& rec) {
    HitRecord tempRec;
    bool hitAnything = false;
    float closestSoFar = rayTmax;
    
    for (int i = 0; i < hittables.numObjects; i++) {
        switch (hittables.objects[i].type) {
            case SPHERE:
                Sphere* sphere = (Sphere*)(hittables.objects[i].object);
                if (hitSphere(*sphere, r, rayTmin, closestSoFar, tempRec)) {
                    hitAnything = true;
                    closestSoFar = tempRec.t;
                    rec = tempRec;
                }
                break;
            default:
                break;
        }
    }
    return hitAnything;
}


// ===============================================================================

// Camera (not done yet)

export struct Camera {
    uniform float aspectRatio; // Ratio of image width over heigh
    uniform int imageWidth; // Rendered image width in pixel count
    uniform int samplesPerPixel; // Count of random samples for each pixel
    uniform int maxDepth; // Maximum number of ray bounces into scene
    uniform int imageHeight;   // Rendered image height
    uniform Vec3 center;      // Camera center
    uniform Vec3 pixel00Location; // Location of pixel (0,0)
    uniform Vec3 pixelDeltaU; // Offset to pixel to the right
    uniform Vec3 pixelDeltaV; // Offset to pixel below
};

export void initialize(uniform Camera& cam) {
    cam.imageHeight = (int)(cam.imageWidth / cam.aspectRatio);
    cam.imageHeight = (cam.imageHeight < 1) ? 1 : cam.imageHeight;

    uniform Vec3 origin = {0.0f, 0.0f, 0.0f};
    cam.center = origin;

    // Determine viewport dimensions.
    uniform float focalLength = 1.0f;
    uniform float viewportHeight = 2.0f;
    uniform float viewportWidth = viewportHeight * (float)cam.imageWidth / (float)cam.imageHeight;
    
    // Calculate the vectors across the horizontal and down the vertical
    // viewport edges.
    uniform Vec3 viewportU = {viewportWidth, 0.0f, 0.0f};
    uniform Vec3 viewportV = {0.0f, -viewportHeight, 0.0f};

    // Calculate the horizontal and vertical delta vectors from pixel to pixel.
    cam.pixelDeltaU = viewportU / (float)cam.imageWidth;
    cam.pixelDeltaV = viewportV / (float)cam.imageHeight;

    // Calculate the location of the upper left pixel.
    uniform Vec3 focalLengthOffset = {0.0f, 0.0f, focalLength};
    uniform Vec3 viewportUpperLeft = cam.center - focalLengthOffset - (viewportU / 2.0f) - (viewportV / 2.0f);
    cam.pixel00Location = viewportUpperLeft + 0.5f * cam.pixelDeltaU + 0.5f * cam.pixelDeltaV;
}

Vec3 pixelSampleSquare(RNGState& state, uniform Camera& cam) {
    float px = -0.5f + randomFloat(state);
    float py = -0.5f + randomFloat(state);
    return (px * cam.pixelDeltaU) + (py * cam.pixelDeltaV);
}

Ray getRay(RNGState& state, uniform Camera& cam, int i, int j) {
    // Get a randomly sampled camera ray for the pixel at location i,j.
    Vec3 pixelCenter = cam.pixel00Location + (i * cam.pixelDeltaU) + (j * cam.pixelDeltaV);
    Vec3 pixelSample = pixelCenter + pixelSampleSquare(state, cam);

    Vec3 rayOrigin = cam.center;
    Vec3 rayDirection = pixelSample - rayOrigin;

    Ray r;
    r.origin = rayOrigin;
    r.direction = rayDirection;

    return r;
}

Vec3 rayColor(RNGState& state, Ray r, uniform int depth, uniform const HittableList& hittables) {
    HitRecord rec;
    
    Vec3 black = {0.0f, 0.0f, 0.0f};
    if (depth <= 0) {
        return black;
    }

    float minBound = 0.001f;
    float maxBound = infinity;
    
    if (hitHittableList(hittables, r, minBound, maxBound, rec)) {
        r.origin = rec.p;
        r.direction = rec.normal + randomUnitVec(state);

        return 0.5f * rayColor(state, r, depth - 1, hittables);
    }

    Vec3 unitDirection = unitVector(r.direction);
    Vec3 white = {1.0f, 1.0f, 1.0f};
    Vec3 blue = {0.5f, 0.7f, 1.0f};
    float a = 0.5f * (unitDirection.y + 1.0f);
    return (1.0f - a) * white + a * blue;
}


Vec3 rayPacketColor(RNGState& state, uniform Ray* rays, uniform bool* active, uniform int depth, uniform int spp, uniform const HittableList& hittables) {
    uniform Vec3 white = {1.0f, 1.0f, 1.0f};
    uniform Vec3 blue = {0.5f, 0.7f, 1.0f};

    uniform float r = 0.0f;
    uniform float g = 0.0f;
    uniform float b = 0.0f;

    foreach (i = 0 ... spp) {
        Vec3 localColor = {0.0f, 0.0f, 0.0f};
        if (active[i]) {
            HitRecord rec;
            if (depth <= 0) {
                active[i] = false;
            } else if (hitHittableList(hittables, rays[i], 0.001f, infinity, rec)) {
                rays[i].origin = rec.p;
                rays[i].direction = rec.normal + randomUnitVec(state);
            } else {
                Vec3 unitDirection = unitVector(rays[i].direction);
                float t = 0.5f * (unitDirection.y + 1.0f);
                localColor += (1.0f - t) * white + t * blue;
                active[i] = false; 
            }
        }
        r += reduce_add(localColor.x);
        g += reduce_add(localColor.y);
        b += reduce_add(localColor.z);
    }

    Vec3 result = {r, g, b};
    if (anyActive(active, spp)) {
        result += 0.5f * rayPacketColor(state, rays, active, depth - 1, spp, hittables);
    }

    return result;
}


task void renderImageTile(uniform Camera& cam, 
                            uniform int rowsPerTask, 
                            uniform const HittableList& hittables, 
                            uniform int outR[], 
                            uniform int outG[], 
                            uniform int outB[]) {
    uniform int ystart = taskIndex * rowsPerTask;
    uniform int yend = min(ystart + rowsPerTask, cam.imageHeight);

    foreach (j = ystart ... yend, i = 0 ... cam.imageWidth) {
        RNGState state = randomState(i, j);
        Vec3 pixelColor = {0.0f, 0.0f, 0.0f};
        for (int sample = 0; sample < cam.samplesPerPixel; sample++) {
            Ray r = getRay(state, cam, i, j);
            pixelColor += rayColor(state, r, cam.maxDepth, hittables);
        }

        int k = (j * cam.imageWidth + i);
        // writeColor(pixelColor, cam.samplesPerPixel, outR, outG, outB, k);

        pixelColor = (1.0f / cam.samplesPerPixel) * pixelColor;
        pixelColor.x = linearToGamma(pixelColor.x); 
        pixelColor.y = linearToGamma(pixelColor.y); 
        pixelColor.z = linearToGamma(pixelColor.z); 
        
        float low = 0.000f;
        float high = 0.999f;

        int r = (int)(256 * clamp(pixelColor.x, low, high));
        int g = (int)(256 * clamp(pixelColor.y, low, high));
        int b = (int)(256 * clamp(pixelColor.z, low, high));

        outR[k] = r;
        outG[k] = g;
        outB[k] = b;
    }
}


export void renderImage(uniform Camera& cam, uniform const HittableList& hittables, uniform int outR[], uniform int outG[], uniform int outB[]) {
    uniform int threadCount = 8;
    uniform int rowsPerTask = cam.imageHeight / threadCount;
    if (rowsPerTask * threadCount < cam.imageHeight) {
        rowsPerTask++;
    }

    launch [threadCount] renderImageTile(cam, rowsPerTask, hittables, outR, outG, outB);
}

// task void renderImageTileWithPackets(uniform Camera& cam, uniform int rowsPerTask, uniform const HittableList& hittables, uniform int outR[], uniform int outG[], uniform int outB[]) {
//     uniform int ystart = taskIndex * rowsPerTask;
//     uniform int yend = min(ystart + rowsPerTask, cam.imageHeight);

//     for (int j = ystart; j < yend; j++) {
//         for (int i = 0; i < cam.imageWidth; i++) {
//             RNGState state = randomState(i, j);
//             uniform Ray* rays = new Ray[cam.samplesPerPixel];
//             uniform bool* active = new bool[cam.samplesPerPixel];
//             foreach (sample = 0 ... cam.samplesPerPixel) {
//                 rays[sample] = getRay(state, cam, i, j);
//                 active[sample] = true;
//             }

//             Vec3 pixelColor = rayPacketColor(state, rays, active, cam.maxDepth, cam.samplesPerPixel, hittables);
//             int k = (j * cam.imageWidth + i) * 3;
//             // writeColor(pixelColor, cam.samplesPerPixel, outR, outG, outB, k);
//             pixelColor = (1.0f / cam.samplesPerPixel) * pixelColor;
//             pixelColor.x = linearToGamma(pixelColor.x); 
//             pixelColor.y = linearToGamma(pixelColor.y); 
//             pixelColor.z = linearToGamma(pixelColor.z); 
            
//             float low = 0.000f;
//             float high = 0.999f;

//             int r = (int)(256 * clamp(pixelColor.x, low, high));
//             int g = (int)(256 * clamp(pixelColor.y, low, high));
//             int b = (int)(256 * clamp(pixelColor.z, low, high));

//             outR[k] = r;
//             outG[k] = g;
//             outB[k] = b;
//         }
//     }
// }


// export void renderImageWithPackets(uniform Camera& cam, uniform const HittableList& hittables, uniform int outR[], uniform int outG[], uniform int outB[]) {
//     uniform int threadCount = 8;
//     uniform int rowsPerTask = cam.imageHeight / threadCount;
//     if (rowsPerTask * threadCount < cam.imageHeight) {
//         rowsPerTask++;
//     }

//     launch [threadCount] renderImageTileWithPackets(cam, rowsPerTask, hittables, outR, outG, outB);
// }
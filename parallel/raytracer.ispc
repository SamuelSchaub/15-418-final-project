// Constants


const float pi = 3.1415926535897932385f;
const float infinity = 340282346638528859811704183484516925440.0f;


// Utility Functions


inline float degreesToRadians(float degrees) {
  return degrees * pi / 180.0f;
}


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

inline Vec3 reflect(const Vec3 v, const Vec3 n) {
    return v - 2 * dot(v, n) * n;
}


bool nearZero(const Vec3 v) {
    const float s = 1e-8f;
    return (abs(v.x) < s) && (abs(v.y) < s) && (abs(v.z) < s);
}


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

Vec3 randomVecInUnitSphere(RNGState& state) {
    while (true) {
        Vec3 p = randomVec(state, -1, 1);
        if (lengthSquared(p) < 1) return p;
    }
}

Vec3 randomUnitVec(RNGState& state) {
    return unitVector(randomVecInUnitSphere(state));
}

Vec3 randomVecOnHemisphere(RNGState& state, Vec3& normal) {
    Vec3 onUnitSphere = randomUnitVec(state);
    if (dot(onUnitSphere, normal) > 0.0) {
        return onUnitSphere;
    } else {
        return (-1.0f * onUnitSphere);
    }
}


// Image


export struct Image {
    uniform int* R;
    uniform int* G;
    uniform int* B;
};


// Color


float linearToGamma(float linearComponenet) {
    return sqrt(linearComponenet);
}

uniform float linearToGamma(uniform float linearComponenet) {
    return sqrt(linearComponenet);
}

inline void writeColor(uniform Image& image, Vec3& pixelColor, int samplesPerPixel, int k) {
    pixelColor = (1.0f / samplesPerPixel) * pixelColor;
    pixelColor.x = linearToGamma(pixelColor.x); 
    pixelColor.y = linearToGamma(pixelColor.y); 
    pixelColor.z = linearToGamma(pixelColor.z); 
    
    float low = 0.000f;
    float high = 0.999f;

    int r = (int)(256 * clamp(pixelColor.x, low, high));
    int g = (int)(256 * clamp(pixelColor.y, low, high));
    int b = (int)(256 * clamp(pixelColor.z, low, high));

    image.R[k] = r;
    image.G[k] = g;
    image.B[k] = b;
}

inline void writeColor(uniform Image& image, uniform Vec3& pixelColor, uniform int samplesPerPixel, uniform int k) {
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
    image.B[k] = b;;
}


// Ray


export struct Ray {
    Vec3 origin;
    Vec3 direction;
};

export struct RayPacket {
    Ray* rays;
    bool* active;
};

// typedef Ray* rayPtr;

Vec3 rayAt(const Ray ray, float t) {
    return ray.origin + t * ray.direction;
}

inline bool anyActive(uniform RayPacket& packet, uniform int spp) {
    bool localActive = false;
    foreach (i = 0 ... spp) {
        bool isActive = packet.active[i];
        localActive |= isActive;
    }
    return any(localActive);
}


// Material

export enum MaterialType {
    LAMBERTIAN,
    MIRROR,
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

void setFaceNormal(HitRecord& rec, const Ray r, const Vec3& outwardNormal) {
    rec.frontFace = dot(r.direction, outwardNormal) < 0;
    rec.normal = rec.frontFace ? outwardNormal : -1.0f * outwardNormal;
}


// Sphere Hittable


export struct Sphere {
    Vec3 center;
    Material mat;
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
    rec.mat = sphere.mat;
    Vec3 outwardNormal = (rec.p - sphere.center) / sphere.radius;
    setFaceNormal(rec, r, outwardNormal);
    return true;
}



bool lambertianScatter(RNGState& state, const Ray& rIn, HitRecord& rec, Vec3& attenuation, Ray& scattered) {
    Vec3 scatterDirection = rec.normal + randomUnitVec(state);
    if (nearZero(scattered.direction)) {
        scattered.direction = rec.normal;
    }
    Ray newRay = {rec.p, scatterDirection};
    scattered = newRay;
    attenuation = rec.mat.albedo;
    return true;
}


bool mirrorScatter(RNGState& state, const Ray& rIn, HitRecord& rec, Vec3& attenuation, Ray& scattered) {
    Vec3 reflected = reflect(unitVector(rIn.direction), rec.normal);
    Ray newRay = {rec.p, reflected};
    scattered = newRay;
    attenuation = rec.mat.albedo;
    return (dot(scattered.direction, rec.normal) > 0);
}


bool scatter(RNGState& state, const Ray& rIn, HitRecord& rec, Vec3& attenuation, Ray& scattered) {
    switch (rec.mat.type) {
        case LAMBERTIAN:
            return lambertianScatter(state, rIn, rec, attenuation, scattered);
        case MIRROR:
            return mirrorScatter(state, rIn, rec, attenuation, scattered);
        default:
            return false;
    }
}


// Hittable


export enum HittableType {
    SPHERE
};

export struct Hittable {
    HittableType type;
    void* object;
};


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


// Camera


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


// Main functions


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
    
    if (hitHittableList(hittables, r, 0.001f, infinity, rec)) {
        Ray scattered;
        Vec3 attenuation;
        if (scatter(state, r, rec, attenuation, scattered)) {
            return attenuation * rayColor(state, scattered, depth - 1, hittables);
        }

        return black;
    }

    Vec3 unitDirection = unitVector(r.direction);
    Vec3 white = {1.0f, 1.0f, 1.0f};
    Vec3 blue = {0.5f, 0.7f, 1.0f};
    float a = 0.5f * (unitDirection.y + 1.0f);
    return (1.0f - a) * white + a * blue;
}


uniform Vec3 rayPacketColor(RNGState& state, uniform RayPacket& packet, uniform int depth, uniform int spp, uniform const HittableList& hittables) {
    uniform Vec3 white = {1.0f, 1.0f, 1.0f};
    uniform Vec3 blue = {0.5f, 0.7f, 1.0f};

    uniform float r = 0.0f;
    uniform float g = 0.0f;
    uniform float b = 0.0f;

    foreach (i = 0 ... spp) {
        bool isActive = packet.active[i];
        if (isActive) {
            Vec3 localColor = {0.0f, 0.0f, 0.0f};
            HitRecord rec;
            if (depth <= 0) {
                packet.active[i] = false;
            } else if (hitHittableList(hittables, packet.rays[i], 0.001f, infinity, rec)) {
                packet.rays[i].origin = rec.p;
                packet.rays[i].direction = rec.normal + randomUnitVec(state);
            } else {
                Vec3 unitDirection = unitVector(packet.rays[i].direction);
                float t = 0.5f * (unitDirection.y + 1.0f);
                localColor += (1.0f - t) * white + t * blue;
                packet.active[i] = false; 
            }
            r += reduce_add(localColor.x);
            g += reduce_add(localColor.y);
            b += reduce_add(localColor.z);
        }
    }

    uniform Vec3 result = {r, g, b};

    if (anyActive(packet, spp)) {
        result += 0.5f * rayPacketColor(state, packet, depth - 1, spp, hittables);
    }

    return result;
}


task void renderImageTile(uniform Image& image, uniform Camera& cam, uniform int rowsPerTask, uniform const HittableList& hittables) {
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

        writeColor(image, pixelColor, cam.samplesPerPixel, k);
    }
}


export void renderImage(uniform Image& image, uniform Camera& cam, uniform const HittableList& hittables) {
    uniform int threadCount = 8;
    uniform int rowsPerTask = cam.imageHeight / threadCount;
    if (rowsPerTask * threadCount < cam.imageHeight) {
        rowsPerTask++;
    }

    launch [threadCount] renderImageTile(image, cam, rowsPerTask, hittables);
}

typedef soa<8> Ray soaRay;

task void renderImageTileWithPackets(uniform Image& image, uniform Camera& cam, uniform int rowsPerTask, uniform const HittableList& hittables) {
    uniform int ystart = taskIndex * rowsPerTask;
    uniform int yend = min(ystart + rowsPerTask, cam.imageHeight);

    for (uniform int j = ystart; j < yend; j++) {
        for (uniform int i = 0; i < cam.imageWidth; i++) {
            RNGState state = randomState(i, j);

            uniform RayPacket packet;

            uniform soaRay * uniform rays = uniform new uniform soaRay[cam.samplesPerPixel];
            uniform bool * uniform active = uniform new uniform bool[cam.samplesPerPixel];

            packet.rays = rays;
            packet.active = active;

            foreach (sample = 0 ... cam.samplesPerPixel) {
                packet.rays[sample] = getRay(state, cam, i, j);
                packet.active[sample] = true;
            }

            uniform Vec3 pixelColor = rayPacketColor(state, packet, cam.maxDepth, cam.samplesPerPixel, hittables);

            uniform int k = (j * cam.imageWidth + i);
            writeColor(image, pixelColor, cam.samplesPerPixel, k);

            delete[] rays;
            delete[] active;
        }
    }
}


export void renderImageWithPackets(uniform Image& image, uniform Camera& cam, uniform const HittableList& hittables) {
    uniform int threadCount = 8;
    uniform int rowsPerTask = cam.imageHeight / threadCount;
    if (rowsPerTask * threadCount < cam.imageHeight) {
        rowsPerTask++;
    }

    launch [threadCount] renderImageTileWithPackets(image, cam, rowsPerTask, hittables);
}
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
typedef float<3> Point3;
typedef float<3> Color;

export Vec3;
export Color;
export Point3;

float lengthSquared(const Vec3& v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

float length(const Vec3 v) {
    return sqrt(lengthSquared(v));
}

inline float dot(const Vec3& u, const Vec3& v) {
    return u.x * v.x + u.y * v.y + u.z * v.z;
}

inline Vec3 cross(const Vec3& u, const Vec3& v) {
    Vec3 product = { u.y * v.z - u.z * v.y, 
                             u.z * v.x - u.x * v.z,
                             u.x * v.y - u.y * v.x };
    return product;
}

inline Vec3 unitVector(Vec3 v) {
    return v / length(v);
}

// ===============================================================================

// Random


float randomFloat() {
    RNGState state;
    int seed = 234567876534567; // TODO implement proper seeding based system time
    seed_rng(&state, seed);
    float randFloat = frandom(&state);
    return randFloat;
}

float randomFloat(float minVal, float maxVal) {
    return minVal + (maxVal - minVal) * randomFloat();
}

// Random vector functions

Vec3 randomVec() {
    Vec3 v = { randomFloat(), randomFloat(), randomFloat() };
    return v;
}

Vec3 randomVec(float minVal, float maxVal) {
    Vec3 v = { randomFloat(minVal, maxVal), randomFloat(minVal, maxVal), randomFloat(minVal, maxVal) };
    return v;
}

inline Vec3 randomVecInUnitSphere() {
    while (true) {
        Vec3 p = randomVec(-1, 1);
        if (lengthSquared(p) < 1) return p;
    }
}

inline Vec3 randomUnitVec() {
    return unitVector(randomVecInUnitSphere());
}

inline Vec3 randomVecOnHemisphere(Vec3& normal) {
    Vec3 onUnitSphere = randomUnitVec();
    if (dot(onUnitSphere, normal) > 0.0) {
        return onUnitSphere;
    } else {
        return -onUnitSphere;
    }
}

// ===============================================================================

// Color

float linearToGamma(float linearComponenet) {
    return sqrt(linearComponenet);
}

void writeColor(Color& pixelColor, int samplesPerPixel, uniform int out[], int k) {
    pixelColor = (1.0f / samplesPerPixel) * pixelColor;
    pixelColor.x = linearToGamma(pixelColor.x); 
    pixelColor.y = linearToGamma(pixelColor.y); 
    pixelColor.z = linearToGamma(pixelColor.z); 
    
    float low = 0.000;
    float high = 0.999;

    print("Instance % writing to pixel % \n", programIndex, k);
    out[k] = (int)(255.999 * clamp(pixelColor.x, low, high));
    out[k + 1] = (int)(255.999 * clamp(pixelColor.y, low, high));
    out[k + 2] = (int)(255.999 * clamp(pixelColor.z, low, high));
}

// ===============================================================================

// Ray

struct Ray {
    Vec3 origin;
    Vec3 direction;
};

Vec3 rayAt(const Ray& ray, float t) {
    return ray.origin + t * ray.direction;
}


// ===============================================================================

// Hit Record 

struct HitRecord {
    Point3 p;
    Vec3 normal;
    float t;
    bool frontFace;
};

void setFaceNormal(HitRecord& rec, const Ray& r, const Vec3& outwardNormal) {
    rec.frontFace = dot(r.direction, outwardNormal) < 0;
    rec.normal = rec.frontFace ? outwardNormal : -outwardNormal;
}

// ===============================================================================

// Sphere Hittable

export struct Sphere {
    Point3 center;
    float radius;
};

bool hitSphere(const Sphere sphere, const Ray& r, float rayTmin, float rayTmax, HitRecord& rec) {
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

void freeSphere(const Sphere* sphere) {
    delete[] sphere;
}

// ===============================================================================

// Hittable

typedef void* Hittable;

enum HittableType {
    SPHERE
};

// ===============================================================================

// Hittable List

struct HittableList {
    Hittable* objects;
    HittableType* objectTypes;
    int numObjects;
};

// void clear(HittableList& hittableList) {
//     for (int i = 0; i < hittableList.numObjects; i++) {
//         uniform Hittable object = hittableList.objects[i];
//         uniform HittableType hittableType = hittableList.objectTypes[i];
//         switch (hittableType)
//         {
//             case SPHERE:
//                 freeSphere((uniform const Sphere*)object);
//                 break;
        
//             default:
//                 break;
//         }
//     }
// }

bool hitSphereList(uniform const Sphere sphereList[], uniform int numSpheres, const Ray& r, float rayTmin, float rayTmax, HitRecord& rec) {
    HitRecord tempRec;
    bool hitAnything = false;
    float closestSoFar = rayTmax;
    
    for (int i = 0; i < numSpheres; i++) {
        if (hitSphere(sphereList[i], r, rayTmin, closestSoFar, tempRec)) {
            hitAnything = true;
            closestSoFar = tempRec.t;
            rec = tempRec;
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
    uniform Point3 center;      // Camera center
    uniform Point3 pixel00Location; // Location of pixel (0,0)
    uniform Vec3 pixelDeltaU; // Offset to pixel to the right
    uniform Vec3 pixelDeltaV; // Offset to pixel below
};

export void initialize(uniform Camera& cam) {
    cam.imageHeight = (int)(cam.imageWidth / cam.aspectRatio);
    cam.imageHeight = (cam.imageHeight < 1) ? 1 : cam.imageHeight;

    uniform Point3 origin = {0, 0, 0};
    cam.center = origin;

    // Determine viewport dimensions.
    uniform float focalLength = 1.0;
    uniform float viewportHeight = 2.0;
    uniform float viewportWidth = viewportHeight * ((float)cam.imageWidth) / cam.imageHeight;
    
    // Calculate the vectors across the horizontal and down the vertical
    // viewport edges.
    uniform Vec3 viewportU = {viewportWidth, 0, 0};
    uniform Vec3 viewportV = {0, -viewportHeight, 0};

    // Calculate the horizontal and vertical delta vectors from pixel to pixel.
    cam.pixelDeltaU = viewportU / cam.imageWidth;
    cam.pixelDeltaV = viewportV / cam.imageHeight;

    // Calculate the location of the upper left pixel.
    uniform Vec3 focalLengthOffset = {0, 0, focalLength};
    uniform Vec3 viewportUpperLeft = cam.center - focalLengthOffset - (viewportU / 2) - (viewportV / 2);
    uniform Point3 pixel00Location = viewportUpperLeft + 0.5 * (cam.pixelDeltaU + cam.pixelDeltaV);
}

Vec3 pixelSampleSquare(uniform Camera& cam) {
    float px = -0.5 + randomFloat();
    float py = -0.5 + randomFloat();
    return (px * cam.pixelDeltaU) + (py * cam.pixelDeltaV);
}

Ray getRay(uniform Camera& cam, int i, int j) {
    // Get a randomly sampled camera ray for the pixel at location i,j.
    Point3 pixelCenter = cam.pixel00Location + (i * cam.pixelDeltaU) + (j * cam.pixelDeltaV);
    Point3 pixelSample = pixelCenter + pixelSampleSquare(cam);

    Point3 rayOrigin = cam.center;
    Point3 rayDirection = pixelSample - rayOrigin;


    Ray r;
    r.origin = rayOrigin;
    r.direction = rayDirection;

    return r;
}

Color rayColor(const Ray& r, int depth, uniform const Sphere sphereList[], uniform int numSpheres) {
    HitRecord rec;

    Color white = {0, 0, 0};
    if (depth <= 0) return white;

    float minBound = 0.001;
    float maxBound = infinity;
    if (hitSphereList(sphereList, numSpheres, r, minBound, maxBound, rec)) {
        Vec3 direction = rec.normal + randomUnitVec();

        Ray bouncedRay;
        bouncedRay.origin = rec.p;
        bouncedRay.direction = direction;

        return 0.5 * rayColor(bouncedRay, depth - 1, sphereList, numSpheres);
    }

    Vec3 unitDirection = unitVector(r.direction);
    float a = 0.5 * (unitDirection.y + 1.0);

    Color color1 = {1.0, 1.0, 1.0};
    Color color2 = {0.5, 0.7, 1.0};
    return (1.0 - a) * color1 + a * color2;
}

export void renderPixel(uniform Camera& cam, uniform const Sphere sphereList[], uniform int numSpheres, uniform int out[]) {
    foreach(j = 0 ... cam.imageHeight, i = 0 ... cam.imageWidth) {
        Color pixelColor = {0, 0, 0};
        for (int sample = 0; sample < cam.samplesPerPixel; sample++) {
            Ray r = getRay(cam, i, j);
            pixelColor += rayColor(r, cam.maxDepth, sphereList, numSpheres);
        }
        int k = (j * cam.imageWidth + i) * 3;
        writeColor(pixelColor, cam.samplesPerPixel, out, k);
    }
}
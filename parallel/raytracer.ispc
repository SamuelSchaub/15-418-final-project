// Constants

const float pi = 3.1415926535897932385;
const float infinity = 340282346638528859811704183484516925440.0f;

// Utility Functions

inline float degreesToRadians(float degrees) {
  return degrees * pi / 180.0;
}

// ===============================================================================

// Vec3

typedef float<3> Vec3;
typedef Vec3 Point3;

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
    Vec3 product = { u.y * v.z - u.z * v.x, 
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

float randomFloat(float min, float max) {
    return min + (max - min) * randomFloat();
}

// Random vector functions

Vec3 randomVec() {
    Vec3 v = { randomFloat(), randomFloat(), randomFloat() };
    return v;
}

Vec3 randomVec(float min, float max) {
    Vec3 v = { randomFloat(min, max), randomFloat(min, max), randomFloat(min, max) };
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

void writeColor(Vec3& pixelColor, int samplesPerPixel) {
    pixelColor = (1.0f / samplesPerPixel) * pixelColor;
    pixelColor.x = linearToGamma(pixelColor.x); 
    pixelColor.y = linearToGamma(pixelColor.y); 
    pixelColor.z = linearToGamma(pixelColor.z); 
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

struct Sphere {
    uniform Point3 center;
    uniform float radius;
};

bool hitSphere(uniform const Sphere& sphere, const Ray& r, float rayTmin, float rayTmax, HitRecord& rec) {
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

void freeSphere(uniform const Sphere* sphere) {
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
    uniform Hittable* objects;
    uniform HittableType* objectTypes;
    uniform int numObjects;
};

void clear(uniform HittableList& hittableList) {
    for (int i = 0; i < hittableList.numObjects; i++) {
        Hittable object = hittableList.objects[i];
        HittableType hittableType = hittableList.objectTypes[i];
        switch (hittableType)
        {
            case SPHERE:
                freeSphere((uniform const Sphere*)object);
                break;
        
            default:
                break;
        }
    }
}

bool hitSphereList(uniform const Sphere sphereList[], uniform int numSpheres, const Ray& r, float rayTmin, float rayTmax, HitRecord& rec) {
    HitRecord tempRec;
    bool hitAnything = false;
    float closestSoFar = rayTmax;

    for (uniform int i = 0; i < numSpheres; i++) {
        if (hitSphere(sphereList[i], r, rayTmin, rayTmax, rec)) {
            hitAnything = true;
            closestSoFar = tempRec.t;
            rec = tempRec;
        }
    }
    
    return hitAnything;
}

// ===============================================================================

// Camera (not done yet)

struct Camera {
    float aspectRatio; // Ratio of image width over heigh
    int imageWidth; // Rendered image width in pixel count
    int samplesPerPixel; // Count of random samples for each pixel
    int maxDepth; // Maximum number of ray bounces into scene
    int image_height;   // Rendered image height
    Point3 center;      // Camera center
    Point3 pixel00Location; // Location of pixel (0,0)
    Vec3 pixelDeltaU; // Offset to pixel to the right
    Vec3 pixelDeltaV; // Offset to pixel below
};
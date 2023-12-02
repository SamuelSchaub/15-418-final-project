// Constants

const uniform float pi = 3.1415926535897932385f;
const uniform float infinity = 340282346638528859811704183484516925440.0f;

// Utility Functions

uniform inline float degreesToRadians(uniform float degrees) {
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

uniform float lengthSquared(const uniform Vec3& v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

uniform float length(const uniform Vec3 v) {
    return sqrt(lengthSquared(v));
}

uniform inline float dot(const uniform Vec3& u, const uniform Vec3& v) {
    return u.x * v.x + u.y * v.y + u.z * v.z;
}

uniform inline Vec3 cross(const uniform Vec3& u, const uniform Vec3& v) {
    uniform Vec3 product = { u.y * v.z - u.z * v.x, 
                     u.z * v.x - u.x * v.z,
                     u.x * v.y - u.y * v.x };
    return product;
}

uniform inline Vec3 unitVector(uniform Vec3 v) {
    return v / length(v);
}

// ===============================================================================

// Random


uniform float randomFloat() {
    uniform RNGState state;
    uniform int seed = 234567876534567; // TODO implement proper seeding based system time
    seed_rng(&state, seed);
    uniform float randFloat = frandom(&state);
    return randFloat;
}

uniform float randomFloat(uniform float min, uniform float max) {
    return min + (max - min) * randomFloat();
}

// Random vector functions

uniform Vec3 randomVec() {
    uniform Vec3 v = { randomFloat(), randomFloat(), randomFloat() };
    return v;
}

uniform Vec3 randomVec(uniform float min, uniform float max) {
    uniform Vec3 v = { randomFloat(min, max), randomFloat(min, max), randomFloat(min, max) };
    return v;
}

uniform inline Vec3 randomVecInUnitSphere() {
    while (true) {
        uniform Vec3 p = randomVec(-1, 1);
        if (lengthSquared(p) < 1) return p;
    }
}

uniform inline Vec3 randomUnitVec() {
    return unitVector(randomVecInUnitSphere());
}

uniform inline Vec3 randomVecOnHemisphere(uniform Vec3& normal) {
    uniform Vec3 onUnitSphere = randomUnitVec();
    if (dot(onUnitSphere, normal) > 0.0) {
        return onUnitSphere;
    } else {
        return -onUnitSphere;
    }
}

// ===============================================================================

// Color

uniform float linearToGamma(uniform float linearComponenet) {
    return sqrt(linearComponenet);
}

void writeColor(uniform Color& pixelColor, uniform int samplesPerPixel, uniform int out[], uniform int k) {
    pixelColor = (1.0f / samplesPerPixel) * pixelColor;
    pixelColor.x = linearToGamma(pixelColor.x); 
    pixelColor.y = linearToGamma(pixelColor.y); 
    pixelColor.z = linearToGamma(pixelColor.z); 
    
    uniform float low = 0.000;
    uniform float high = 0.999;

    out[k] = (uniform int)(255.999 * clamp(pixelColor.x, low, high));
    out[k + 1] = (uniform int)(255.999 * clamp(pixelColor.y, low, high));
    out[k + 2] = (uniform int)(255.999 * clamp(pixelColor.z, low, high));
}

// ===============================================================================

// Ray

struct Ray {
    uniform Vec3 origin;
    uniform Vec3 direction;
};

uniform Vec3 rayAt(uniform const Ray& ray, uniform float t) {
    return ray.origin + t * ray.direction;
}


// ===============================================================================

// Hit Record 

struct HitRecord {
    uniform Point3 p;
    uniform Vec3 normal;
    uniform float t;
    uniform bool frontFace;
};

void setFaceNormal(uniform HitRecord& rec, uniform const Ray& r, uniform const Vec3& outwardNormal) {
    rec.frontFace = dot(r.direction, outwardNormal) < 0;
    rec.normal = rec.frontFace ? outwardNormal : -outwardNormal;
}

// ===============================================================================

// Sphere Hittable

export struct Sphere {
    uniform Point3 center;
    uniform float radius;
};

uniform bool hitSphere(uniform const Sphere& sphere, uniform const Ray& r, uniform float rayTmin, uniform float rayTmax, uniform HitRecord& rec) {
    uniform Vec3 oc = r.origin - sphere.center;
    uniform float a = lengthSquared(r.direction);
    uniform float halfB = dot(oc, r.direction);
    uniform float c = lengthSquared(oc) - sphere.radius * sphere.radius;

    uniform float discriminant = (halfB * halfB) - (a * c);
    if (discriminant < 0) return false;
    uniform float sqrtd = sqrt(discriminant);
    
    uniform float root = (-halfB - sqrtd) / a;
    if (!(rayTmin < root && root < rayTmax)) {
        root = (-halfB + sqrtd) / a;
        if (!(rayTmin < root && root < rayTmax)) {
            return false;
        }
    }
    
    rec.t = root;
    rec.p = rayAt(r, rec.t);
    uniform Vec3 outwardNormal = (rec.p - sphere.center) / sphere.radius;
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

// void clear(uniform HittableList& hittableList) {
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

uniform bool hitSphereList(uniform const Sphere sphereList[], uniform int numSpheres, uniform const Ray& r, uniform float rayTmin, uniform float rayTmax, uniform HitRecord& rec) {
    uniform HitRecord tempRec;
    uniform bool hitAnything = false;
    uniform float closestSoFar = rayTmax;

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

uniform Vec3 pixelSampleSquare(uniform Camera& cam) {
    uniform float px = -0.5 + randomFloat();
    uniform float py = -0.5 + randomFloat();
    return (px * cam.pixelDeltaU) + (py * cam.pixelDeltaV);
}

uniform Ray getRay(uniform Camera& cam, uniform int i, uniform int j) {
    // Get a randomly sampled camera ray for the pixel at location i,j.
    uniform Point3 pixelCenter = cam.pixel00Location + (i * cam.pixelDeltaU) + (j * cam.pixelDeltaV);
    uniform Point3 pixelSample = pixelCenter + pixelSampleSquare(cam);

    uniform Point3 rayOrigin = cam.center;
    uniform Point3 rayDirection = pixelSample - rayOrigin;

    uniform Ray r;
    r.origin = rayOrigin;
    r.direction = rayDirection;

    return r;
}

uniform Color rayColor(uniform const Ray& r, uniform int depth, uniform const Sphere sphereList[], uniform int numSpheres) {
    uniform HitRecord rec;

    uniform Color white = {0, 0, 0};
    if (depth <= 0) return white;

    uniform float minBound = 0.001;
    uniform float maxBound = infinity;
    if (hitSphereList(sphereList, numSpheres, r, minBound, maxBound, rec)) {
        uniform Vec3 direction = rec.normal + randomUnitVec();

        uniform Ray bouncedRay;
        bouncedRay.origin = rec.p;
        bouncedRay.direction = direction;

        return 0.5 * rayColor(bouncedRay, depth - 1, sphereList, numSpheres);
    }

    uniform Vec3 unitDirection = unitVector(r.direction);
    uniform float a = 0.5 * (unitDirection.y + 1.0);

    uniform Color color1 = {1.0, 1.0, 1.0};
    uniform Color color2 = {0.5, 0.7, 1.0};
    return (1.0 - a) * color1 + a * color2;
}

export void renderPixel(uniform Camera& cam, uniform const Sphere sphereList[], uniform int numSpheres, uniform int out[]) {
    for (uniform int j = 0; j < cam.imageHeight; j++) {
        for (uniform int i = 0; i < cam.imageWidth; i++) {
            uniform Color pixelColor = {0, 0, 0};
            for (uniform int sample = 0; sample < cam.samplesPerPixel; sample++) {
                uniform Ray r = getRay(cam, i, j);
                pixelColor += rayColor(r, cam.maxDepth, sphereList, numSpheres);
            }
            uniform int k = (j * cam.imageWidth + i) * 3;
            writeColor(pixelColor, cam.samplesPerPixel, out, k);
        }
    }
}
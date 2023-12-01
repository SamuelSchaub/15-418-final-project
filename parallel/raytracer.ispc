// Random
float randomFloat() {
    return 0;
}

float randomFloat(float min, float max) {
    return 0;
}


// Vec3
typedef float<3> Vec3;

float vecLengthSquared(const Vec3& v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

float vecLength(const Vec3 v) {
    return sqrt(vecLengthSquared(v));
}

Vec3 vecRandom() {
    return { randomfloat(), randomfloat(), randomfloat() };
}

Vec3 vecRandom(float min, float max) {
    return { randomfloat(min, max), randomfloat(min, max), randomfloat(min, max) };
}

inline float vecDot(const Vec3& u, const Vec3& v) {
    return u.x * v.x + u.y * v.y + u.z * v.z;
}

inline Vec3 vecCross(const Vec3& u, const Vec3& v) {
    return { u.y * v.z - u.z * v.x, 
             u.z * v.x - u.x * v.z,
             u.x * v.y - u.y * v.x };
}

inline Vec3 vecUnitVector(Vec3 v) {
    return v / vecLength(v)
}



// Ray
struct Ray {
    Vec3 origin;
    Vec3 dir;
};

Vec3 rayAt(Ray& ray, float t) {
    return ray.origin + t * ray.dir;
}


// Interval
struct Interval {
    float min;
    float max;
}

bool intervalContains(Interval& interval, float x) {
    return interval.min <= x && x <= interval.max;
}

bool intervalSurrounds(Interval& interval, float x) {
    return interval.min < x && x < interval.max;
}

float intervalClamp(Interval& interval, float x) {
    if (x < interval.min) {
        return interval.min;
    }

    if (x > interval.max) {
        return interval.max;
    }

    return x;
}


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
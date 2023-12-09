#ifndef RTWEEKEND_H
#define RTWEEKEND_H

#include <cmath>
#include <cstdlib>
#include <limits>
#include <memory>

// Usings

using std::shared_ptr;
using std::make_shared;
using std::sqrt;

// Constants

const float infinity = std::numeric_limits<float>::infinity();
const float pi = 3.1415926535897932385f;

// Utility Functions

inline float degrees_to_radians(float degrees) {
  return degrees * pi / 180.0;
}

inline float random_float() {
  // Returns a random real in [0, 1).
  return rand() / (RAND_MAX + 1.0f);
}

inline float random_float(float min, float max) {
  // Returns a random real in [mix, max);
  return min + (max - min) * random_float();
}

inline int random_int(int min, int max) {
  return static_cast<int>(random_float(min, max + 1));
}

// Common headers

#include "interval.h"
#include "ray.h"
#include "vec3.h"

#endif

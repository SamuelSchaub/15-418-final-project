
#include "shape.h"
#include "../geometry/util.h"

namespace Shapes {

Vec2 Sphere::uv(Vec3 dir) {
	float u = std::atan2(dir.z, dir.x) / (2.0f * PI_F);
	if (u < 0.0f) u += 1.0f;
	float v = std::acos(-1.0f * std::clamp(dir.y, -1.0f, 1.0f)) / PI_F;
	return Vec2{u, v};
}

BBox Sphere::bbox() const {
	BBox box;
	box.enclose(Vec3(-radius));
	box.enclose(Vec3(radius));
	return box;
}

PT::Trace Sphere::hit(Ray ray) const {
	//A3T2 - sphere hit

    // TODO (PathTracer): Task 2
    // Intersect this ray with a sphere of radius Sphere::radius centered at the origin.

    // If the ray intersects the sphere twice, ret should
    // represent the first intersection, but remember to respect
    // ray.dist_bounds! For example, if there are two intersections,
    // but only the _later_ one is within ray.dist_bounds, you should
    // return that one!

	float oDotD = dot(ray.point, ray.dir);
	float discriminant = 4 * oDotD * oDotD - 4 * ray.dir.norm_squared() * (ray.point.norm_squared() - radius * radius);
	if (discriminant < 0.0f) {
		return PT::Trace();
	}

	float denominator = 2 * ray.dir.norm_squared();

	float t1 = (-2 * oDotD + std::sqrt(discriminant)) / denominator;
	float t2 = (-2 * oDotD - std::sqrt(discriminant)) / denominator;
	float tFirst = std::min(t1, t2);
	float tSecond = std::max(t1, t2);

	bool finalHit;
	float finalDist;
	Vec3 finalPos;
	Vec3 finalNormal;
	Vec2 finalUV;

	auto firstPos = ray.at(tFirst);
	auto secondPos = ray.at(tSecond);
	bool isFirstHit = ray.dist_bounds.x <= tFirst && tFirst <= ray.dist_bounds.y;
	bool isSecondHit = ray.dist_bounds.x <= tSecond && tSecond <= ray.dist_bounds.y;
	if (isFirstHit) {
		finalHit = true;
		finalDist = tFirst;
		finalPos = firstPos;
		finalNormal = firstPos.unit();
		finalUV = uv(firstPos);
	} else if (isSecondHit) {
		finalHit = true;
		finalDist = tSecond;
		finalPos = secondPos;
		finalNormal = secondPos.unit();
		finalUV = uv(secondPos);
	} else {
		return PT::Trace();
	}

    PT::Trace ret;
    ret.origin = ray.point;
    ret.hit = finalHit;       // was there an intersection?
    ret.distance = finalDist;   // at what distance did the intersection occur?
    ret.position = finalPos; // where was the intersection?
    ret.normal = finalNormal;   // what was the surface normal at the intersection?
	ret.uv = finalUV; 	   // what was the uv coordinates at the intersection? (you may find Sphere::uv to be useful)
    return ret;
}

Vec3 Sphere::sample(RNG &rng, Vec3 from) const {
	die("Sampling sphere area lights is not implemented yet.");
}

float Sphere::pdf(Ray ray, Mat4 pdf_T, Mat4 pdf_iT) const {
	die("Sampling sphere area lights is not implemented yet.");
}

Indexed_Mesh Sphere::to_mesh() const {
	return Util::closed_sphere_mesh(radius, 2);
}

} // namespace Shapes

bool operator!=(const Shapes::Sphere& a, const Shapes::Sphere& b) {
	return a.radius != b.radius;
}

bool operator!=(const Shape& a, const Shape& b) {
	if (a.shape.index() != b.shape.index()) return false;
	return std::visit(
		[&](const auto& shape) {
			return shape != std::get<std::decay_t<decltype(shape)>>(b.shape);
		},
		a.shape);
}

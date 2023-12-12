//
// raytracer.h
// (Header automatically generated by the ispc compiler.)
// DO NOT EDIT THIS FILE.
//

#pragma once
#include <stdint.h>

#if !defined(__cplusplus)
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
#include <stdbool.h>
#else
typedef int bool;
#endif
#endif



#ifdef __cplusplus
namespace ispc { /* namespace */
#endif // __cplusplus
///////////////////////////////////////////////////////////////////////////
// Vector types with external visibility from ispc code
///////////////////////////////////////////////////////////////////////////

#ifndef __ISPC_VECTOR_float3__
#define __ISPC_VECTOR_float3__
#ifdef _MSC_VER
__declspec( align(16) ) struct float3 { float v[3]; };
#else
struct float3 { float v[3]; } __attribute__ ((aligned(16)));
#endif
#endif


///////////////////////////////////////////////////////////////////////////
// Enumerator types with external visibility from ispc code
///////////////////////////////////////////////////////////////////////////

#ifndef __ISPC_ENUM_HittableType__
#define __ISPC_ENUM_HittableType__
enum HittableType {
    SPHERE = 0,
    QUAD = 1,
    NODE = 2,
    BVH = 3 
};
#endif

#ifndef __ISPC_ENUM_MaterialType__
#define __ISPC_ENUM_MaterialType__
enum MaterialType {
    LAMBERTIAN = 0,
    MIRROR = 1,
    GLASS = 2,
    DIFFUSE_LIGHT = 3 
};
#endif


#ifndef __ISPC_ALIGN__
#if defined(__clang__) || !defined(_MSC_VER)
// Clang, GCC, ICC
#define __ISPC_ALIGN__(s) __attribute__((aligned(s)))
#define __ISPC_ALIGNED_STRUCT__(s) struct __ISPC_ALIGN__(s)
#else
// Visual Studio
#define __ISPC_ALIGN__(s) __declspec(align(s))
#define __ISPC_ALIGNED_STRUCT__(s) __ISPC_ALIGN__(s) struct
#endif
#endif

#ifndef __ISPC_STRUCT_Bvh__
#define __ISPC_STRUCT_Bvh__
struct Bvh {
    struct Hittable * objects;
    struct Node * nodes;
    uint32_t numNodes;
    uint32_t numObjects;
    uint32_t root;
};
#endif

#ifndef __ISPC_STRUCT_Hittable__
#define __ISPC_STRUCT_Hittable__
struct Hittable {
    enum HittableType type;
    void * object;
};
#endif

#ifndef __ISPC_STRUCT_interval__
#define __ISPC_STRUCT_interval__
struct interval {
    float min;
    float max;
};
#endif

#ifndef __ISPC_STRUCT_aabb__
#define __ISPC_STRUCT_aabb__
struct aabb {
    struct interval x;
    struct interval y;
    struct interval z;
};
#endif

#ifndef __ISPC_STRUCT_Node__
#define __ISPC_STRUCT_Node__
struct Node {
    struct aabb bbox;
    uint32_t start;
    uint32_t size;
    uint32_t left;
    uint32_t right;
};
#endif

#ifndef __ISPC_STRUCT_Material__
#define __ISPC_STRUCT_Material__
struct Material {
    enum MaterialType type;
    struct float3  albedo;
};
#endif

#ifndef __ISPC_STRUCT_Quad__
#define __ISPC_STRUCT_Quad__
struct Quad {
    struct float3  Q;
    struct float3  u;
    struct float3  v;
    struct Material mat;
    struct float3  normal;
    float D;
    struct float3  w;
    struct aabb bbox;
};
#endif

#ifndef __ISPC_STRUCT_Sphere__
#define __ISPC_STRUCT_Sphere__
struct Sphere {
    struct float3  center;
    struct Material mat;
    struct aabb bbox;
    float radius;
};
#endif

#ifndef __ISPC_STRUCT_Camera__
#define __ISPC_STRUCT_Camera__
struct Camera {
    float aspectRatio;
    int32_t imageWidth;
    int32_t samplesPerPixel;
    int32_t maxDepth;
    float vfov;
    struct float3  lookfrom;
    struct float3  lookat;
    struct float3  vup;
    int32_t imageHeight;
    struct float3  center;
    struct float3  pixel00Location;
    struct float3  pixelDeltaU;
    struct float3  pixelDeltaV;
    struct float3  u;
    struct float3  v;
    struct float3  w;
    struct float3  background;
};
#endif

#ifndef __ISPC_STRUCT_Image__
#define __ISPC_STRUCT_Image__
struct Image {
    int32_t * R;
    int32_t * G;
    int32_t * B;
};
#endif

#ifndef __ISPC_STRUCT_HittableList__
#define __ISPC_STRUCT_HittableList__
struct HittableList {
    struct Hittable * objects;
    struct aabb bbox;
    int32_t numObjects;
};
#endif


///////////////////////////////////////////////////////////////////////////
// Functions exported from ispc code
///////////////////////////////////////////////////////////////////////////
#if defined(__cplusplus) && (! defined(__ISPC_NO_EXTERN_C) || !__ISPC_NO_EXTERN_C )
extern "C" {
#endif // __cplusplus
#if defined(__cplusplus)
    extern void dummyBVH(struct Bvh &bvh);
#else
    extern void dummyBVH(struct Bvh *bvh);
#endif // dummyBVH function declaraion
#if defined(__cplusplus)
    extern void dummyNode(struct Node &node);
#else
    extern void dummyNode(struct Node *node);
#endif // dummyNode function declaraion
#if defined(__cplusplus)
    extern void dummyQuad(struct Quad &quad);
#else
    extern void dummyQuad(struct Quad *quad);
#endif // dummyQuad function declaraion
#if defined(__cplusplus)
    extern void dummySphere(struct Sphere &sphere);
#else
    extern void dummySphere(struct Sphere *sphere);
#endif // dummySphere function declaraion
#if defined(__cplusplus)
    extern void initQuad(struct Quad &quad);
#else
    extern void initQuad(struct Quad *quad);
#endif // initQuad function declaraion
#if defined(__cplusplus)
    extern void initialize(struct Camera &cam);
#else
    extern void initialize(struct Camera *cam);
#endif // initialize function declaraion
#if defined(__cplusplus)
    extern void renderImage(struct Image &image, struct Camera &cam, const struct HittableList &hittables);
#else
    extern void renderImage(struct Image *image, struct Camera *cam, const struct HittableList *hittables);
#endif // renderImage function declaraion
#if defined(__cplusplus)
    extern void renderImageWithPackets(struct Image &image, struct Camera &cam, const struct HittableList &hittables);
#else
    extern void renderImageWithPackets(struct Image *image, struct Camera *cam, const struct HittableList *hittables);
#endif // renderImageWithPackets function declaraion
#if defined(__cplusplus) && (! defined(__ISPC_NO_EXTERN_C) || !__ISPC_NO_EXTERN_C )
} /* end extern C */
#endif // __cplusplus


#ifdef __cplusplus
} /* namespace */
#endif // __cplusplus

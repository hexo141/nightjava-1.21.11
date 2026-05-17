#pragma once

#include <jni.h>

#ifdef _WIN32
#define JNI_EXPORT __declspec(dllexport)
#else
#define JNI_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

JNI_EXPORT jboolean JNICALL Java_com_nj_particle_ParticleNativeLib_batchTickParticles(
    JNIEnv* env, jclass clazz,
    jfloatArray positions, jfloatArray velocities,
    jintArray ages, jintArray maxAges, jint count,
    jfloat gravityStrength, jfloat velocityMultiplier);

JNI_EXPORT jfloatArray JNICALL Java_com_nj_particle_ParticleNativeLib_computeBillboardVertices(
    JNIEnv* env, jclass clazz,
    jfloatArray particlePositions, jfloatArray particleSizes,
    jfloatArray particleColors, jfloatArray cameraRotation,
    jfloat tickProgress, jfloatArray cameraPosition, jint vertexCount);

JNI_EXPORT jboolean JNICALL Java_com_nj_particle_ParticleNativeLib_batchMoveParticles(
    JNIEnv* env, jclass clazz,
    jfloatArray positions, jfloatArray velocities,
    jint count, jfloatArray worldBounds);

#ifdef __cplusplus
}
#endif

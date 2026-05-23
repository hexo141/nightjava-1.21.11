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

JNI_EXPORT jobject JNICALL Java_com_nj_particle_ParticleNativeLib_getOrCreateNativeBuffer(
    JNIEnv* env, jclass clazz, jint minParticleCount);

JNI_EXPORT jint JNICALL Java_com_nj_particle_ParticleNativeLib_getNativeBufferCapacity(
    JNIEnv* env, jclass clazz);

JNI_EXPORT jboolean JNICALL Java_com_nj_particle_ParticleNativeLib_tickAllParticlesInPlace(
    JNIEnv* env, jclass clazz, jint particleCount);

JNI_EXPORT jboolean JNICALL Java_com_nj_particle_ParticleNativeLib_computeCameraRelativePositions(
    JNIEnv* env, jclass clazz,
    jobject vertexBuffer, jint particleCount, jint vertexStride,
    jfloat camX, jfloat camY, jfloat camZ,
    jfloat rotX, jfloat rotY, jfloat rotZ, jfloat rotW,
    jfloat tickDelta);

JNI_EXPORT jboolean JNICALL Java_com_nj_particle_ParticleNativeLib_computeBillboardVerticesNative(
    JNIEnv* env, jclass clazz,
    jobject vertexBuffer, jint particleCount, jint vertexStride,
    jfloat camX, jfloat camY, jfloat camZ,
    jfloat rotX, jfloat rotY, jfloat rotZ, jfloat rotW,
    jfloat tickDelta);

JNI_EXPORT jboolean JNICALL Java_com_nj_particle_ParticleNativeLib_batchTickParticles(
    JNIEnv* env, jclass clazz,
    jfloatArray positions, jfloatArray velocities,
    jintArray ages, jintArray maxAges, jint count,
    jfloat gravityStrength, jfloat velocityMultiplier);

JNI_EXPORT jboolean JNICALL Java_com_nj_particle_ParticleNativeLib_batchTickParticlesDirect(
    JNIEnv* env, jclass clazz,
    jobject buffer, jint particleCount, jint stride);

JNI_EXPORT jboolean JNICALL Java_com_nj_particle_ParticleNativeLib_batchTickParticlesFull(
    JNIEnv* env, jclass clazz,
    jobject buffer, jint particleCount, jint stride);

JNI_EXPORT jboolean JNICALL Java_com_nj_particle_ParticleNativeLib_applyPhysicsOnly(
    JNIEnv* env, jclass clazz,
    jobject buffer, jint particleCount, jint stride);

JNI_EXPORT jboolean JNICALL Java_com_nj_particle_ParticleNativeLib_tickSingleParticle(
    JNIEnv* env, jclass clazz, jobject buffer);

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
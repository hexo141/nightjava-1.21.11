#include <jni.h>
#include <cstring>
#include <cmath>

extern "C" {
    JNIEXPORT jboolean JNICALL Java_com_nj_particle_ParticleNativeLib_batchTickParticles(
        JNIEnv* env, jclass clazz,
        jfloatArray positions, jfloatArray velocities,
        jintArray ages, jintArray maxAges, jint count,
        jfloat gravityStrength, jfloat velocityMultiplier);

    JNIEXPORT jfloatArray JNICALL Java_com_nj_particle_ParticleNativeLib_computeBillboardVertices(
        JNIEnv* env, jclass clazz,
        jfloatArray particlePositions, jfloatArray particleSizes,
        jfloatArray particleColors, jfloatArray cameraRotation,
        jfloat tickProgress, jfloatArray cameraPosition, jint vertexCount);

    JNIEXPORT jboolean JNICALL Java_com_nj_particle_ParticleNativeLib_batchMoveParticles(
        JNIEnv* env, jclass clazz,
        jfloatArray positions, jfloatArray velocities,
        jint count, jfloatArray worldBounds);
}

struct ParticleData {
    float x, y, z;
    float vx, vy, vz;
    float size;
    int age, maxAge;
};

struct VertexData {
    float x, y, z;
    float r, g, b, a;
    float u, v;
    int light;
};

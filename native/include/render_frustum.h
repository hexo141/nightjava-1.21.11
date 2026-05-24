#pragma once

#include <jni.h>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jobject JNICALL Java_com_nj_render_RenderNative_getOrCreateFrustumBuffer(JNIEnv* env, jclass cls, jint maxEntities);
JNIEXPORT void JNICALL Java_com_nj_render_RenderNative_updateFrustumPlanes(JNIEnv* env, jclass cls, jfloatArray matrix, jdouble cx, jdouble cy, jdouble cz);
JNIEXPORT jint JNICALL Java_com_nj_render_RenderNative_batchFrustumCullEntities(JNIEnv* env, jclass cls, jint count);
JNIEXPORT jboolean JNICALL Java_com_nj_render_RenderNative_getVisibilityResult(JNIEnv* env, jclass cls, jint index);

#ifdef __cplusplus
}
#endif
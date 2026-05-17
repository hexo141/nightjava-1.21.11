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

JNIEXPORT jboolean JNICALL Java_com_nj_datafixer_DataFixerNative_convertChunkPalette(
    JNIEnv* env, jclass clazz,
    jlongArray blockStates,
    jobjectArray palette,
    jintArray outputBlockIds,
    jint blockCount);

JNIEXPORT jboolean JNICALL Java_com_nj_datafixer_DataFixerNative_convertNibbleArray(
    JNIEnv* env, jclass clazz,
    jbyteArray input,
    jbyteArray output,
    jint count);

JNIEXPORT jboolean JNICALL Java_com_nj_datafixer_DataFixerNative_convertBiomeArray(
    JNIEnv* env, jclass clazz,
    jintArray inputBiomes,
    jintArray outputSections,
    jint inputSize,
    jint sectionCount);

JNIEXPORT jboolean JNICALL Java_com_nj_datafixer_DataFixerNative_fixHeightmap(
    JNIEnv* env, jclass clazz,
    jlongArray inputHeightmap,
    jlongArray outputHeightmap,
    jint heightmapLength);

JNIEXPORT jobjectArray JNICALL Java_com_nj_datafixer_DataFixerNative_batchReplaceStrings(
    JNIEnv* env, jclass clazz,
    jobjectArray inputStrings,
    jobjectArray fromKeys,
    jobjectArray toValues,
    jint count);

JNIEXPORT jboolean JNICALL Java_com_nj_datafixer_DataFixerNative_applyBitStorageFix(
    JNIEnv* env, jclass clazz,
    jlongArray inputStorage,
    jint inputBitsPerEntry,
    jint outputBitsPerEntry,
    jlongArray outputStorage,
    jint entryCount);

JNIEXPORT jboolean JNICALL Java_com_nj_datafixer_DataFixerNative_convertPalettedChunkBlocks(
    JNIEnv* env, jclass clazz,
    jlongArray rawBlockStates,
    jint paletteSize,
    jobjectArray paletteEntries,
    jintArray outBlockIds,
    jint blockCount);

#ifdef __cplusplus
}
#endif

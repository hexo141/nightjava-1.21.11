#include "datafixer_native.h"
#include <cstring>
#include <unordered_map>
#include <vector>
#include <string>
#include <immintrin.h>
#include <thread>
#include <algorithm>

#ifdef _MSC_VER
#include <intrin.h>
#endif

// ============================================================
// Helper: Extract bits from packed long array
// ============================================================
static inline int extractBits(const uint64_t* storage, int index, int bitsPerEntry) {
    int bitIndex = index * bitsPerEntry;
    int longIndex = bitIndex / 64;
    int bitOffset = bitIndex % 64;
    uint64_t value;

    if (bitOffset + bitsPerEntry <= 64) {
        value = storage[longIndex];
    } else {
        value = storage[longIndex] >> bitOffset;
        if (longIndex + 1 < 1000) {
            value |= storage[longIndex + 1] << (64 - bitOffset);
        }
    }

    return static_cast<int>(value & ((1ULL << bitsPerEntry) - 1));
}

// ============================================================
// SIMD: Unpack block states using AVX2
// Optimizes ChunkPalettedStorageFix BlockStates processing
// ============================================================
static void unpackBlockStates_SIMD(const uint64_t* input, int* output,
                                    int bitsPerEntry, int count) {
    int i = 0;

#if defined(__AVX2__)
    uint64_t mask = (1ULL << bitsPerEntry) - 1;

    for (; i + 8 <= count; i += 8) {
        __m256i result = _mm256_setzero_si256();

        for (int j = 0; j < 8; j++) {
            int bitIndex = (i + j) * bitsPerEntry;
            int longIndex = bitIndex / 64;
            int bitOffset = bitIndex % 64;

            uint64_t raw;
            if (bitOffset + bitsPerEntry <= 64) {
                raw = input[longIndex];
            } else {
                raw = (input[longIndex] >> bitOffset) |
                      (input[longIndex + 1] << (64 - bitOffset));
            }

            int val = static_cast<int>(raw & mask);
            result = _mm256_insert_epi32(result, val, j);
        }

        _mm256_storeu_si256(reinterpret_cast<__m256i*>(output + i), result);
    }
#endif

    for (; i < count; i++) {
        output[i] = extractBits(input, i, bitsPerEntry);
    }
}

// ============================================================
// JNI: convertChunkPalette
// Optimizes ChunkPalettedStorageFix palette conversion
// ============================================================
JNIEXPORT jboolean JNICALL Java_com_nj_datafixer_DataFixerNative_convertChunkPalette(
    JNIEnv* env, jclass clazz,
    jlongArray blockStates,
    jobjectArray palette,
    jintArray outputBlockIds,
    jint blockCount)
{
    if (!blockStates || !outputBlockIds || !palette) {
        return JNI_FALSE;
    }

    jsize paletteSize = env->GetArrayLength(palette);
    jsize statesLength = env->GetArrayLength(blockStates);

    jlong* statesRaw = env->GetLongArrayElements(blockStates, nullptr);
    jint* outIds = env->GetIntArrayElements(outputBlockIds, nullptr);

    if (!statesRaw || !outIds) {
        if (statesRaw) env->ReleaseLongArrayElements(blockStates, statesRaw, JNI_ABORT);
        if (outIds) env->ReleaseIntArrayElements(outputBlockIds, outIds, JNI_ABORT);
        return JNI_FALSE;
    }

    const uint64_t* states = reinterpret_cast<const uint64_t*>(statesRaw);

    // Determine bits per entry
    int bitsPerEntry = 4;
    if (paletteSize > 16) bitsPerEntry = 5;
    if (paletteSize > 32) bitsPerEntry = 6;
    if (paletteSize > 64) bitsPerEntry = 7;
    if (paletteSize > 128) bitsPerEntry = 8;

    // Use SIMD to unpack
    std::vector<int> paletteIndices(blockCount);
    unpackBlockStates_SIMD(states, paletteIndices.data(), bitsPerEntry, blockCount);

    // Convert palette indices to block IDs
    jstring* paletteStrings = new jstring[paletteSize];
    for (int i = 0; i < paletteSize; i++) {
        paletteStrings[i] = (jstring)env->GetObjectArrayElement(palette, i);
    }

    for (int i = 0; i < blockCount; i++) {
        int paletteIdx = paletteIndices[i];
        if (paletteIdx >= 0 && paletteIdx < paletteSize) {
            const char* name = env->GetStringUTFChars(paletteStrings[paletteIdx], nullptr);
            outIds[i] = static_cast<int>(std::hash<std::string>{}(name) & 0x7FFFFFFF);
            env->ReleaseStringUTFChars(paletteStrings[paletteIdx], name);
        } else {
            outIds[i] = 0;
        }
    }

    env->ReleaseLongArrayElements(blockStates, statesRaw, JNI_ABORT);
    env->ReleaseIntArrayElements(outputBlockIds, outIds, 0);

    delete[] paletteStrings;
    return JNI_TRUE;
}

// ============================================================
// JNI: convertNibbleArray
// Optimizes ChunkNibbleArray nibble processing
// Uses SIMD to convert nibble arrays efficiently
// ============================================================
JNIEXPORT jboolean JNICALL Java_com_nj_datafixer_DataFixerNative_convertNibbleArray(
    JNIEnv* env, jclass clazz,
    jbyteArray input,
    jbyteArray output,
    jint count)
{
    if (!input || !output) {
        return JNI_FALSE;
    }

    jsize inputLen = env->GetArrayLength(input);
    jbyte* inBytes = env->GetByteArrayElements(input, nullptr);
    jbyte* outBytes = env->GetByteArrayElements(output, nullptr);

    if (!inBytes || !outBytes) {
        if (inBytes) env->ReleaseByteArrayElements(input, inBytes, JNI_ABORT);
        if (outBytes) env->ReleaseByteArrayElements(output, outBytes, JNI_ABORT);
        return JNI_FALSE;
    }

    int i = 0;

#if defined(__AVX2__)
    __m256i lowMask = _mm256_set1_epi8(0x0F);

    for (; i + 32 <= count; i += 32) {
        int byteIdx = i / 2;

        __m256i data = _mm256_loadu_si256(
            reinterpret_cast<const __m256i*>(inBytes + byteIdx));

        __m256i low = _mm256_and_si256(data, lowMask);
        __m256i high = _mm256_and_si256(_mm256_srli_epi16(data, 4), lowMask);

        // Interleave low and high nibbles
        for (int j = 0; j < 16; j++) {
            outBytes[i + j * 2] = ((int8_t*)&low)[j];
            outBytes[i + j * 2 + 1] = ((int8_t*)&high)[j];
        }
    }
#endif

    for (; i < count; i++) {
        int byteIdx = i / 2;
        int nibble = (i % 2 == 0) ? (inBytes[byteIdx] & 0x0F) : ((inBytes[byteIdx] >> 4) & 0x0F);
        outBytes[i] = static_cast<jbyte>(nibble);
    }

    env->ReleaseByteArrayElements(input, inBytes, JNI_ABORT);
    env->ReleaseByteArrayElements(output, outBytes, 0);
    return JNI_TRUE;
}

// ============================================================
// JNI: convertBiomeArray
// Optimizes ChunkHeightAndBiomeFix biome array conversion
// Converts 1024/1536 element array to 24 section arrays
// ============================================================
JNIEXPORT jboolean JNICALL Java_com_nj_datafixer_DataFixerNative_convertBiomeArray(
    JNIEnv* env, jclass clazz,
    jintArray inputBiomes,
    jintArray outputSections,
    jint inputSize,
    jint sectionCount)
{
    if (!inputBiomes || !outputSections || inputSize <= 0 || sectionCount <= 0) {
        return JNI_FALSE;
    }

    jint* inBiomes = env->GetIntArrayElements(inputBiomes, nullptr);
    jint* outSections = env->GetIntArrayElements(outputSections, nullptr);

    if (!inBiomes || !outSections) {
        if (inBiomes) env->ReleaseIntArrayElements(inputBiomes, inBiomes, JNI_ABORT);
        if (outSections) env->ReleaseIntArrayElements(outputSections, outSections, JNI_ABORT);
        return JNI_FALSE;
    }

    int elementsPerSection = 64;

    int i = 0;
    int section = 0;
    int elemInSection = 0;

#if defined(__AVX2__)
    for (; section < sectionCount && elemInSection + 8 <= elementsPerSection;) {
        __m256i biomeData = _mm256_loadu_si256(
            reinterpret_cast<const __m256i*>(inBiomes + i));
        _mm256_storeu_si256(
            reinterpret_cast<__m256i*>(outSections + section * elementsPerSection + elemInSection),
            biomeData);

        i += 8;
        elemInSection += 8;

        if (elemInSection >= elementsPerSection) {
            elemInSection = 0;
            section++;
        }
    }
#endif

    for (; section < sectionCount; section++) {
        int outOffset = section * elementsPerSection;
        int end = std::min(i + elementsPerSection, inputSize);

        for (int j = i; j < end && elemInSection < elementsPerSection; j++, elemInSection++) {
            outSections[outOffset + elemInSection] = inBiomes[j];
        }
        i = end;
        elemInSection = 0;
    }

    env->ReleaseIntArrayElements(inputBiomes, inBiomes, JNI_ABORT);
    env->ReleaseIntArrayElements(outputSections, outSections, 0);
    return JNI_TRUE;
}

// ============================================================
// JNI: fixHeightmap
// Optimizes heightmap bit conversion in ChunkHeightAndBiomeFix
// Uses 64-bit parallel processing
// ============================================================
JNIEXPORT jboolean JNICALL Java_com_nj_datafixer_DataFixerNative_fixHeightmap(
    JNIEnv* env, jclass clazz,
    jlongArray inputHeightmap,
    jlongArray outputHeightmap,
    jint heightmapLength)
{
    if (!inputHeightmap || !outputHeightmap || heightmapLength <= 0) {
        return JNI_FALSE;
    }

    jlong* inHm = env->GetLongArrayElements(inputHeightmap, nullptr);
    jlong* outHm = env->GetLongArrayElements(outputHeightmap, nullptr);

    if (!inHm || !outHm) {
        if (inHm) env->ReleaseLongArrayElements(inputHeightmap, inHm, JNI_ABORT);
        if (outHm) env->ReleaseLongArrayElements(outputHeightmap, outHm, JNI_ABORT);
        return JNI_FALSE;
    }

    const uint64_t* input = reinterpret_cast<const uint64_t*>(inHm);
    uint64_t* output = reinterpret_cast<uint64_t*>(outHm);

    int i = 0;

    for (; i + 4 <= heightmapLength; i += 4) {
        uint64_t in0 = input[i];
        uint64_t in1 = input[i + 1];
        uint64_t in2 = input[i + 2];
        uint64_t in3 = input[i + 3];

        // Extract 9-bit height values and pack into 8 64-bit words
        // Each output word contains 7 height values (9 bits each = 63 bits)
        for (int h = 0; h < 8; h++) {
            uint64_t out = 0;
            for (int v = 0; v < 7; v++) {
                int bitIdx = h * 7 + v;
                if (bitIdx < 64) {
                    int srcWord = (bitIdx * 9) / 64;
                    int srcBit = (bitIdx * 9) % 64;
                    uint64_t srcVal;

                    if (srcWord == 0) srcVal = in0;
                    else if (srcWord == 1) srcVal = in1;
                    else if (srcWord == 2) srcVal = in2;
                    else srcVal = in3;

                    uint64_t height = (srcVal >> srcBit) & 0x1FF;
                    out |= (height << (v * 9));
                }
            }
            output[i + h] = static_cast<jlong>(out);
        }
    }

    for (; i < heightmapLength; i++) {
        output[i] = input[i];
    }

    env->ReleaseLongArrayElements(inputHeightmap, inHm, JNI_ABORT);
    env->ReleaseLongArrayElements(outputHeightmap, outHm, 0);
    return JNI_TRUE;
}

// ============================================================
// JNI: batchReplaceStrings
// Optimizes bulk string replacement operations
// Used by various rename fixes (BlockNameFix, ItemNameFix, etc.)
// ============================================================
JNIEXPORT jobjectArray JNICALL Java_com_nj_datafixer_DataFixerNative_batchReplaceStrings(
    JNIEnv* env, jclass clazz,
    jobjectArray inputStrings,
    jobjectArray fromKeys,
    jobjectArray toValues,
    jint count)
{
    if (!inputStrings || !fromKeys || !toValues || count <= 0) {
        return inputStrings;
    }

    jint keysLen = env->GetArrayLength(fromKeys);
    jint valuesLen = env->GetArrayLength(toValues);

    if (keysLen != valuesLen) {
        return inputStrings;
    }

    // Build lookup table
    std::unordered_map<std::string, std::string> lookup;
    lookup.reserve(keysLen);

    for (int i = 0; i < keysLen; i++) {
        jstring keyStr = (jstring)env->GetObjectArrayElement(fromKeys, i);
        jstring valStr = (jstring)env->GetObjectArrayElement(toValues, i);

        const char* keyChars = env->GetStringUTFChars(keyStr, nullptr);
        const char* valChars = env->GetStringUTFChars(valStr, nullptr);

        lookup[std::string(keyChars)] = std::string(valChars);

        env->ReleaseStringUTFChars(keyStr, keyChars);
        env->ReleaseStringUTFChars(valStr, valChars);
    }

    // Create output array
    jclass stringClass = env->FindClass("java/lang/String");
    jobjectArray outputArray = env->NewObjectArray(count, stringClass, nullptr);

    // Process strings
    for (int i = 0; i < count; i++) {
        jstring inputStr = (jstring)env->GetObjectArrayElement(inputStrings, i);
        const char* chars = env->GetStringUTFChars(inputStr, nullptr);
        std::string inputStrCpp(chars);
        env->ReleaseStringUTFChars(inputStr, chars);

        auto it = lookup.find(inputStrCpp);
        if (it != lookup.end()) {
            jstring result = env->NewStringUTF(it->second.c_str());
            env->SetObjectArrayElement(outputArray, i, result);
        } else {
            env->SetObjectArrayElement(outputArray, i, inputStr);
        }
    }

    return outputArray;
}

// ============================================================
// JNI: applyBitStorageFix
// Optimizes BitStorageAlignFix bit storage conversion
// Converts between different bits-per-entry configurations
// ============================================================
JNIEXPORT jboolean JNICALL Java_com_nj_datafixer_DataFixerNative_applyBitStorageFix(
    JNIEnv* env, jclass clazz,
    jlongArray inputStorage,
    jint inputBitsPerEntry,
    jint outputBitsPerEntry,
    jlongArray outputStorage,
    jint entryCount)
{
    if (!inputStorage || !outputStorage || inputBitsPerEntry <= 0 || outputBitsPerEntry <= 0) {
        return JNI_FALSE;
    }

    jlong* inRaw = env->GetLongArrayElements(inputStorage, nullptr);
    jlong* outRaw = env->GetLongArrayElements(outputStorage, nullptr);

    if (!inRaw || !outRaw) {
        if (inRaw) env->ReleaseLongArrayElements(inputStorage, inRaw, JNI_ABORT);
        if (outRaw) env->ReleaseLongArrayElements(outputStorage, outRaw, JNI_ABORT);
        return JNI_FALSE;
    }

    const uint64_t* input = reinterpret_cast<const uint64_t*>(inRaw);
    uint64_t* output = reinterpret_cast<uint64_t*>(outRaw);

    uint64_t inputMask = (1ULL << inputBitsPerEntry) - 1;
    uint64_t outputMask = (1ULL << outputBitsPerEntry) - 1;

    // Multi-threaded conversion for large arrays
    const int CHUNK_SIZE = 256;
    int numThreads = std::min(static_cast<int>(std::thread::hardware_concurrency()), 4);

    if (entryCount > CHUNK_SIZE * numThreads) {
        std::vector<std::thread> threads;
        threads.reserve(numThreads);

        for (int t = 0; t < numThreads; t++) {
            int start = t * CHUNK_SIZE;
            int end = std::min(start + CHUNK_SIZE, entryCount);

            threads.emplace_back([&, start, end]() {
                uint64_t outBitPos = 0;

                for (int i = start; i < end; i++) {
                    // Extract value from input storage
                    int inBitPos = i * inputBitsPerEntry;
                    int inWordIdx = inBitPos / 64;
                    int inBitOffset = inBitPos % 64;

                    uint64_t value;
                    if (inBitOffset + inputBitsPerEntry <= 64) {
                        value = input[inWordIdx] >> inBitOffset;
                    } else {
                        value = (input[inWordIdx] >> inBitOffset) |
                                (input[inWordIdx + 1] << (64 - inBitOffset));
                    }
                    value &= inputMask;

                    // Write value to output storage
                    int outWordIdx = outBitPos / 64;
                    int outBitOffset = outBitPos % 64;

                    if (outBitOffset + outputBitsPerEntry <= 64) {
                        uint64_t old = output[outWordIdx];
                        uint64_t newBits = (old & ~(outputMask << outBitOffset)) |
                                          (value << outBitOffset);
                        output[outWordIdx] = newBits;
                    } else {
                        uint64_t old1 = output[outWordIdx];
                        uint64_t old2 = output[outWordIdx + 1];

                        output[outWordIdx] = old1 | (value << outBitOffset);
                        output[outWordIdx + 1] = old2 | (value >> (64 - outBitOffset));
                    }

                    outBitPos += outputBitsPerEntry;
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }
    } else {
        // Single-threaded for small arrays
        uint64_t outBitPos = 0;

        for (int i = 0; i < entryCount; i++) {
            int inBitPos = i * inputBitsPerEntry;
            int inWordIdx = inBitPos / 64;
            int inBitOffset = inBitPos % 64;

            uint64_t value;
            if (inBitOffset + inputBitsPerEntry <= 64) {
                value = input[inWordIdx] >> inBitOffset;
            } else {
                value = (input[inWordIdx] >> inBitOffset) |
                        (input[inWordIdx + 1] << (64 - inBitOffset));
            }
            value &= inputMask;

            int outWordIdx = outBitPos / 64;
            int outBitOffset = outBitPos % 64;

            if (outBitOffset + outputBitsPerEntry <= 64) {
                uint64_t old = output[outWordIdx];
                output[outWordIdx] = (old & ~(outputMask << outBitOffset)) |
                                    (value << outBitOffset);
            } else {
                output[outWordIdx] |= (value << outBitOffset);
                output[outWordIdx + 1] |= (value >> (64 - outBitOffset));
            }

            outBitPos += outputBitsPerEntry;
        }
    }

    env->ReleaseLongArrayElements(inputStorage, inRaw, JNI_ABORT);
    env->ReleaseLongArrayElements(outputStorage, outRaw, 0);
    return JNI_TRUE;
}

// ============================================================
// JNI: convertPalettedChunkBlocks
// Optimizes full paletted chunk block conversion
// Combines palette unpacking and block ID mapping
// ============================================================
JNIEXPORT jboolean JNICALL Java_com_nj_datafixer_DataFixerNative_convertPalettedChunkBlocks(
    JNIEnv* env, jclass clazz,
    jlongArray rawBlockStates,
    jint paletteSize,
    jobjectArray paletteEntries,
    jintArray outBlockIds,
    jint blockCount)
{
    if (!rawBlockStates || !paletteEntries || !outBlockIds) {
        return JNI_FALSE;
    }

    jlong* statesRaw = env->GetLongArrayElements(rawBlockStates, nullptr);
    jint* outIds = env->GetIntArrayElements(outBlockIds, nullptr);

    if (!statesRaw || !outIds) {
        if (statesRaw) env->ReleaseLongArrayElements(rawBlockStates, statesRaw, JNI_ABORT);
        if (outIds) env->ReleaseIntArrayElements(outBlockIds, outIds, JNI_ABORT);
        return JNI_FALSE;
    }

    const uint64_t* states = reinterpret_cast<const uint64_t*>(statesRaw);

    // Auto-detect bits per entry based on palette size
    int bitsPerEntry = 4;
    if (paletteSize > 16) bitsPerEntry = 5;
    if (paletteSize > 32) bitsPerEntry = 6;
    if (paletteSize > 64) bitsPerEntry = 7;
    if (paletteSize > 128) bitsPerEntry = 8;
    if (paletteSize > 256) bitsPerEntry = 9;

    // Pre-cache palette strings
    std::vector<std::string> paletteCache(paletteSize);
    for (int i = 0; i < paletteSize; i++) {
        jstring s = (jstring)env->GetObjectArrayElement(paletteEntries, i);
        const char* chars = env->GetStringUTFChars(s, nullptr);
        paletteCache[i] = chars;
        env->ReleaseStringUTFChars(s, chars);
    }

    // SIMD unpack + mapping
    std::vector<int> indices(blockCount);
    unpackBlockStates_SIMD(states, indices.data(), bitsPerEntry, blockCount);

    // Map palette indices to block IDs
    for (int i = 0; i < blockCount; i++) {
        int idx = indices[i];
        if (idx >= 0 && idx < paletteSize) {
            // Simple hash-based ID (can be replaced with actual block ID mapping)
            const std::string& name = paletteCache[idx];
            uint32_t hash = 0;
            for (char c : name) {
                hash = hash * 31 + static_cast<uint32_t>(c);
            }
            outIds[i] = static_cast<jint>(hash & 0x7FFFFFFF);
        } else {
            outIds[i] = 0; // Air
        }
    }

    env->ReleaseLongArrayElements(rawBlockStates, statesRaw, JNI_ABORT);
    env->ReleaseIntArrayElements(outBlockIds, outIds, 0);
    return JNI_TRUE;
}

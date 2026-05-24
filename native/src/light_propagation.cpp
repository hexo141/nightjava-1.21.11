#include "light_native.h"
#include <cstring>
#include <emmintrin.h>
#include <tmmintrin.h>

static constexpr int NIBBLES_PER_SECTION = 4096;
static constexpr int BYTES_PER_SECTION = 2048;
static constexpr int SECTION_SIZE = 16;

static inline int nibbleIndex(int x, int y, int z) {
    return (y << 8) | (z << 4) | x;
}

static inline int nibbleGet(const uint8_t* bytes, int index) {
    return (bytes[index >> 1] >> ((index & 1) << 2)) & 0xF;
}

static inline void nibbleSet(uint8_t* bytes, int index, int value) {
    int byteIdx = index >> 1;
    int shift = (index & 1) << 2;
    bytes[byteIdx] = (bytes[byteIdx] & ~(0xF << shift)) | ((value & 0xF) << shift);
}

static inline uint8_t packNibble(int value) {
    uint8_t b = (uint8_t)(value & 0xF);
    b |= (uint8_t)((value & 0xF) << 4);
    return b;
}

void nibble_bulk_copy(uint8_t* dst, const uint8_t* src, int byteCount) {
    int i = 0;
    for (; i + 15 < byteCount; i += 16) {
        __m128i v = _mm_loadu_si128((const __m128i*)(src + i));
        _mm_storeu_si128((__m128i*)(dst + i), v);
    }
    for (; i < byteCount; i++) {
        dst[i] = src[i];
    }
}

void nibble_bulk_fill(uint8_t* buf, int value, int byteCount) {
    uint8_t packed = packNibble(value);
    int i = 0;
    __m128i v = _mm_set1_epi8((char)packed);
    for (; i + 15 < byteCount; i += 16) {
        _mm_storeu_si128((__m128i*)(buf + i), v);
    }
    for (; i < byteCount; i++) {
        buf[i] = packed;
    }
}

int nibble_bulk_diff(const uint8_t* a, const uint8_t* b, int byteCount,
                      int* outX, int* outY, int* outZ, int maxDiffs) {
    int diffCount = 0;
    for (int idx = 0; idx < NIBBLES_PER_SECTION && diffCount < maxDiffs; idx++) {
        int va = nibbleGet(a, idx);
        int vb = nibbleGet(b, idx);
        if (va != vb) {
            outX[diffCount] = idx & 0xF;
            outY[diffCount] = (idx >> 8) & 0xF;
            outZ[diffCount] = (idx >> 4) & 0xF;
            diffCount++;
        }
    }
    return diffCount;
}

static const int DX[6] = { -1, 1, 0, 0, 0, 0 };
static const int DY[6] = { 0, 0, -1, 1, 0, 0 };
static const int DZ[6] = { 0, 0, 0, 0, -1, 1 };
static const int OPPOSITE_DIR[6] = { 1, 0, 3, 2, 5, 4 };

struct LightQueue {
    static constexpr int MAX_QUEUE = 65536;
    int positions[MAX_QUEUE];
    int flags[MAX_QUEUE];
    int head;
    int tail;

    void clear() { head = 0; tail = 0; }
    bool empty() const { return head >= tail; }
    void push(int pos, int flag) {
        positions[tail] = pos;
        flags[tail] = flag;
        tail++;
    }
    int popPos() { return positions[head]; }
    int popFlag() { return flags[head++]; }
};

static bool inSection(int x, int y, int z) {
    return (unsigned)x < 16 && (unsigned)y < 16 && (unsigned)z < 16;
}

static int getOpacity(const uint8_t* opacityGrid, int x, int y, int z) {
    int idx = nibbleIndex(x, y, z);
    return nibbleGet(opacityGrid, idx);
}

static int getSourceLight(const uint8_t* sourceGrid, int x, int y, int z) {
    int idx = nibbleIndex(x, y, z);
    return nibbleGet(sourceGrid, idx);
}

int propagate_block_light_internal(
    uint8_t* lightBytes, const uint8_t* opacityGrid, const uint8_t* sourceLuminance,
    const jlong* queuePositions, const jlong* queueFlags,
    int decreaseCount, int increaseCount)
{
    LightQueue decQ, incQ;
    decQ.clear();
    incQ.clear();

    for (int i = 0; i < decreaseCount; i++) {
        decQ.push((int)queuePositions[i], (int)queueFlags[i]);
    }
    for (int i = 0; i < increaseCount; i++) {
        incQ.push((int)queuePositions[decreaseCount + i], (int)queueFlags[decreaseCount + i]);
    }

    int propagateCount = 0;

    while (!decQ.empty()) {
        int pos = decQ.popPos();
        int flag = decQ.popFlag();
        int lightLevel = flag & 0xF;
        int x = pos & 0xF;
        int y = (pos >> 8) & 0xF;
        int z = (pos >> 4) & 0xF;

        for (int d = 0; d < 6; d++) {
            if (!((flag >> (4 + d)) & 1)) continue;
            int nx = x + DX[d];
            int ny = y + DY[d];
            int nz = z + DZ[d];
            if (!inSection(nx, ny, nz)) continue;

            int nidx = nibbleIndex(nx, ny, nz);
            int neighborLight = nibbleGet(lightBytes, nidx);
            if (neighborLight == 0) continue;

            if (neighborLight <= lightLevel - 1) {
                int srcLight = getSourceLight(sourceLuminance, nx, ny, nz);
                nibbleSet(lightBytes, nidx, 0);
                propagateCount++;
                if (srcLight < neighborLight) {
                    int newFlag = neighborLight | (0x3F << 4);
                    newFlag &= ~(1 << (4 + OPPOSITE_DIR[d]));
                    decQ.push(nidx, newFlag);
                }
                if (srcLight > 0) {
                    incQ.push(nidx, (srcLight & 0xF) | (0x3F << 4) | (1 << 11));
                }
            } else {
                incQ.push(nidx, (neighborLight & 0xF) | (1 << (4 + OPPOSITE_DIR[d])));
            }
        }
    }

    while (!incQ.empty()) {
        int pos = incQ.popPos();
        int flag = incQ.popFlag();
        int lightLevel = flag & 0xF;
        bool forceSet = (flag >> 11) & 1;
        int x = pos & 0xF;
        int y = (pos >> 8) & 0xF;
        int z = (pos >> 4) & 0xF;

        int currentLight = nibbleGet(lightBytes, nibbleIndex(x, y, z));
        if (forceSet && currentLight < lightLevel) {
            nibbleSet(lightBytes, nibbleIndex(x, y, z), lightLevel);
            currentLight = lightLevel;
            propagateCount++;
        }
        if (currentLight != lightLevel) continue;

        for (int d = 0; d < 6; d++) {
            if (!((flag >> (4 + d)) & 1)) continue;
            int nx = x + DX[d];
            int ny = y + DY[d];
            int nz = z + DZ[d];
            if (!inSection(nx, ny, nz)) continue;

            int nidx = nibbleIndex(nx, ny, nz);
            int neighborLight = nibbleGet(lightBytes, nidx);
            int targetLight = lightLevel - 1;
            if (targetLight > neighborLight) {
                int op = getOpacity(opacityGrid, nx, ny, nz);
                int k = lightLevel - op;
                if (k > neighborLight) {
                    nibbleSet(lightBytes, nidx, k);
                    propagateCount++;
                    if (k > 1) {
                        int newFlag = k | (0x3F << 4);
                        newFlag &= ~(1 << (4 + OPPOSITE_DIR[d]));
                        incQ.push(nidx, newFlag);
                    }
                }
            }
        }
    }

    return propagateCount;
}

int propagate_sky_light_internal(
    uint8_t* lightBytes, const uint8_t* opacityGrid,
    const jlong* queuePositions, const jlong* queueFlags,
    int decreaseCount, int increaseCount, int sectionsBelow)
{
    LightQueue decQ, incQ;
    decQ.clear();
    incQ.clear();

    for (int i = 0; i < decreaseCount; i++) {
        decQ.push((int)queuePositions[i], (int)queueFlags[i]);
    }
    for (int i = 0; i < increaseCount; i++) {
        incQ.push((int)queuePositions[decreaseCount + i], (int)queueFlags[decreaseCount + i]);
    }

    int propagateCount = 0;

    while (!decQ.empty()) {
        int pos = decQ.popPos();
        int flag = decQ.popFlag();
        int lightLevel = flag & 0xF;
        int x = pos & 0xF;
        int y = (pos >> 8) & 0xF;
        int z = (pos >> 4) & 0xF;

        for (int d = 0; d < 6; d++) {
            if (!((flag >> (4 + d)) & 1)) continue;
            int nx = x + DX[d];
            int ny = y + DY[d];
            int nz = z + DZ[d];
            if (!inSection(nx, ny, nz)) continue;

            int nidx = nibbleIndex(nx, ny, nz);
            int neighborLight = nibbleGet(lightBytes, nidx);
            if (neighborLight == 0) continue;

            if (neighborLight <= lightLevel - 1) {
                nibbleSet(lightBytes, nidx, 0);
                propagateCount++;
                int newFlag = neighborLight | (0x3F << 4);
                newFlag &= ~(1 << (4 + OPPOSITE_DIR[d]));
                decQ.push(nidx, newFlag);
            } else {
                incQ.push(nidx, (neighborLight & 0xF) | (1 << (4 + OPPOSITE_DIR[d])));
            }
        }
    }

    while (!incQ.empty()) {
        int pos = incQ.popPos();
        int flag = incQ.popFlag();
        int lightLevel = flag & 0xF;
        bool forceSet = (flag >> 11) & 1;
        int x = pos & 0xF;
        int y = (pos >> 8) & 0xF;
        int z = (pos >> 4) & 0xF;

        int currentLight = nibbleGet(lightBytes, nibbleIndex(x, y, z));
        if (forceSet && currentLight < lightLevel) {
            nibbleSet(lightBytes, nibbleIndex(x, y, z), lightLevel);
            currentLight = lightLevel;
            propagateCount++;
        }
        if (currentLight != lightLevel) continue;

        for (int d = 0; d < 6; d++) {
            if (!((flag >> (4 + d)) & 1)) continue;
            int nx = x + DX[d];
            int ny = y + DY[d];
            int nz = z + DZ[d];
            if (!inSection(nx, ny, nz)) continue;

            int nidx = nibbleIndex(nx, ny, nz);
            int neighborLight = nibbleGet(lightBytes, nidx);
            int targetLight = lightLevel - 1;
            if (targetLight > neighborLight) {
                int op = getOpacity(opacityGrid, nx, ny, nz);
                int k = lightLevel - op;
                if (k > neighborLight) {
                    nibbleSet(lightBytes, nidx, k);
                    propagateCount++;
                    if (k > 1) {
                        int newFlag = k | (0x3F << 4);
                        newFlag &= ~(1 << (4 + OPPOSITE_DIR[d]));
                        incQ.push(nidx, newFlag);
                    }

                    if (sectionsBelow > 0 && d == 1 && ny == 0) {
                        for (int sy = 1; sy < sectionsBelow; sy++) {
                            int belowIdx = nibbleIndex(nx, sy, nz);
                            nibbleSet(lightBytes, belowIdx, k);
                        }
                    }
                }
            }
        }
    }

    return propagateCount;
}
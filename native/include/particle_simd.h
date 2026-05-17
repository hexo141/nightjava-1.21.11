#pragma once

#include <immintrin.h>
#include <cstddef>

#ifdef _MSC_VER
#define ALIGN32 __declspec(align(32))
#else
#define ALIGN32 __attribute__((aligned(32)))
#endif

#define SIMD_WIDTH 8

void simd_gravity_apply(float* velocities, int count, float gravityStrength);
void simd_velocity_multiply(float* velocities, int count, float multiplier);
void simd_position_update(float* positions, const float* velocities, int count);
void simd_age_increment(int* ages, int count);

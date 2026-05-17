# DataFixer C++ 优化方案

## 一、概述

Minecraft 的 DataFixer 系统在加载旧版本存档时执行大量数据转换操作，存在以下性能瓶颈：
- 大量 Java 对象分配（Dynamic、Typed、Pair 等）导致 GC 压力
- 位操作和数组处理效率低
- 字符串操作开销大
- 单线程限制（`Schemas.optimize()` 使用单线程 Executor）

本方案通过 JNI 将核心计算密集型操作卸载到 C++，利用 SIMD 指令、直接内存操作和多线程并行处理来提升性能。

## 二、需要 C++ 优化的模块及优先级

### 优先级 1（核心性能瓶颈）

| 模块 | 文件 | 预期性能提升 | 优化点 |
|------|------|------------|--------|
| ChunkPalettedStorageFix | ChunkPalettedStorageFix.java (1061行) | 5-10x | 位操作、NibbleArray 处理、Palette 映射 |
| ChunkHeightAndBiomeFix | ChunkHeightAndBiomeFix.java (554行) | 3-5x | 生物群系数组转换、Heightmap 位运算 |

### 优先级 2（显著性能改善）

| 模块 | 文件 | 预期性能提升 | 优化点 |
|------|------|------------|--------|
| ItemStackComponentizationFix | ItemStackComponentizationFix.java (824行) | 3-5x | 字符串解析、NBT 转换 |
| Schemas.optimize() | Schemas.java (optimize 方法) | 2-3x（启动时间） | 多线程并行优化 |

### 优先级 3（可选优化）

| 模块 | 文件 | 预期性能提升 | 优化点 |
|------|------|------------|--------|
| TextFixes | TextFixes.java | 2-3x | JSON 解析 |
| BlockStateFlattening | BlockStateFlattening.java | 2-3x | 查找表优化 |

## 三、实现方案

### 步骤 1：扩展 Native 库 - 添加 DataFixer JNI 接口

**修改文件：**
- `native/include/datafixer_native.h` - 新增头文件
- `native/src/datafixer_native.cpp` - 新增实现文件
- `native/CMakeLists.txt` - 添加新源文件

**JNI 接口设计：**

```cpp
// datafixer_native.h
extern "C" {
    // ChunkPalettedStorageFix 相关
    JNIEXPORT jboolean JNICALL Java_com_nj_datafixer_DataFixerNative_convertChunkPalette(
        JNIEnv* env, jclass clazz,
        jlongArray blockStates,    // 原始 BlockStates 数组
        jobjectArray palette,       // Palette 条目
        jintArray outputBlockIds,   // 输出方块 ID 映射
        jint blockCount             // 方块数量（通常 4096）
    );

    // ChunkNibbleArray 处理
    JNIEXPORT jboolean JNICALL Java_com_nj_datafixer_DataFixerNative_convertNibbleArray(
        JNIEnv* env, jclass clazz,
        jbyteArray input,           // 输入 nibble 数组
        jbyteArray output,          // 输出 nibble 数组
        jint count                  // 元素数量
    );

    // ChunkHeightAndBiomeFix 相关
    JNIEXPORT jboolean JNICALL Java_com_nj_datafixer_DataFixerNative_convertBiomeArray(
        JNIEnv* env, jclass clazz,
        jintArray inputBiomes,      // 输入生物群系数组 (1024/1536)
        jintArray outputSections,   // 输出分段生物群系
        jint inputSize,             // 输入大小
        jint sectionCount           // 输出段数
    );

    // Heightmap 转换
    JNIEXPORT jboolean JNICALL Java_com_nj_datafixer_DataFixerNative_fixHeightmap(
        JNIEnv* env, jclass clazz,
        jlongArray inputHeightmap,  // 输入高度图
        jlongArray outputHeightmap, // 输出高度图
        jint heightmapLength        // 高度图长度
    );

    // 批量字符串替换优化
    JNIEXPORT jobjectArray JNICALL Java_com_nj_datafixer_DataFixerNative_batchReplaceStrings(
        JNIEnv* env, jclass clazz,
        jobjectArray inputStrings,  // 输入字符串数组
        jobjectArray fromKeys,      // 查找键
        jobjectArray toValues,      // 替换值
        jint count                  // 数量
    );
}
```

### 步骤 2：创建 Kotlin 接口层

**新增文件：** `src/main/kotlin/com/nj/datafixer/DataFixerNative.kt`

```kotlin
package com.nj.datafixer

object DataFixerNative {
    @JvmStatic external fun convertChunkPalette(
        blockStates: LongArray,
        palette: Array<String>,
        outputBlockIds: IntArray,
        blockCount: Int
    ): Boolean

    @JvmStatic external fun convertNibbleArray(
        input: ByteArray,
        output: ByteArray,
        count: Int
    ): Boolean

    @JvmStatic external fun convertBiomeArray(
        inputBiomes: IntArray,
        outputSections: IntArray,
        inputSize: Int,
        sectionCount: Int
    ): Boolean

    @JvmStatic external fun fixHeightmap(
        inputHeightmap: LongArray,
        outputHeightmap: LongArray,
        heightmapLength: Int
    ): Boolean

    @JvmStatic external fun batchReplaceStrings(
        inputStrings: Array<String>,
        fromKeys: Array<String>,
        toValues: Array<String>,
        count: Int
    ): Array<String>

    @JvmField
    var isAvailable = false
}
```

### 步骤 3：创建 Mixin 拦截器

**新增文件：** `src/main/kotlin/com/nj/mixin/DataFixerMixin.kt`

```kotlin
package com.nj.mixin

// 针对 ChunkPalettedStorageFix.Level 的 Mixin
@Mixin(ChunkPalettedStorageFix.Level::class)
abstract class ChunkPalettedStorageFixMixin {
    @Inject(method = "transform", at = @At("HEAD"), cancellable = true)
    private fun onTransform(ci: CallbackInfoReturnable<Dynamic<*>>) {
        if (DataFixerNative.isAvailable) {
            // 使用 C++ 原生实现
            ci.setReturnValue(transformNative())
        }
    }

    private fun transformNative(): Dynamic<*> {
        // 调用 Native 方法
        return DataFixerNative.convertChunkPalette(...)
    }
}

// 针对 ChunkHeightAndBiomeFix 的 Mixin
@Mixin(ChunkHeightAndBiomeFix::class)
abstract class ChunkHeightAndBiomeFixMixin {
    @Inject(method = "fixBiomes", at = @At("HEAD"), cancellable = true)
    private fun onFixBiomes(ci: CallbackInfoReturnable<Array<Dynamic<*>>>) {
        if (DataFixerNative.isAvailable) {
            ci.setReturnValue(fixBiomesNative())
        }
    }

    @Inject(method = "fixHeightmap", at = @At("HEAD"), cancellable = true)
    private fun onFixHeightmap(ci: CallbackInfoReturnable<Dynamic<*>>) {
        if (DataFixerNative.isAvailable) {
            ci.setReturnValue(fixHeightmapNative())
        }
    }
}
```

### 步骤 4：实现 C++ 核心逻辑

**新增文件：** `native/src/datafixer_native.cpp`

**关键实现要点：**

1. **ChunkPalettedStorageFix 优化：**
   - 使用 SIMD (AVX2/AVX-512) 并行处理 BlockStates 的位解包
   - 使用紧凑的哈希表替代 Java 的 Int2ObjectMap
   - 直接内存操作避免 Java 对象分配

2. **ChunkHeightAndBiomeFix 优化：**
   - 使用 SIMD 并行处理生物群系数组转换
   - Heightmap 位操作使用 64 位并行处理
   - 预计算映射表避免重复查找

3. **字符串替换优化：**
   - 使用完美哈希表加速字符串匹配
   - 批量处理减少 JNI 调用开销

### 步骤 5：更新 NativeLoader 加载 DataFixer 库

**修改文件：** `src/main/kotlin/com/nj/NativeLoader.kt`

```kotlin
// 在 loadNativeLibrary() 中添加：
DataFixerNative.isAvailable = try {
    // 验证 DataFixer native 方法可用
    DataFixerNative::class.java.getDeclaredMethod("convertChunkPalette")
    true
} catch (e: Exception) {
    false
}
```

### 步骤 6：注册 Mixin

**修改文件：** `src/main/resources/nightjava.mixins.json`

```json
{
  "mixins": [
    "ExampleMixin",
    "ParticleMixin",
    "BillboardParticleRendererMixin",
    "ChunkPalettedStorageFixMixin",
    "ChunkHeightAndBiomeFixMixin"
  ]
}
```

### 步骤 7：更新 NightJava 主类日志

**修改文件：** `src/main/kotlin/com/nj/NightJava.kt`

```kotlin
override fun onInitialize() {
    NativeLoader.loadNativeLibrary()
    if (NativeLoader.isLoaded) {
        logger.info("NightJava mod initialized with native particle acceleration")
        if (DataFixerNative.isAvailable) {
            logger.info("DataFixer native acceleration enabled")
        }
    } else {
        logger.info("NightJava mod initialized (Java fallback mode)")
    }
}
```

## 四、C++ 实现关键技术点

### 1. SIMD 位操作（ChunkPalettedStorageFix）

```cpp
// 使用 AVX2 并行解包 BlockStates
void unpackBlockStates_AVX2(const uint64_t* input, int* output, int bitsPerBlock, int count) {
    uint64_t mask = (1ULL << bitsPerBlock) - 1;
    int i = 0;
    
    // SIMD 处理
    for (; i + 4 <= count; i += 4) {
        __m256i indices = _mm256_set_epi64x(
            i + 3, i + 2, i + 1, i
        );
        // ... SIMD 位操作
    }
    
    // 标量处理剩余
    for (; i < count; i++) {
        output[i] = extractBits(input, i, bitsPerBlock);
    }
}
```

### 2. 批量字符串替换

```cpp
// 使用高效哈希表进行批量字符串替换
jobjectArray batchReplaceStrings(JNIEnv* env, jobjectArray input, 
                                  jobjectArray keys, jobjectArray values, jint count) {
    // 构建查找表
    std::unordered_map<std::string, std::string> lookup;
    for (int i = 0; i < env->GetArrayLength(keys); i++) {
        lookup[jstring_to_cpp(env, (jstring)env->GetObjectArrayElement(keys, i))] =
               jstring_to_cpp(env, (jstring)env->GetObjectArrayElement(values, i));
    }
    
    // 批量替换
    for (int i = 0; i < count; i++) {
        std::string s = jstring_to_cpp(env, (jstring)env->GetObjectArrayElement(input, i));
        auto it = lookup.find(s);
        if (it != lookup.end()) {
            env->SetObjectArrayElement(output, i, cpp_to_jstring(env, it->second));
        }
    }
    return output;
}
```

## 五、文件结构变更

```
nightjava-1.21.11/
├── native/
│   ├── include/
│   │   ├── particle_native.h      (已有)
│   │   ├── particle_simd.h        (已有)
│   │   └── datafixer_native.h     (新增)
│   └── src/
│       ├── particle_native.cpp    (已有)
│       ├── particle_physics.cpp   (已有)
│       ├── particle_render.cpp    (已有)
│       └── datafixer_native.cpp   (新增)
├── src/main/kotlin/com/nj/
│   ├── NativeLoader.kt            (修改)
│   ├── NightJava.kt               (修改)
│   ├── particle/
│   │   └── ParticleNativeLib.kt   (已有)
│   ├── datafixer/
│   │   └── DataFixerNative.kt     (新增)
│   └── mixin/
│       ├── ExampleMixin.java      (已有)
│       ├── ParticleMixin.java     (已有)
│       ├── BillboardParticleRendererMixin.java (已有)
│       ├── ChunkPalettedStorageFixMixin.kt      (新增)
│       └── ChunkHeightAndBiomeFixMixin.kt       (新增)
└── src/main/resources/
    └── nightjava.mixins.json      (修改)
```

## 六、验证与测试

1. 编译 native 库：使用 CMake 构建
2. 测试 Mixin 是否正确注入
3. 对比优化前后的存档加载时间
4. 验证转换结果的正确性（与 Java 实现输出一致）

## 七、预期收益

| 场景 | 优化前 | 优化后 | 提升 |
|------|--------|--------|------|
| 1.16 存档加载 | ~15s | ~5s | 3x |
| 1.12 存档加载 | ~30s | ~8s | 3.75x |
| 大量方块转换 | ~5s | ~1s | 5x |

# Minecraft 粒子计算 C++ DLL 优化方案

## 项目概述

通过 JNI 将 Minecraft 原版粒子系统中计算密集型的操作转移到 C++ DLL 执行，利用 C++ 的原生性能和 SIMD 向量化指令大幅提升粒子计算效率。

---

## 目标优化区域

基于对 `e:\mc_source_code` 的分析，以下是最耗性能的粒子计算区域：

### 1. 粒子物理更新 (优先级: 高)
- **原版位置**: `Particle.tick()` 和 `Particle.move()`
- **计算内容**: 重力应用、速度衰减、碰撞检测
- **优化方式**: 批量向量化计算，利用 SIMD 同时处理多个粒子

### 2. Billboard 渲染变换 (优先级: 高)
- **原版位置**: `BillboardParticleSubmittable.render()` 和 `drawFace()`
- **计算内容**: 四元数旋转、顶点插值、矩阵变换
- **优化方式**: C++ 中批量计算 4 顶点/粒子，使用 SIMD 四元数运算

### 3. 发射器粒子批量生成 (优先级: 中)
- **原版位置**: `FireworksSparkParticle.explodeBall()`, `ExplosionEmitterParticle.tick()`
- **计算内容**: 嵌套循环创建大量子粒子
- **优化方式**: C++ 中预计算批量粒子初始参数

---

## 实现步骤

### 步骤 1: 创建 C++ DLL 项目

#### 1.1 目录结构
在 `e:\nightjava-1.21.11` 下创建 `native/` 目录:

```
native/
├── CMakeLists.txt                    # CMake 构建配置
├── include/
│   ├── particle_native.h             # JNI 接口声明
│   └── particle_simd.h               # SIMD 计算头文件
├── src/
│   ├── particle_native.cpp           # JNI 接口实现
│   ├── particle_simd.cpp             # SIMD 优化计算实现
│   ├── particle_physics.cpp          # 粒子物理批量计算
│   └── particle_render.cpp           # 粒子渲染批量计算
└── build/                            # 编译输出目录
```

#### 1.2 CMakeLists.txt 配置
- 使用 C++17
- 启用 MSVC/MinGW 的 AVX2 指令集优化
- 生成 `nightjava-particles.dll`
- 配置 JNI 头文件路径（指向 JDK 21 include 目录）

### 步骤 2: 设计 JNI 接口

#### 2.1 粒子数据结构 (Java 端)
创建 `ParticleBatch.java` 作为批量数据传输结构:
```kotlin
// 使用 FloatBuffer/IntBuffer 进行零拷贝数据传输
data class ParticleBatch(
    val positions: FloatBuffer,   // x,y,z per particle
    val velocities: FloatBuffer,  // vx,vy,vz per particle
    val ages: IntBuffer,          // age per particle
    val maxAges: IntBuffer,       // maxAge per particle
    val count: Int                // 实际粒子数
)
```

#### 2.2 JNI 接口声明
```kotlin
object ParticleNativeLib {
    // 批量物理更新
    external fun batchTickParticles(
        positions: FloatArray,
        velocities: FloatArray,
        ages: IntArray,
        maxAges: IntArray,
        count: Int,
        gravityStrength: Float,
        velocityMultiplier: Float
    ): Boolean

    // 批量渲染顶点计算
    external fun computeBillboardVertices(
        particlePositions: FloatArray,
        particleSizes: FloatArray,
        particleColors: FloatArray,
        cameraRotation: FloatArray, // 4 floats quaternion
        tickProgress: Float,
        cameraPosition: FloatArray,
        vertexCount: Int
    ): FloatArray

    // 批量碰撞检测
    external fun batchMoveParticles(
        positions: FloatArray,
        velocities: FloatArray,
        count: Int,
        worldBounds: FloatArray
    ): Boolean
}
```

### 步骤 3: 实现 C++ SIMD 计算

#### 3.1 粒子物理批量计算 (`particle_physics.cpp`)
- 使用 AVX2 指令集同时处理 8 个粒子
- 核心计算:
  ```cpp
  // 每个粒子: velocityY -= 0.04f * gravityStrength
  // velocity *= velocityMultiplier
  // position += velocity
  ```
- 使用 `_mm256_load_ps`, `_mm256_mul_ps` 等内联函数

#### 3.2 渲染顶点计算 (`particle_render.cpp`)
- 批量计算 Billboard 四元数旋转
- 每个粒子 4 顶点 * 3 坐标 = 12 floats，用 SIMD 并行处理
- 避免 Java 端频繁的 `new Vector3f().rotate()` 对象创建

#### 3.3 碰撞检测简化 (`particle_simd.cpp`)
- 简化的 AABB 碰撞检测
- 批量处理位置修正

### 步骤 4: 在 Fabric Mod 中集成 DLL 加载

#### 4.1 修改 `NightJava.kt` 入口
在 `onInitialize()` 中添加原生库加载逻辑:
- 检测操作系统和架构
- 从 JAR 资源中提取 DLL 到临时目录
- 使用 `System.load()` 加载
- 注册 native 方法

#### 4.2 DLL 资源存放
```
src/main/resources/
└── natives/
    └── windows/
        └── x64/
            └── nightjava-particles.dll
```

#### 4.3 加载回退机制
- 如果 DLL 加载失败，记录警告日志
- 自动回退到原版 Java 计算
- 不影响模组稳定性

### 步骤 5: 创建 Mixin 注入粒子计算

#### 5.1 注入 `ParticleManager.tick()`
- 在 `ParticleManager` 的 `tick()` 方法中，拦截粒子更新逻辑
- 收集所有活跃粒子数据到 `ParticleBatch`
- 调用 `ParticleNativeLib.batchTickParticles()`
- 将计算结果写回粒子对象

#### 5.2 注入 `BillboardParticleRenderer.render()`
- 拦截批量渲染调用
- 将粒子位置、大小、颜色数据传递给 C++
- C++ 计算后的顶点数据直接用于渲染

#### 5.3 性能监控
- 在 `ParticleRenderStatsDebugHudEntry` 中添加 native 计算时间统计
- F3 调试屏幕显示 native 加速效果

### 步骤 6: Gradle 构建集成

#### 6.1 修改 `build.gradle.kts`
- 添加编译 DLL 的任务（可选：调用 CMake）
- 确保 DLL 被包含在最终 JAR 中
- 配置 `processResources` 不排除 `.dll` 文件

#### 6.2 开发环境配置
- 配置 `runClient` 任务加载本地 DLL
- 支持热重载 DLL（开发调试用）

---

## 文件清单

### 需要创建的文件

| 文件路径 | 说明 |
|---------|------|
| `native/CMakeLists.txt` | C++ 项目构建配置 |
| `native/include/particle_native.h` | JNI 接口头文件 |
| `native/include/particle_simd.h` | SIMD 优化声明 |
| `native/src/particle_native.cpp` | JNI 接口实现 |
| `native/src/particle_physics.cpp` | 粒子物理 SIMD 计算 |
| `native/src/particle_render.cpp` | 粒子渲染 SIMD 计算 |
| `src/main/resources/natives/windows/x64/nightjava-particles.dll` | 编译后的 DLL |
| `src/main/kotlin/com/nj/NightJava.kt` | (修改) 添加 DLL 加载逻辑 |
| `src/main/kotlin/com/nj/NativeLoader.kt` | DLL 加载器工具类 |
| `src/main/kotlin/com/nj/particle/ParticleNativeLib.kt` | JNI 接口声明 |
| `src/main/kotlin/com/nj/mixin/ParticleManagerMixin.kt` | Mixin 注入粒子更新 |
| `src/main/kotlin/com/nj/mixin/BillboardParticleRendererMixin.kt` | Mixin 注入渲染 |

### 需要修改的文件

| 文件路径 | 修改内容 |
|---------|---------|
| `build.gradle.kts` | 添加 native 构建支持 |
| `gradle.properties` | 可选：添加 native 构建配置 |

---

## 性能预期

| 场景 | 原版 Java | C++ SIMD | 预期提升 |
|------|----------|----------|---------|
| 1000 粒子物理更新 | ~0.5ms | ~0.05ms | ~10x |
| 16384 粒子渲染变换 | ~3ms | ~0.3ms | ~10x |
| 爆炸粒子批量生成 | ~2ms | ~0.2ms | ~10x |

---

## 风险与注意事项

1. **平台兼容性**: 初期仅支持 Windows x64，后续可扩展 Linux/macOS
2. **内存安全**: JNI 调用需严格管理数组边界，避免 JVM 崩溃
3. **调试困难**: native 代码调试需要 Visual Studio 或 GDB
4. **版本适配**: Minecraft 更新可能导致 Mixin 注入点变化
5. **回退机制**: 必须确保 DLL 加载失败时模组仍可正常运行

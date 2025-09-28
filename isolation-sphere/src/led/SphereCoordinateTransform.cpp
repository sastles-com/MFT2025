#include "led/SphereCoordinateTransform.h"
#include <M5Unified.h>
#include <cmath>

SphereCoordinateTransform::SphereCoordinateTransform() 
    : initialized_(false), use_fast_math_(true) {
    // Initialize identity quaternion
    current_quaternion_ = {0.0f, 0.0f, 0.0f, 1.0f};
    ui_offset_ = {0.0f, 0.0f};
}

SphereCoordinateTransform::~SphereCoordinateTransform() {
    // Smart pointers will automatically clean up
}

bool SphereCoordinateTransform::initialize() {
    if (initialized_) {
        M5.Log.println("SphereCoordinateTransform already initialized");
        return true;
    }

    M5.Log.println("Initializing SphereCoordinateTransform with fast math approximations");
    
    initialized_ = true;
    return true;
}

void SphereCoordinateTransform::setIMUQuaternion(const Quaternion& quat) {
    current_quaternion_ = quat;
}

void SphereCoordinateTransform::setUIOffset(float latitude_offset, float longitude_offset) {
    ui_offset_.latitude = latitude_offset;
    ui_offset_.longitude = longitude_offset;
}

UVCoordinate SphereCoordinateTransform::transform3DToUV(const Vector3D& position_3d) const {
    // Step 1: Apply IMU quaternion rotation
    Vector3D rotated_pos = applyQuaternionRotation(position_3d, current_quaternion_);
    
    // Step 2: Convert to UV coordinates using the algorithm from the blog
    UVCoordinate uv = convertToUVCoordinates(rotated_pos);
    
    // Step 3: Apply UI offset
    uv.u += ui_offset_.latitude;
    uv.v += ui_offset_.longitude;
    
    // Step 4: Normalize to [0.0, 1.0] range
    uv.u = fmodf(uv.u + 1.0f, 1.0f);
    uv.v = fmodf(uv.v + 1.0f, 1.0f);
    
    return uv;
}

Vector3D SphereCoordinateTransform::applyQuaternionRotation(const Vector3D& pos, const Quaternion& quat) const {
    // Quaternion rotation: pos' = quat.rotateVector(pos)
    // Using the standard quaternion rotation formula:
    // v' = v + 2 * cross(quat.xyz, cross(quat.xyz, v) + quat.w * v)
    
    Vector3D quat_vec = {quat.x, quat.y, quat.z};
    Vector3D cross1 = crossProduct(quat_vec, pos);
    Vector3D temp = {
        cross1.x + quat.w * pos.x,
        cross1.y + quat.w * pos.y,
        cross1.z + quat.w * pos.z
    };
    Vector3D cross2 = crossProduct(quat_vec, temp);
    
    Vector3D result = {
        pos.x + 2.0f * cross2.x,
        pos.y + 2.0f * cross2.y,
        pos.z + 2.0f * cross2.z
    };
    
    return result;
}

UVCoordinate SphereCoordinateTransform::convertToUVCoordinates(const Vector3D& rotated_pos) const {
    // Implementation based on the blog algorithm:
    // u = arctan2(√(rx²+rz²), ry)
    // v = arctan2(rx, rz)
    
    UVCoordinate uv;
    
    if (use_fast_math_) {
        // Use fast approximation algorithms from the blog
        float sqrt_val = fastSqrt(rotated_pos.x * rotated_pos.x + rotated_pos.z * rotated_pos.z);
        uv.u = fastAtan2(sqrt_val, rotated_pos.y);
        uv.v = fastAtan2(rotated_pos.x, rotated_pos.z);
    } else {
        // Use standard math functions
        float sqrt_val = sqrtf(rotated_pos.x * rotated_pos.x + rotated_pos.z * rotated_pos.z);
        uv.u = atan2f(sqrt_val, rotated_pos.y);
        uv.v = atan2f(rotated_pos.x, rotated_pos.z);
    }
    
    // Convert from radians to normalized [0.0, 1.0] coordinates
    uv.u = (uv.u + M_PI) / (2.0f * M_PI);  // arctan2 range: [-π, π] → [0, 1]
    uv.v = (uv.v + M_PI) / (2.0f * M_PI);  // arctan2 range: [-π, π] → [0, 1]
    
    return uv;
}

float SphereCoordinateTransform::fastAtan2(float y, float x) const {
    // 4th order convergent approximation from the blog
    // Achieves ~20万倍 speedup: 2.09μs → 0.00001μs per call
    
    if (x == 0.0f) {
        return (y > 0.0f) ? M_PI_2 : -M_PI_2;
    }
    
    float ratio = y / x;
    float abs_ratio = fabsf(ratio);
    
    // Polynomial approximation for atan(t) where |t| <= 1
    float result;
    if (abs_ratio <= 1.0f) {
        float t2 = ratio * ratio;
        float t3 = t2 * ratio;
        float t5 = t3 * t2;
        float t7 = t5 * t2;
        
        // 4th order approximation: atan(t) ≈ t - t³/3 + t⁵/5 - t⁷/7
        result = ratio - t3/3.0f + t5/5.0f - t7/7.0f;
    } else {
        // For |t| > 1, use atan(1/t) = π/2 - atan(t)
        float inv_ratio = 1.0f / ratio;
        float t2 = inv_ratio * inv_ratio;
        float t3 = t2 * inv_ratio;
        float t5 = t3 * t2;
        float t7 = t5 * t2;
        
        result = M_PI_2 - (inv_ratio - t3/3.0f + t5/5.0f - t7/7.0f);
        if (ratio < 0.0f) result = -result;
    }
    
    // Adjust for quadrant
    if (x < 0.0f) {
        result = (y >= 0.0f) ? result + M_PI : result - M_PI;
    }
    
    return result;
}

float SphereCoordinateTransform::fastSqrt(float x) const {
    // 4th order convergent Newton-Raphson approximation from the blog
    // Achieves ~3.4倍 speedup: 15μs → 4.4μs per call
    
    if (x <= 0.0f) return 0.0f;
    if (x == 1.0f) return 1.0f;
    
    // Initial approximation using bit manipulation
    union { float f; uint32_t i; } u;
    u.f = x;
    u.i = (1 << 29) + (u.i >> 1) - (1 << 22);
    
    float result = u.f;
    
    // Newton-Raphson iterations: x_{n+1} = 0.5 * (x_n + S/x_n)
    // 4 iterations for 4th order convergence
    for (int i = 0; i < 4; i++) {
        result = 0.5f * (result + x / result);
    }
    
    return result;
}

Vector3D SphereCoordinateTransform::crossProduct(const Vector3D& a, const Vector3D& b) const {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

void SphereCoordinateTransform::setUseFastMath(bool use_fast) {
    use_fast_math_ = use_fast;
    M5.Log.printf("Fast math approximations %s\n", use_fast ? "enabled" : "disabled");
}

Quaternion SphereCoordinateTransform::getCurrentQuaternion() const {
    return current_quaternion_;
}

UIOffset SphereCoordinateTransform::getCurrentUIOffset() const {
    return ui_offset_;
}

bool SphereCoordinateTransform::isInitialized() const {
    return initialized_;
}

// Performance monitoring functions
uint32_t SphereCoordinateTransform::benchmarkTransform(uint32_t iterations) const {
    M5.Log.printf("Benchmarking coordinate transform (%u iterations)...\n", iterations);
    
    uint32_t start_time = micros();
    
    // Test with representative 3D positions
    Vector3D test_positions[] = {
        {1.0f, 0.0f, 0.0f},    // X-axis
        {0.0f, 1.0f, 0.0f},    // Y-axis
        {0.0f, 0.0f, 1.0f},    // Z-axis
        {0.707f, 0.707f, 0.0f}, // 45° diagonal
        {0.577f, 0.577f, 0.577f} // 3D diagonal
    };
    
    int num_positions = sizeof(test_positions) / sizeof(test_positions[0]);
    
    for (uint32_t i = 0; i < iterations; i++) {
        Vector3D pos = test_positions[i % num_positions];
        UVCoordinate uv = transform3DToUV(pos);
        
        // Prevent optimization from removing the calculation
        volatile float dummy = uv.u + uv.v;
        (void)dummy;
    }
    
    uint32_t end_time = micros();
    uint32_t total_time = end_time - start_time;
    
    M5.Log.printf("Transform benchmark: %u iterations in %u μs (%.3f μs/transform)\n",
                 iterations, total_time, (float)total_time / iterations);
    
    return total_time;
}

void SphereCoordinateTransform::comparePerformance() const {
    const uint32_t test_iterations = 1000;
    
    M5.Log.println("=== Coordinate Transform Performance Comparison ===");
    
    // Test fast math
    const_cast<SphereCoordinateTransform*>(this)->setUseFastMath(true);
    uint32_t fast_time = benchmarkTransform(test_iterations);
    
    // Test standard math
    const_cast<SphereCoordinateTransform*>(this)->setUseFastMath(false);
    uint32_t standard_time = benchmarkTransform(test_iterations);
    
    // Restore fast math setting
    const_cast<SphereCoordinateTransform*>(this)->setUseFastMath(true);
    
    float speedup = (float)standard_time / fast_time;
    M5.Log.printf("Performance comparison:\n");
    M5.Log.printf("  Fast math: %.3f μs/transform\n", (float)fast_time / test_iterations);
    M5.Log.printf("  Standard:  %.3f μs/transform\n", (float)standard_time / test_iterations);
    M5.Log.printf("  Speedup:   %.1fx faster\n", speedup);
}
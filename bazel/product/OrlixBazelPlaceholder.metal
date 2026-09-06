#include <metal_stdlib>
using namespace metal;

kernel void orlix_bazel_placeholder(device uint *out [[buffer(0)]]) {
    *out = 0u;
}

// Whiteout Delarus - Step 17 (1:1 Part XV): 64-Bin Log-Luminance Histogram Compute Shader (Slang)
// Vulkan 1.3+ / 1.4 Dynamic Rendering
// - Computes high-precision 64-bin histogram across dynamic range [2^-8 cd/m², 2^16 cd/m²] (>100,000:1)

[[vk::binding(0, 0)]]
Sampler2D hdrTexture;

[[vk::binding(1, 0)]]
RWStructuredBuffer<uint> histogramBuffer; // 64 uints

struct HistogramPushConstants {
    uint width;
    uint height;
    float minLogLum;  // -8.0 (covers deep shadows & moonlit night)
    float maxLogLum;  // +16.0 (covers midday sun on snow & specular glints)
};

[[vk::push_constant]]
ConstantBuffer<HistogramPushConstants> pc;

groupshared uint localHistogram[64];

[shader("compute")]
[numthreads(16, 16, 1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID, uint groupIndex : SV_GroupIndex) {
    // 1. Initialize threadgroup shared histogram bins
    if (groupIndex < 64) {
        localHistogram[groupIndex] = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    // 2. Sample pixel luminance and increment bin
    if (dispatchThreadID.x < pc.width && dispatchThreadID.y < pc.height) {
        float2 uv = (float2(dispatchThreadID.xy) + 0.5) / float2(pc.width, pc.height);
        float3 color = hdrTexture.SampleLevel(uv, 0.0).rgb;

        // Photometric luminance (CIE Rec. 709 / sRGB coefficients)
        float lum = dot(color, float3(0.2126, 0.7152, 0.0722));
        if (lum > 0.0001) {
            float logLum = log2(lum);
            float norm = clamp((logLum - pc.minLogLum) / (pc.maxLogLum - pc.minLogLum), 0.0, 1.0);
            uint bin = min(uint(norm * 64.0), 63u);
            InterlockedAdd(localHistogram[bin], 1);
        }
    }
    GroupMemoryBarrierWithGroupSync();

    // 3. Atomically accumulate into global storage buffer
    if (groupIndex < 64) {
        InterlockedAdd(histogramBuffer[groupIndex], localHistogram[groupIndex]);
    }
}

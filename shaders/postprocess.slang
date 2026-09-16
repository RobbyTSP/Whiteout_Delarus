// Whiteout Delarus - Step 17 (1:1 Part XV): Photometric Post-Processing, Glare & ACES Tone Mapping (Slang)
// Vulkan 1.3+ / 1.4 Dynamic Rendering
// - Fullscreen Triangle Rasterization
// - Hexagonal 6-Point Diffraction Sunstars (60° Ice Crystal Angles) with Occlusion Testing
// - Dual-Pass Anamorphic Ice Glare Streaks
// - Photometric Exposure Application & ACES Filmic Curve

struct PostProcessOutput {
    float4 sv_position : SV_Position;
    float2 uv          : TEXCOORD0;
};

[shader("vertex")]
PostProcessOutput vertexMain(uint vertexId : SV_VertexID) {
    PostProcessOutput output;
    // Fullscreen triangle covering clip space [-1, 1]
    float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
    float2 clipPos = uv * 2.0 - 1.0;
    output.uv = uv;
    output.sv_position = float4(clipPos, 0.0, 1.0);
    return output;
}

[[vk::binding(0, 0)]]
Sampler2D hdrTexture;

[[vk::binding(1, 0)]]
StructuredBuffer<float> exposureBuffer; // [0] = adaptedLum, [1] = exposure, [2] = targetLum

struct PostProcessPushConstants {
    float4 sunScreenPos; // xy = screen UV [0, 1], z = isVisible (0/1), w = sunIntensity
    float2 resolution;   // screen width, height
    float time;
    float blizzard;
};

[[vk::push_constant]]
ConstantBuffer<PostProcessPushConstants> pc;

// ACES Filmic Tone Mapping Curve (Academy Color Encoding System standard)
float3 ACESFilm(float3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

[shader("fragment")]
float4 fragmentMain(PostProcessOutput input) : SV_Target {
    float2 uv = input.uv;
    float3 hdrColor = hdrTexture.SampleLevel(uv, 0.0).rgb;

    float adaptedLum = max(exposureBuffer[0], 0.001);
    float exposure = clamp(exposureBuffer[1], 0.0008, 12.0);

    // =========================================================================
    // 1. Anamorphic Glare & Hexagonal Diffraction Sunstars (Beugungssterne)
    // =========================================================================
    float3 glare = float3(0.0);

    // A. Sun Diffraction Sunstar & Anamorphic Streak
    if (pc.sunScreenPos.z > 0.5) {
        float2 sunUV = pc.sunScreenPos.xy;

        // Mountain Occlusion Check:
        // Sample multi-tap kernel at the sun's screen coordinate to test if the sun is occluded
        // behind mountain peaks or visible in the open sky.
        float2 pStep = float2(1.0 / pc.resolution.x, 1.0 / pc.resolution.y) * 4.0;
        float lum0 = dot(hdrTexture.SampleLevel(sunUV, 0.0).rgb, float3(0.2126, 0.7152, 0.0722));
        float lum1 = dot(hdrTexture.SampleLevel(sunUV + float2(pStep.x, 0.0), 0.0).rgb, float3(0.2126, 0.7152, 0.0722));
        float lum2 = dot(hdrTexture.SampleLevel(sunUV - float2(pStep.x, 0.0), 0.0).rgb, float3(0.2126, 0.7152, 0.0722));
        float lum3 = dot(hdrTexture.SampleLevel(sunUV + float2(0.0, pStep.y), 0.0).rgb, float3(0.2126, 0.7152, 0.0722));
        float lum4 = dot(hdrTexture.SampleLevel(sunUV - float2(0.0, pStep.y), 0.0).rgb, float3(0.2126, 0.7152, 0.0722));
        float avgSunLum = (lum0 * 2.0 + lum1 + lum2 + lum3 + lum4) / 6.0;

        // An unoccluded sun has luminance > 10.0; a mountain ridge in shadow has luminance < 1.0
        float sunOcclusion = smoothstep(1.5, 6.5, avgSunLum);

        if (sunOcclusion > 0.001) {
            float aspect = pc.resolution.x / max(pc.resolution.y, 1.0);
            float2 dSun = uv - sunUV;
            dSun.x *= aspect;

            float distSun = length(dSun);
            if (distSun > 0.0001 && distSun < 1.3) {
                float angle = atan2(dSun.y, dSun.x);

                // Hexagonal 6-point diffraction rays (matching 60° prism faces of ice crystals)
                float hexSpikes = pow(clamp(cos(6.0 * angle) * 0.5 + 0.5, 0.0, 1.0), 80.0);
                float spikeFalloff = exp(-distSun * 5.2);

                // Chromatic dispersion on the diffraction spikes
                float3 sunstarColor = float3(
                    hexSpikes * exp(-distSun * 4.8) * 1.18,
                    hexSpikes * exp(-distSun * 5.2) * 1.04,
                    hexSpikes * exp(-distSun * 5.7) * 0.88
                ) * 3.2;

                // Anamorphic horizontal glare streak
                float horizStreak = exp(-pow(abs(dSun.y) / 0.0018, 2.0)) * exp(-abs(dSun.x) * 2.8);
                float3 streakColor = float3(1.15, 0.98, 0.76) * horizStreak * 2.2;

                // Secondary diagonal cross-rays (subtle optical element flare)
                float diagCross = pow(clamp(cos(4.0 * (angle + 0.35)) * 0.5 + 0.5, 0.0, 1.0), 64.0) * exp(-distSun * 7.5);
                float3 diagColor = float3(0.85, 0.95, 1.20) * diagCross * 0.65;

                glare += (sunstarColor + streakColor + diagColor) * pc.sunScreenPos.w * sunOcclusion * (1.0 - pc.blizzard * 0.85);
            }
        }
    }

    // B. Local Micro-Glint Sunstars (for intense specular sparkles on firn & sérac ice)
    float localLum = dot(hdrColor, float3(0.2126, 0.7152, 0.0722));
    float glintThreshold = adaptedLum * 4.5;
    if (localLum > glintThreshold && localLum > 4.0) {
        float glintExcess = (localLum - glintThreshold) / localLum;
        float2 px = float2(1.0 / pc.resolution.x, 1.0 / pc.resolution.y);

        // 4-point micro star glint on isolated specular peaks
        float3 sampleL = hdrTexture.SampleLevel(uv - float2(px.x * 2.5, 0.0), 0.0).rgb;
        float3 sampleR = hdrTexture.SampleLevel(uv + float2(px.x * 2.5, 0.0), 0.0).rgb;
        float3 sampleU = hdrTexture.SampleLevel(uv - float2(0.0, px.y * 2.5), 0.0).rgb;
        float3 sampleD = hdrTexture.SampleLevel(uv + float2(0.0, px.y * 2.5), 0.0).rgb;

        float neighborAvg = (dot(sampleL, float3(0.333)) + dot(sampleR, float3(0.333)) +
                             dot(sampleU, float3(0.333)) + dot(sampleD, float3(0.333))) * 0.25;

        // Only trigger if fragment is a localized sharp spike (not flat white area)
        if (localLum > neighborAvg * 1.5) {
            float glintSharpness = clamp((localLum - neighborAvg * 1.5) / localLum, 0.0, 1.0);
            float3 microSparkle = hdrColor * glintSharpness * glintExcess * 0.45;
            glare += microSparkle;
        }
    }

    // =========================================================================
    // 2. Photometric Exposure Application
    // =========================================================================
    float3 exposedScene = hdrColor * exposure + glare;

    // =========================================================================
    // 3. ACES Filmic Tone Mapping (HDR -> LDR Display)
    // =========================================================================
    float3 finalLdr = ACESFilm(exposedScene);

    return float4(finalLdr, 1.0);
}

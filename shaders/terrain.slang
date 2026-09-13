// Whiteout Delarus - Himalayan Multi-Layer PBR & POM Terrain Shader (Slang)
// Vulkan 1.3+ / 1.4 Dynamic Rendering

struct PushConstants {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float4 cameraPos;
    float4 sunDir;      // Direction towards sun (normalized)
    float4 sunColor;    // Sun intensity & spectrum
    float time;
    float minElev;      // 3651.0m
    float maxElev;      // 8753.0m
    float padding;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> pc;

// 18 Textures: ESRI Satellite, Macro Normal, and 4 ambientCG PBR sets (Rock, Snow, Scree, Glacier)
[[vk::binding(0, 0)]]  Sampler2D texSatellite;
[[vk::binding(1, 0)]]  Sampler2D texMacroNormal;

[[vk::binding(2, 0)]]  Sampler2D texRockAlbedo;
[[vk::binding(3, 0)]]  Sampler2D texRockNormal;
[[vk::binding(4, 0)]]  Sampler2D texRockRoughness;
[[vk::binding(5, 0)]]  Sampler2D texRockDisp;

[[vk::binding(6, 0)]]  Sampler2D texSnowAlbedo;
[[vk::binding(7, 0)]]  Sampler2D texSnowNormal;
[[vk::binding(8, 0)]]  Sampler2D texSnowRoughness;
[[vk::binding(9, 0)]]  Sampler2D texSnowDisp;

[[vk::binding(10, 0)]] Sampler2D texScreeAlbedo;
[[vk::binding(11, 0)]] Sampler2D texScreeNormal;
[[vk::binding(12, 0)]] Sampler2D texScreeRoughness;
[[vk::binding(13, 0)]] Sampler2D texScreeDisp;

[[vk::binding(14, 0)]] Sampler2D texGlacierAlbedo;
[[vk::binding(15, 0)]] Sampler2D texGlacierNormal;
[[vk::binding(16, 0)]] Sampler2D texGlacierRoughness;
[[vk::binding(17, 0)]] Sampler2D texGlacierDisp;

struct VertexInput {
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};

struct VertexOutput {
    float4 sv_position : SV_Position;
    float3 worldPos    : WORLDPOS;
    float3 worldNormal : NORMAL;
    float2 uv          : TEXCOORD0;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input) {
    VertexOutput output;
    float4 worldPos = mul(pc.model, float4(input.position, 1.0));
    output.worldPos = worldPos.xyz;
    output.worldNormal = normalize(mul((float3x3)pc.model, input.normal));
    output.uv = input.uv;
    output.sv_position = mul(pc.proj, mul(pc.view, worldPos));
    return output;
}

// Parallax Occlusion Mapping (POM) for first-person relief
float2 parallaxOcclusion(Sampler2D dispMap, float2 uv, float3 viewDirTS, float scale) {
    const float minLayers = 6.0;
    const float maxLayers = 20.0;
    float numLayers = lerp(maxLayers, minLayers, abs(dot(float3(0, 0, 1), viewDirTS)));
    float layerDepth = 1.0 / numLayers;
    float currentLayerDepth = 0.0;
    float2 P = viewDirTS.xy * scale;
    float2 deltaUV = P / numLayers;

    float2 currentUV = uv;
    float currentDepthMapValue = 1.0 - dispMap.Sample(currentUV).r;

    for (int i = 0; i < 20; i++) {
        if (currentLayerDepth >= currentDepthMapValue) break;
        currentUV -= deltaUV;
        currentDepthMapValue = 1.0 - dispMap.Sample(currentUV).r;
        currentLayerDepth += layerDepth;
    }

    float2 prevUV = currentUV + deltaUV;
    float afterDepth = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = (1.0 - dispMap.Sample(prevUV).r) - currentLayerDepth + layerDepth;
    float weight = clamp(afterDepth / max(afterDepth - beforeDepth, 0.001), 0.0, 1.0);
    return lerp(currentUV, prevUV, weight);
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
    float3 geomN = normalize(input.worldNormal);
    float3 V = normalize(pc.cameraPos.xyz - input.worldPos);
    float3 L = normalize(pc.sunDir.xyz);
    float distToCam = length(pc.cameraPos.xyz - input.worldPos);

    // 1. Sample Macro Textures (ESRI Satellite + Macro Normal)
    float4 satColor = texSatellite.Sample(input.uv);
    float3 macroNormSample = texMacroNormal.Sample(input.uv).rgb * 2.0 - 1.0;
    float3 N = normalize(geomN + macroNormSample * 0.45);

    // 2. Slope & Elevation Biome Weights
    float slope = 1.0 - clamp(N.y, 0.0, 1.0);
    float elevNorm = clamp((input.worldPos.y - pc.minElev) / max(pc.maxElev - pc.minElev, 1.0), 0.0, 1.0);

    // Steep cliff walls (> 40°): Snow cannot cling, exposing Himalayan granite rock
    float rockWeight = smoothstep(0.22, 0.48, slope);

    // High elevations (> 5300m) on gentle slopes: Alpine firn snow
    float snowWeight = smoothstep(0.35, 0.58, elevNorm) * (1.0 - rockWeight);

    // Mid elevations & glacial tongues (Khumbu Valley, 4800-5500m): Glacier ice
    float glacierMask = smoothstep(0.25, 0.40, elevNorm) * (1.0 - smoothstep(0.48, 0.62, elevNorm));
    float glacierWeight = glacierMask * (1.0 - rockWeight) * (1.0 - snowWeight) * 0.75;

    // Scree / Moraine gravel fills the remaining slopes and lower valley floors
    float screeWeight = max(0.0, 1.0 - (rockWeight + snowWeight + glacierWeight));

    // Normalize weights
    float totalW = rockWeight + snowWeight + screeWeight + glacierWeight;
    rockWeight /= totalW;
    snowWeight /= totalW;
    screeWeight /= totalW;
    glacierWeight /= totalW;

    // 3. Multi-Scale Detail UV Coordinates
    // Macro covers 34.6 km; Detail tiles every ~30 meters (tiling factor 1150.0)
    float2 detailUV = input.uv * 1150.0;

    // Micro tiling for close-up inspection right under mountaineer boots
    float2 microUV = input.uv * 4500.0;

    // Tangent space view vector approximation for POM
    float3 tangent = normalize(cross(float3(0, 0, 1), geomN));
    float3 bitangent = cross(geomN, tangent);
    float3 viewDirTS = normalize(float3(dot(V, tangent), dot(V, bitangent), dot(V, geomN)));

    // Parallax Occlusion Mapping (POM) active within 150m of player
    float pomFactor = clamp(1.0 - distToCam / 150.0, 0.0, 1.0);
    float2 pbrUV = detailUV;
    if (pomFactor > 0.01) {
        float2 pomOffset = parallaxOcclusion(texRockDisp, detailUV, viewDirTS, 0.035 * pomFactor);
        pbrUV = lerp(detailUV, pomOffset, pomFactor);
    }

    // 4. Sample Detail PBR Material Maps
    float3 rockAlbedo = texRockAlbedo.Sample(pbrUV).rgb;
    float3 rockNorm = texRockNormal.Sample(pbrUV).rgb * 2.0 - 1.0;
    float rockRough = texRockRoughness.Sample(pbrUV).r;

    float3 snowAlbedo = texSnowAlbedo.Sample(pbrUV).rgb;
    float3 snowNorm = texSnowNormal.Sample(pbrUV).rgb * 2.0 - 1.0;
    float snowRough = texSnowRoughness.Sample(pbrUV).r;

    float3 screeAlbedo = texScreeAlbedo.Sample(pbrUV).rgb;
    float3 screeNorm = texScreeNormal.Sample(pbrUV).rgb * 2.0 - 1.0;
    float screeRough = texScreeRoughness.Sample(pbrUV).r;

    float3 glacierAlbedo = texGlacierAlbedo.Sample(pbrUV).rgb;
    float3 glacierNorm = texGlacierNormal.Sample(pbrUV).rgb * 2.0 - 1.0;
    float glacierRough = texGlacierRoughness.Sample(pbrUV).r;

    // Blend PBR properties
    float3 detailAlbedo = rockAlbedo * rockWeight +
                          snowAlbedo * snowWeight +
                          screeAlbedo * screeWeight +
                          glacierAlbedo * glacierWeight;

    float3 detailNorm = rockNorm * rockWeight +
                        snowNorm * snowWeight +
                        screeNorm * screeWeight +
                        glacierNorm * glacierWeight;

    float roughness = rockRough * rockWeight +
                      snowRough * snowWeight +
                      screeRough * screeWeight +
                      glacierRough * glacierWeight;

    // Add subtle micro-grain to break up any repetition
    float microGrain = texRockRoughness.Sample(microUV).r * 0.15 - 0.075;
    detailAlbedo = clamp(detailAlbedo + microGrain, 0.0, 1.0);

    // 5. Combine Macro Satellite with High-Frequency PBR Detail
    // Near camera: crisp PBR textures modulated with satellite natural tint.
    // Far distance: satellite orthophoto dominates.
    float detailBlend = clamp(1.0 - (distToCam - 20.0) / 1200.0, 0.0, 1.0);
    float3 compositeAlbedo = lerp(satColor.rgb, detailAlbedo * satColor.rgb * 1.35, detailBlend);

    // Perturb terrain normal with detail normal map
    float3 finalNormal = normalize(N + (tangent * detailNorm.x + bitangent * detailNorm.y) * 0.65 * detailBlend);

    // 6. Physically-Based Lighting
    float NdotL = max(dot(finalNormal, L), 0.0);
    float3 directLight = NdotL * pc.sunColor.rgb;

    // High-altitude mountain sky hemisphere ambient (deep Himalayan azure)
    float skyFactor = clamp(finalNormal.y * 0.5 + 0.5, 0.0, 1.0);
    float3 skyLight = lerp(float3(0.10, 0.12, 0.15), float3(0.28, 0.38, 0.55), skyFactor);

    // Specular highlight for alpine snow, ice, and wet rock
    float3 H = normalize(L + V);
    float NdotH = max(dot(finalNormal, H), 0.0);
    float specPower = lerp(8.0, 64.0, 1.0 - roughness);
    float specIntensity = (snowWeight * 0.45 + glacierWeight * 0.85 + rockWeight * 0.15);
    float specular = pow(NdotH, specPower) * specIntensity * NdotL;

    // Fresnel effect on glancing angles (glacier ice & snow shimmer)
    float fresnel = pow(1.0 - max(dot(V, finalNormal), 0.0), 5.0) * (snowWeight * 0.35 + glacierWeight * 0.6);

    float3 litColor = compositeAlbedo * (directLight + skyLight) + float3(specular + fresnel, specular + fresnel, specular + fresnel);

    // 7. Atmospheric Rayleigh Scattering / Himalayan Altitude Fog
    float fogDensity = 0.000035;
    float fogFactor = 1.0 - exp(-distToCam * fogDensity);
    float3 fogColor = float3(0.52, 0.66, 0.82);

    float3 finalColor = lerp(litColor, fogColor, clamp(fogFactor, 0.0, 0.85));

    return float4(finalColor, 1.0);
}

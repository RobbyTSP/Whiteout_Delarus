// Whiteout Delarus - Step 18 (1:1 Part XVI): 3D Boulder & Talus Instancing (Slang)
// Vulkan 1.3+ / 1.4 Dynamic Rendering
// Features:
// - GPU-Instanced 3D Frost-Shattered Talus Rocks & Glacial Erratics
// - Triplanar Granite/Limestone PBR Mapping
// - Xanthoria elegans Golden-Orange High-Alpine Lichen Colonies
// - Top-Surface Snow Accumulation Dusting
// - 35-km DEM Cone-Tracing Shadows & Stratospheric Aerial Perspective

struct PushConstants {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float4 cameraPos;   // xyz = camera world pos, w = cloudDensity (0..1)
    float4 sunDir;      // xyz = normalized sun direction, w = windSpeed (km/h)
    float4 sunColor;    // rgb = sun irradiance spectrum, w = blizzardFactor (0..1)
    float time;
    float minElev;      // 3651.0m
    float maxElev;      // 8780.0m
    float cloudBase;    // Wolkenmeer cloud base elevation (~4900m)
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> pc;

// 20 Shared Textures
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

[[vk::binding(18, 0)]] Sampler2D texGeomorphology;
[[vk::binding(19, 0)]] Sampler2D texElevationDEM;

struct VertexInput {
    [[vk::location(0)]] float3 position   : POSITION;
    [[vk::location(1)]] float3 normal     : NORMAL;
    [[vk::location(2)]] float2 uv         : TEXCOORD0;

    // Per-instance attributes (Binding 1)
    [[vk::location(3)]] float4 instRow0   : ROW0;
    [[vk::location(4)]] float4 instRow1   : ROW1;
    [[vk::location(5)]] float4 instRow2   : ROW2;
    [[vk::location(6)]] float4 instRow3   : ROW3;
    [[vk::location(7)]] float4 instParams : PARAMS; // x = lichenDensity, y = rockType, z = snowDusting, w = scale
};

struct VertexOutput {
    float4 sv_position : SV_Position;
    float3 worldPos    : WORLDPOS;
    float3 worldNormal : NORMAL;
    float2 uv          : TEXCOORD0;
    float4 instParams  : PARAMS;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input) {
    VertexOutput output;

    // Column vectors of the instance model matrix from vertex attributes
    float3 col0 = input.instRow0.xyz; // X basis
    float3 col1 = input.instRow1.xyz; // Y basis
    float3 col2 = input.instRow2.xyz; // Z basis
    float3 col3 = input.instRow3.xyz; // Translation (world position)

    float3 worldPos3 = input.position.x * col0 +
                       input.position.y * col1 +
                       input.position.z * col2 +
                       col3;
    float4 worldPos = float4(worldPos3, 1.0);

    // Exact normal transformation for orthogonal rotation + non-uniform scale
    float3 worldNormal = normalize(
        (input.normal.x / max(dot(col0, col0), 0.0001)) * col0 +
        (input.normal.y / max(dot(col1, col1), 0.0001)) * col1 +
        (input.normal.z / max(dot(col2, col2), 0.0001)) * col2
    );

    output.sv_position = mul(pc.proj, mul(pc.view, worldPos));
    output.worldPos = worldPos3;
    output.worldNormal = worldNormal;
    output.uv = input.uv;
    output.instParams = input.instParams;

    return output;
}

// Cone-Tracing Soft Shadows from 35-km DEM Heightfield
float evaluateConeTracingShadows(float3 worldPos, float3 L) {
    if (L.y <= 0.01) return 0.0;

    float meshWidth = 34560.0;
    float meshDepth = 34560.0;
    float2 startUV = (worldPos.xz / float2(meshWidth, meshDepth)) + 0.5;

    float stepSizeMeters = 38.0;
    float2 stepUV = (L.xz * stepSizeMeters) / float2(meshWidth, meshDepth);
    float stepElev = L.y * stepSizeMeters;

    float2 currUV = startUV + stepUV * 2.5;
    float rayY = worldPos.y + stepElev * 2.5 + 1.2;

    float minDistanceRatio = 1.0;
    float sunTanHalfAngle = 0.00925;

    for (int i = 0; i < 24; i++) {
        if (currUV.x < 0.0 || currUV.x > 1.0 || currUV.y < 0.0 || currUV.y > 1.0) break;
        if (rayY > 8855.0) break;

        float terrainH = texElevationDEM.SampleLevel(currUV, 0.0).r;
        float hDiff = terrainH - rayY;

        if (hDiff > 0.0) {
            return 0.0;
        }

        float travelDist = float(i + 1) * stepSizeMeters;
        float coneRadius = travelDist * sunTanHalfAngle;
        float distToBeam = -hDiff;

        float ratio = distToBeam / max(coneRadius, 0.001);
        minDistanceRatio = min(minDistanceRatio, ratio);

        currUV += stepUV;
        rayY += stepElev;
    }

    return smoothstep(0.0, 1.0, clamp(minDistanceRatio, 0.0, 1.0));
}

// Triplanar Texture Sample Helper
float4 sampleTriplanar(Sampler2D tex, float3 worldPos, float3 N, float scale) {
    float3 weights = abs(N);
    weights = max(weights - 0.2, 0.0);
    weights /= (weights.x + weights.y + weights.z + 0.0001);

    float2 uvX = worldPos.zy * scale;
    float2 uvY = worldPos.xz * scale;
    float2 uvZ = worldPos.xy * scale;

    float4 colX = tex.Sample(uvX);
    float4 colY = tex.Sample(uvY);
    float4 colZ = tex.Sample(uvZ);

    return colX * weights.x + colY * weights.y + colZ * weights.z;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
    float3 N = normalize(input.worldNormal);
    float3 V = normalize(pc.cameraPos.xyz - input.worldPos);
    float3 L = normalize(pc.sunDir.xyz);
    float distToCam = length(pc.cameraPos.xyz - input.worldPos);

    // Triplanar sample rock PBR
    float rockScale = 0.35;
    float4 rockSample = sampleTriplanar(texRockAlbedo, input.worldPos, N, rockScale);
    float rockRoughSample = sampleTriplanar(texRockRoughness, input.worldPos, N, rockScale).r;

    // Rock base tint by formation
    float3 baseRockTint = float3(0.11, 0.115, 0.125);
    if (input.instParams.y > 1.5) {
        baseRockTint = float3(0.24, 0.19, 0.13); // Yellow Band marble
    } else if (input.instParams.y > 0.5) {
        baseRockTint = float3(0.15, 0.16, 0.17); // Qomolangma limestone
    }

    float3 albedo = rockSample.rgb * baseRockTint * 2.2;
    float roughness = clamp(rockRoughSample * 0.92, 0.82, 0.98);

    // 1. Xanthoria elegans Golden-Orange Lichen Colonies
    float lichenDensity = input.instParams.x;
    float isSideFace = 1.0 - clamp(abs(N.y), 0.0, 1.0);
    if (lichenDensity > 0.12 && isSideFace > 0.25) {
        float2 lUV = input.worldPos.xz * 1.8 + input.worldPos.y * 0.9;
        float p1 = sin(lUV.x * 2.8) * cos(lUV.y * 2.8);
        float p2 = sin(lUV.x * 5.6 + 1.1) * cos(lUV.y * 5.2 + 0.8);
        float colony = clamp(p1 * 0.65 + p2 * 0.35, 0.0, 1.0);
        float colonyPatch = smoothstep(0.52, 0.76, colony);

        float lichenFactor = colonyPatch * lichenDensity * isSideFace * 0.55;
        float3 lichenOrange = float3(0.72, 0.42, 0.14);
        float3 lichenGold   = float3(0.82, 0.58, 0.18);
        float3 lichenColor  = lerp(lichenOrange, lichenGold, colony);

        albedo = lerp(albedo, lichenColor, lichenFactor);
        roughness = lerp(roughness, 0.94, lichenFactor);
    }

    // 2. Top-Surface Snow Dusting
    float snowDust = smoothstep(0.55, 0.82, N.y) * input.instParams.z;
    if (snowDust > 0.01) {
        float4 snowSample = sampleTriplanar(texSnowAlbedo, input.worldPos, N, 0.5);
        float3 snowCol = snowSample.rgb * float3(0.96, 0.97, 1.0);
        albedo = lerp(albedo, snowCol, snowDust);
        roughness = lerp(roughness, 0.45, snowDust);
    }

    // 3. Photometric Lighting & DEM Cone-Tracing Shadows
    float NdotL = max(dot(N, L), 0.0);
    float3 H = normalize(V + L);
    float NdotH = max(dot(N, H), 0.0);

    float shadow = evaluateConeTracingShadows(input.worldPos, L);

    float3 sunColor = pc.sunColor.rgb * (1.0 - pc.sunColor.w * 0.85);
    float3 directLight = albedo * sunColor * NdotL * shadow * 3.8;

    // Specular
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denom = (NdotH * NdotH * (alpha2 - 1.0) + 1.0);
    float D = alpha2 / (3.14159 * denom * denom + 0.0001);
    float3 specular = float3(0.04) * D * NdotL * shadow * 0.75;

    // Ambient Sky & Bounce
    float skyExposure = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
    float3 skyLightColor = float3(0.18, 0.28, 0.48);
    float3 groundBounce = float3(0.25, 0.26, 0.28);
    float3 ambient = albedo * (skyLightColor * skyExposure + groundBounce * (1.0 - skyExposure)) * 0.45;

    float3 finalHDR = directLight + specular + ambient;

    // Aerial Haze
    float hazeDist = min(distToCam / 2500.0, 1.0);
    float3 hazeColor = float3(0.42, 0.58, 0.82);
    finalHDR = lerp(finalHDR, hazeColor, hazeDist * 0.15);

    return float4(finalHDR, 1.0);
}

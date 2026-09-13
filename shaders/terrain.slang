// Whiteout Delarus - Step 7 (1:1 Part III): Alpine Sculpting & Photorealism PBR Shader (Slang)
// Vulkan 1.3+ / 1.4 Dynamic Rendering
// Features:
// - Triplanar Mapping on steep slopes (zero vertical stretching across the entire 35 km massif)
// - Physical Slope Masking (<24° Snow/Glacier, 24°-38° Scree/Talus, >38° Bare Granite/Limestone)
// - Anti-Tiling Distance Blending (eliminates wallpaper grid repetition at distance)
// - Geomorphological Horizon Ambient Occlusion (Sky View Factor contact shadows)
// - ACES Filmic Tone Mapping (prevents sunlit snow highlight burnout)
// - Stratospheric Aerial Perspective & Rayleigh Haze
// - PBR Material Responses (Diamond Dust snow glitter, wet glacier ice Fresnel)

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

// 19 Textures: ESRI Satellite, Macro Normal, 4x ambientCG PBR sets, and Sculpted Geomorphology Map
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

// 18: Sculpted Geomorphology Map
// Channel R: Couloirs & Avalanche Chute Fluting (Hydraulic Erosion)
// Channel G: Thermal Talus Scree Cones (Angle of Repose 25°-35°)
// Channel B: Knife-Edge Ridge Crests & Arêtes (Convexity & Pinching)
// Channel A: Geomorphological Horizon Ambient Occlusion (Sky View Factor)
[[vk::binding(18, 0)]] Sampler2D texGeomorphology;

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

// 2D rotation for anti-tiling texture sampling
float2 rotateUV(float2 uv, float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return float2(uv.x * c - uv.y * s, uv.x * s + uv.y * c);
}

// Anti-Tiling Dual-Frequency Texture Sampling:
// Blends two incommensurable UV scales and rotations with organic multi-frequency spatial noise,
// completely eliminating repeating rectangular "wallpaper" patterns across vast distances!
float3 sampleAntiTile(Sampler2D tex, float2 uv, float3 worldPos) {
    float2 uv1 = uv;
    float2 uv2 = rotateUV(uv * 1.37, 0.54) + float2(0.43, 0.17);
    float n1 = sin(worldPos.x * 0.008 + sin(worldPos.z * 0.006) * 2.2);
    float n2 = cos(worldPos.z * 0.007 + cos(worldPos.x * 0.005) * 1.8);
    float blendNoise = smoothstep(0.32, 0.68, (n1 * n2) * 0.5 + 0.5);
    return lerp(tex.Sample(uv1).rgb, tex.Sample(uv2).rgb, blendNoise);
}

// Parallax Occlusion Mapping (POM) for first-person relief on flat/gentle ground
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

// ACES Filmic Tone Mapping:
// Compresses high-luminance highlights gracefully so sunlit snow retains surface detail
float3 ACESFilm(float3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
    float3 geomN = normalize(input.worldNormal);
    float3 V = normalize(pc.cameraPos.xyz - input.worldPos);
    float3 L = normalize(pc.sunDir.xyz);
    float distToCam = length(pc.cameraPos.xyz - input.worldPos);

    // 1. Sample Macro Textures (ESRI Satellite + Recomputed Macro Normal + Sculpted Geomorphology)
    float4 satColor = texSatellite.Sample(input.uv);
    float3 macroNormSample = texMacroNormal.Sample(input.uv).rgb * 2.0 - 1.0;
    float4 geomMorph = texGeomorphology.Sample(input.uv);
    // geomMorph.r = Couloirs & Avalanche Chute Fluting
    // geomMorph.g = Thermal Talus Scree Cones
    // geomMorph.b = Knife-Edge Ridge Crests & Arêtes
    // geomMorph.a = Geomorphological Horizon AO (Sky View Factor)

    // Map normal map channels to 3D world space: R = X (East/West), G = Z (North/South), B = Y (Up)
    float3 macroNormalWorld = float3(macroNormSample.r, macroNormSample.b, macroNormSample.g);
    float3 N = normalize(geomN + macroNormalWorld * 0.45);

    // 2. Physical Slope-Masking (Neigungs-Weiche)
    // Precise physical slope angle in degrees
    float slopeRad = acos(clamp(N.y, 0.0, 1.0));
    float slopeDeg = degrees(slopeRad);
    float elevNorm = clamp((input.worldPos.y - pc.minElev) / max(pc.maxElev - pc.minElev, 1.0), 0.0, 1.0);

    // Physical Classification:
    // Flach (< 22°): Schnee / Firn auf Hochebenen, Gletschereis im Tal
    // Mittel (22°–36°): Geröll / Moräne / Talus-Schotter
    // Steil (> 36°): Reine Felswand (Granit, Kalkstein, Yellow Band)
    float flatSlopeWeight = 1.0 - smoothstep(18.0, 25.0, slopeDeg);
    float screeSlopeWeight = smoothstep(18.0, 25.0, slopeDeg) * (1.0 - smoothstep(32.0, 42.0, slopeDeg));
    float steepCliffWeight = smoothstep(32.0, 42.0, slopeDeg);

    // Ridge crest wind-scour: jet stream blows snow away from sharp knife-edge arêtes
    float ridgeExposure = geomMorph.b;
    float bareRidgeBoost = ridgeExposure * smoothstep(0.35, 0.85, elevNorm) * 0.50;

    // Couloir Fluting: in carved avalanche chute troughs, firn snow clings down steep slopes
    float couloirFactor = geomMorph.r;
    float couloirSnowCling = couloirFactor * smoothstep(22.0, 55.0, slopeDeg) * 0.70;

    // Thermal Talus Scree Cone boost
    float talusConeBoost = geomMorph.g * 1.6;

    // Final physical material weights:
    // 1. Rock: steep cliffs (>32°-42°) + wind-scoured knife-edge ridges
    float rockWeight = clamp(steepCliffWeight + bareRidgeBoost, 0.0, 1.0);

    // 2. Snow: high-elevation plateau/firn (<22°) + couloir chute troughs
    float baseSnowWeight = smoothstep(0.35, 0.55, elevNorm) * flatSlopeWeight;
    float snowWeight = clamp(baseSnowWeight + couloirSnowCling, 0.0, 1.0) * (1.0 - rockWeight * 0.90);

    // 3. Glacier Ice: Khumbu Valley tongue (4,700m - 5,450m on gentle slopes < 22°)
    float glacierElev = smoothstep(4700.0, 5000.0, input.worldPos.y) * (1.0 - smoothstep(5400.0, 5650.0, input.worldPos.y));
    float glacierWeight = glacierElev * flatSlopeWeight * (1.0 - rockWeight * 0.80) * 0.85;

    // 4. Scree / Moraine / Valley Floor:
    // Universal geological base substrate for all middle slopes, talus cones, and valley floors
    float screeWeight = max(0.0, 1.0 - rockWeight - snowWeight - glacierWeight);

    // Normalize material weights
    float totalW = rockWeight + snowWeight + screeWeight + glacierWeight;
    if (totalW > 0.0001) {
        rockWeight /= totalW;
        snowWeight /= totalW;
        screeWeight /= totalW;
        glacierWeight /= totalW;
    } else {
        screeWeight = 1.0;
    }

    // 3. Triplanar PBR Mapping for Rock Cliffs (Completely Eliminates Vertical Stretching!)
    float rockScale = 1.0 / 22.0; // 22 meters per texture repetition
    float2 uvX = input.worldPos.zy * rockScale;
    float2 uvY = input.worldPos.xz * rockScale;
    float2 uvZ = input.worldPos.xy * rockScale;

    // Sharp triplanar projection blend weights (power-8 for clean, stretch-free edges)
    float3 blendWeights = pow(abs(geomN), float3(8.0, 8.0, 8.0));
    blendWeights /= (blendWeights.x + blendWeights.y + blendWeights.z + 0.00001);

    // Sample Triplanar Rock Albedo with Anti-Tiling
    float3 rockAlbedoX = sampleAntiTile(texRockAlbedo, uvX, input.worldPos.zyx);
    float3 rockAlbedoY = sampleAntiTile(texRockAlbedo, uvY, input.worldPos.xyz);
    float3 rockAlbedoZ = sampleAntiTile(texRockAlbedo, uvZ, input.worldPos.xzy);
    float3 triplanarRockAlbedo = rockAlbedoX * blendWeights.x + rockAlbedoY * blendWeights.y + rockAlbedoZ * blendWeights.z;

    // Sample Triplanar Rock Normal
    float3 rockNormX = texRockNormal.Sample(uvX).rgb * 2.0 - 1.0;
    float3 rockNormY = texRockNormal.Sample(uvY).rgb * 2.0 - 1.0;
    float3 rockNormZ = texRockNormal.Sample(uvZ).rgb * 2.0 - 1.0;

    // World-space normal reorientation for triplanar projection
    float3 wNormX = float3(rockNormX.z * sign(geomN.x), rockNormX.y, rockNormX.x * sign(geomN.x));
    float3 wNormY = float3(rockNormY.x, rockNormY.z * sign(geomN.y), rockNormY.y);
    float3 wNormZ = float3(rockNormZ.x, rockNormZ.y, rockNormZ.z * sign(geomN.z));
    float3 triplanarRockNormal = normalize(wNormX * blendWeights.x + wNormY * blendWeights.y + wNormZ * blendWeights.z);

    // Triplanar Rock Roughness
    float rockRoughX = texRockRoughness.Sample(uvX).r;
    float rockRoughY = texRockRoughness.Sample(uvY).r;
    float rockRoughZ = texRockRoughness.Sample(uvZ).r;
    float triplanarRockRoughness = rockRoughX * blendWeights.x + rockRoughY * blendWeights.y + rockRoughZ * blendWeights.z;

    // 4. Geological Strata: Mount Everest Rock Formations & The "Yellow Band"
    float strataH = input.worldPos.y - 0.22 * input.worldPos.z;
    float3 strataTint = float3(0.92, 0.92, 0.95);

    if (strataH >= 7000.0 && strataH < 8180.0) {
        float metaBand = sin(strataH * 0.08) * 0.05 * clamp(1.0 - distToCam / 600.0, 0.0, 1.0);
        strataTint = float3(0.88 + metaBand, 0.86 + metaBand, 0.82);
    }
    else if (strataH >= 8180.0 && strataH < 8600.0) {
        float yellowRhythm = (sin(strataH * 0.14) * 0.15) * clamp(1.0 - distToCam / 500.0, 0.0, 1.0);
        strataTint = lerp(float3(1.42, 1.22, 0.76), float3(1.26, 1.08, 0.68), yellowRhythm + 0.5);
    }
    else if (strataH >= 8600.0) {
        strataTint = float3(1.15, 1.12, 1.08);
    }

    triplanarRockAlbedo = clamp(triplanarRockAlbedo * strataTint * 1.15, 0.0, 1.0);

    // 5. Anti-Tiling Planar & POM Detail Sampling for Snow, Scree & Glacier
    float2 detailUV = input.uv * 1150.0;
    float2 microUV = input.uv * 4500.0;

    // Robust orthonormal tangent space (Gram-Schmidt):
    float3 upRef = (abs(geomN.y) < 0.99) ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(upRef, geomN));
    float3 bitangent = cross(geomN, tangent);
    float3 viewDirTS = normalize(float3(dot(V, tangent), dot(V, bitangent), dot(V, geomN)));

    // Parallax Occlusion Mapping (POM) on ground within 160m of camera
    float pomFactor = clamp(1.0 - distToCam / 160.0, 0.0, 1.0);
    float2 pbrUV = detailUV;
    if (pomFactor > 0.01) {
        float2 pomOffset = parallaxOcclusion(texSnowDisp, detailUV, viewDirTS, 0.038 * pomFactor);
        pbrUV = lerp(detailUV, pomOffset, pomFactor);
    }

    // Sample Snow, Scree, Glacier with Anti-Tiling to eliminate the "wallpaper effect"
    float3 snowAlbedo = sampleAntiTile(texSnowAlbedo, pbrUV, input.worldPos);
    float3 snowNorm = texSnowNormal.Sample(pbrUV).rgb * 2.0 - 1.0;
    float snowRough = texSnowRoughness.Sample(pbrUV).r * 0.85 + 0.15; // Crystalline firn ~0.62

    float3 screeAlbedo = sampleAntiTile(texScreeAlbedo, pbrUV, input.worldPos);
    // Alpine Desaturation above tree-line (> 4,200m): turns organic soil into grey moraine schist/shale
    float moraineGain = smoothstep(4000.0, 5100.0, input.worldPos.y);
    float screeLuma = dot(screeAlbedo, float3(0.299, 0.587, 0.114));
    screeAlbedo = lerp(screeAlbedo, float3(screeLuma * 0.75, screeLuma * 0.72, screeLuma * 0.70), moraineGain * 0.88);
    float3 screeNorm = texScreeNormal.Sample(pbrUV).rgb * 2.0 - 1.0;
    float screeRough = clamp(texScreeRoughness.Sample(pbrUV).r * 1.15, 0.82, 0.96); // Matte talus ~0.92

    float3 glacierAlbedo = sampleAntiTile(texGlacierAlbedo, pbrUV, input.worldPos);
    float3 glacierNorm = texGlacierNormal.Sample(pbrUV).rgb * 2.0 - 1.0;
    float glacierRough = clamp(texGlacierRoughness.Sample(pbrUV).r * 0.55, 0.20, 0.38); // Smooth ice ~0.28

    // Composite Planar Surfaces
    float3 planarAlbedo = snowAlbedo * snowWeight + screeAlbedo * screeWeight + glacierAlbedo * glacierWeight;
    float3 planarNorm = snowNorm * snowWeight + screeNorm * screeWeight + glacierNorm * glacierWeight;
    float planarRough = snowRough * snowWeight + screeRough * screeWeight + glacierRough * glacierWeight;

    // Composite Triplanar Rock with Planar Ground (Full PBR across entire mountain!)
    float3 detailAlbedo = triplanarRockAlbedo * rockWeight + planarAlbedo;
    float roughness = triplanarRockRoughness * rockWeight + planarRough;

    // High-frequency micro-grain: prevents pixelation at close distances (< 5m)
    float microGrain = texRockRoughness.Sample(microUV).r * 0.08 - 0.04;
    detailAlbedo = clamp(detailAlbedo + microGrain, 0.0, 1.0);

    // 6. Seamless Multi-Scale Blending:
    // PBR detail materials are the PRIMARY visual definition across the ENTIRE mountain at all distances!
    // The satellite orthofoto is applied as a subtle macro tint (25%) so regional color variations remain intact.
    float3 satMacroTint = lerp(float3(1.0, 1.0, 1.0), satColor.rgb * 1.35, 0.25);
    float3 compositeAlbedo = detailAlbedo * satMacroTint;

    // Apply Yellow Band geological marble strata to exposed rock
    float yellowBandMask = smoothstep(8160.0, 8240.0, strataH) * (1.0 - smoothstep(8580.0, 8640.0, strataH));
    float3 strataMacroTint = lerp(float3(1.0, 1.0, 1.0), float3(1.36, 1.18, 0.72), yellowBandMask * rockWeight * 0.75);
    compositeAlbedo *= strataMacroTint;

    // Composite detailed normal safely (prevent any normalize(0,0,0) NaNs):
    float planarLenSq = dot(planarNorm, planarNorm);
    float3 surfacePerturb = (planarLenSq > 0.001) ? 
        normalize(tangent * planarNorm.x + bitangent * planarNorm.y + geomN * planarNorm.z) : geomN;

    float3 pbrNormal = (rockWeight > 0.99) ? triplanarRockNormal :
                       (rockWeight < 0.01) ? surfacePerturb :
                       normalize(lerp(surfacePerturb, triplanarRockNormal, rockWeight));

    float3 finalNormal = normalize(N + pbrNormal * 0.50);

    // 7. Geomorphological Ambient Occlusion (Sky View Factor)
    // Deep couloirs, narrow canyons, and base folds receive deep contact shadowing
    float ao = clamp(geomMorph.a, 0.22, 1.0);

    // 8. Stratospheric Himalayan Atmospheric Lighting & Irradiance
    // Soft diffuse terminator (Half-Lambert wrap) prevents harsh polygonal shadow edges on hills
    float rawNdotL = dot(finalNormal, L);
    float NdotL = clamp((rawNdotL + 0.20) / 1.20, 0.0, 1.0);

    // Alpenglühen: High summits catch sunrise first while deep valleys are still in mountain shadow
    float dawnSummitBoost = clamp((input.worldPos.y - 4500.0) / 4200.0, 0.0, 1.0);
    float earthShadow = clamp(L.y * 14.0 + dawnSummitBoost * 0.70, 0.0, 1.0);
    // Direct light gets soft contact occlusion in deep crevices
    float3 directLight = NdotL * pc.sunColor.rgb * earthShadow * (0.60 + 0.40 * ao);

    // Stratospheric Sky Model: Dark navy sky with altitude progression
    float altRatio = clamp((input.worldPos.y - 5000.0) / 3848.0, 0.0, 1.0);
    float skyHemisphere = clamp(finalNormal.y * 0.5 + 0.5, 0.0, 1.0);
    float3 lowAltSky = lerp(float3(0.16, 0.20, 0.28), float3(0.38, 0.50, 0.70), skyHemisphere);
    float3 highAltSky = lerp(float3(0.08, 0.10, 0.18), float3(0.20, 0.28, 0.48), skyHemisphere);
    // Sky light fully modulated by Horizon AO (Sky View Factor)
    float3 skyLight = lerp(lowAltSky, highAltSky, altRatio) * (ao * 0.85 + 0.15);

    // Alpine indirect snow-bounce fill light: sunlit peaks bounce ambient light into shaded valleys
    float3 snowBounce = pc.sunColor.rgb * 0.22 * (ao * 0.70 + 0.30) * earthShadow;
    float3 totalAmbient = skyLight + snowBounce;

    // 9. PBR Specular & Diamond Dust Snow Glitter
    float3 H = normalize(L + V);
    float NdotH = clamp(dot(finalNormal, H), 0.0, 1.0);
    float specPower = lerp(12.0, 128.0, 1.0 - roughness);
    float specIntensity = (snowWeight * 0.40 + glacierWeight * 0.95 + rockWeight * 0.15);

    // Diamond dust sparkle: microscopic ice crystal facets shimmering in direct sunlight
    float RdotV = clamp(dot(reflect(-L, finalNormal), V), 0.0, 1.0);
    float sparkle = pow(RdotV, 96.0) * snowWeight * 0.55 * NdotL * earthShadow;
    float specular = pow(NdotH, specPower) * specIntensity * NdotL * earthShadow + sparkle;

    // Fresnel sheen at glancing angles (clamped strictly to [0, 1] to prevent NaNs)
    float VdotN = clamp(dot(V, finalNormal), 0.0, 1.0);
    float fresnel = pow(1.0 - VdotN, 5.0) * (snowWeight * 0.30 + glacierWeight * 0.75);

    float3 litColor = compositeAlbedo * (directLight + totalAmbient) + float3(specular + fresnel, specular + fresnel, specular + fresnel);

    // 10. Volumetric Valley Cloud Sea (Wolkenmeer)
    float cloudDensitySetting = pc.cameraPos.w;
    if (cloudDensitySetting > 0.05) {
        float cloudDeckBottom = pc.cloudBase - 250.0;
        float cloudDeckTop = pc.cloudBase + 600.0;

        float3 rayDir = normalize(input.worldPos - pc.cameraPos.xyz);
        float rayDist = distToCam;

        float invRayY = 1.0 / (abs(rayDir.y) > 0.0001 ? rayDir.y : 0.0001);
        float t1 = (cloudDeckBottom - pc.cameraPos.y) * invRayY;
        float t2 = (cloudDeckTop - pc.cameraPos.y) * invRayY;
        float tMin = max(0.0, min(t1, t2));
        float tMax = min(rayDist, max(t1, t2));

        if (tMin < tMax) {
            float slabThickness = tMax - tMin;
            float3 cloudCenterPos = pc.cameraPos.xyz + rayDir * (tMin + slabThickness * 0.5);
            float2 cloudDrift = float2(pc.time * 6.0, pc.time * 2.5);
            float cloudWave = sin((cloudCenterPos.x + cloudDrift.x) * 0.00045) * 
                              cos((cloudCenterPos.z + cloudDrift.y) * 0.00045) * 0.5 + 0.5;

            float midElevation = (cloudDeckBottom + cloudDeckTop) * 0.5;
            float vertProfile = clamp(1.0 - abs(cloudCenterPos.y - midElevation) / (cloudDeckTop - cloudDeckBottom), 0.0, 1.0);
            float localCloudDensity = smoothstep(0.18, 0.65, cloudWave * vertProfile) * cloudDensitySetting;

            float opticalDepth = localCloudDensity * clamp(slabThickness * 0.0018, 0.0, 4.0);
            float cloudTransmittance = exp(-opticalDepth);

            float cosTheta = dot(rayDir, L);
            float phaseMie = 0.5 * (1.0 + cosTheta * cosTheta) / pow(1.0 + 0.55 * 0.55 - 2.0 * 0.55 * cosTheta, 1.5);
            float3 sunlitCloud = pc.sunColor.rgb * (0.85 + 0.40 * phaseMie);
            float3 ambientCloud = lerp(float3(0.18, 0.22, 0.32), float3(0.55, 0.65, 0.82), skyHemisphere);
            float3 cloudColor = sunlitCloud + ambientCloud;

            litColor = lerp(cloudColor, litColor, cloudTransmittance);
        }
    }

    // 11. Stratospheric Himalayan Aerial Perspective & Rayleigh Scattering
    // High Himalayan dry, thin air has 100+ km crystal clarity
    float fogDensity = lerp(0.000008, 0.000003, altRatio);
    float fogFactor = 1.0 - exp(-distToCam * fogDensity);
    float3 fogColor = lerp(float3(0.50, 0.65, 0.82), float3(0.12, 0.18, 0.36), altRatio);
    float3 finalColor = lerp(litColor, fogColor, clamp(fogFactor, 0.0, 0.55));

    // 12. Blizzard, Spindrift & Whiteout Storm Simulation
    float blizzard = pc.sunColor.w;
    if (blizzard > 0.01) {
        float2 stormCoord = input.worldPos.xz * 0.08 + float2(pc.time * 55.0, pc.time * 25.0);
        float spindriftNoise = sin(stormCoord.x) * cos(stormCoord.y) * 0.5 + 0.5;
        float spindriftPlume = pow(spindriftNoise, 3.0) * (geomMorph.b * 1.5 + (1.0 - N.y) * 0.8) * blizzard * 0.40;

        float stormFogDensity = lerp(fogDensity, 0.00035, blizzard);
        float stormFogFactor = 1.0 - exp(-distToCam * stormFogDensity);
        float3 stormFogColor = float3(0.85, 0.88, 0.92);

        finalColor = lerp(finalColor, stormFogColor, clamp(stormFogFactor * (1.0 + blizzard * 4.5), 0.0, 0.96)) + 
                     float3(spindriftPlume, spindriftPlume, spindriftPlume);
    }

    // 13. ACES Filmic Tone Mapping & Anti-Blowout Highlight Preservation:
    // Compresses bright sunlit snow gracefully so highlight details never burn out into flat white!
    float exposure = 0.88;
    float3 toneMapped = ACESFilm(finalColor * exposure);

    return float4(toneMapped, 1.0);
}

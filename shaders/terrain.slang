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
    float3 normal = normalize(mul((float3x3)pc.model, input.normal));

    // Step 9.1: GPU-Tessellation / Virtual Heightfield (Detail-Displacement on Vertex Level)
    // Breaks up the 33.8-meter polygon raster edges into fine, rugged alpine rock crags and couloir terraces!
    float distToCam = length(pc.cameraPos.xyz - worldPos.xyz);
    // Player clearance: preserve ground collision within 4m of player camera stance, full effect from 9m to 1000m
    float nearClearance = smoothstep(3.5, 9.0, distToCam);
    float dispFade = smoothstep(1200.0, 80.0, distToCam) * nearClearance;

    if (dispFade > 0.001) {
        float2 wp = worldPos.xz;
        // Multi-octave domain-warped FBM micro-fracturing noise
        float n1 = sin(wp.x * 0.18 + cos(wp.y * 0.14) * 2.2);
        float n2 = cos(wp.y * 0.22 + sin(wp.x * 0.16) * 1.9);
        float n3 = sin(wp.x * 0.44 + wp.y * 0.38) * cos(wp.y * 0.52);
        float fbmNoise = (n1 * 0.55 + n2 * 0.30 + n3 * 0.15);

        // Geomorphology guide: lift sharp ridges and deepen avalanche couloirs
        float4 morph = texGeomorphology.SampleLevel(input.uv, 0.0);
        float couloirTrough = morph.r;
        float ridgeCrest = morph.b;

        // Slope factor: steeper slopes receive rugged horizontal crag fracturing
        float slopeFactor = 1.0 - clamp(normal.y, 0.0, 1.0);
        float2 horizNorm = normalize(normal.xz + float2(0.0001, 0.0001));
        float3 horizDisplace = float3(horizNorm.x, 0.0, horizNorm.y) * (fbmNoise * slopeFactor * 2.8);
        float vertDisplace = (fbmNoise * 0.35 + (ridgeCrest * 1.6 - couloirTrough * 1.2)) * slopeFactor;

        float3 dispVector = (horizDisplace + float3(0.0, vertDisplace, 0.0)) * dispFade;
        worldPos.xyz += dispVector;
    }

    output.worldPos = worldPos.xyz;
    output.worldNormal = normal;
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

// Anti-Tiling Multi-Scale Texture Sampling:
// Blends detail, mid, and far frequencies based on camera distance with domain warping and rotation.
float3 sampleAntiTileMulti(Sampler2D tex, float2 worldXZ, float distToCam, float baseScale) {
    float2 warp = float2(
        sin(worldXZ.y * 0.012 + cos(worldXZ.x * 0.008) * 1.6),
        cos(worldXZ.x * 0.011 + sin(worldXZ.y * 0.007) * 1.8)
    ) * 8.0;
    float2 warpedXZ = worldXZ + warp;

    float farBlend = smoothstep(120.0, 550.0, distToCam);
    float midBlend = smoothstep(30.0, 160.0, distToCam);

    float2 uvDetail = warpedXZ * (1.0 / baseScale);
    float2 uvMid = rotateUV(warpedXZ * (1.0 / (baseScale * 4.5)), 0.52) + float2(0.37, 0.61);
    float2 uvFar = rotateUV(warpedXZ * (1.0 / (baseScale * 16.0)), 1.15) + float2(0.82, 0.24);

    float n1 = sin(worldXZ.x * 0.005 + sin(worldXZ.y * 0.004) * 2.3);
    float n2 = cos(worldXZ.y * 0.0045 + cos(worldXZ.x * 0.004) * 1.9);
    float noise = smoothstep(0.28, 0.72, (n1 * n2) * 0.5 + 0.5);

    float3 colDetail = lerp(tex.Sample(uvDetail).rgb, tex.Sample(rotateUV(uvDetail, 0.68) + float2(0.4, 0.2)).rgb, noise * 0.5);
    float3 colMid = lerp(tex.Sample(uvMid).rgb, tex.Sample(rotateUV(uvMid, 0.76) + float2(0.7, 0.5)).rgb, noise * 0.5);
    float3 colFar = lerp(tex.Sample(uvFar).rgb, tex.Sample(rotateUV(uvFar, 0.89) + float2(0.2, 0.9)).rgb, noise * 0.5);

    float3 colCloseMid = lerp(colDetail, colMid, midBlend);
    return lerp(colCloseMid, colFar, farBlend);
}

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

// Domain-Warped Multi-Scale Distance-Tiled Triplanar Texture Sampling:
// Completely eliminates repeating rectangular or hexagonal grid tiling, hatching, and waffle patterns!
float3 sampleTriplanarAlbedoMulti(Sampler2D tex, float2 planeUV, float distToCam) {
    // 1. Organic Domain Warping: twists and bends coordinates so straight texture cracks don't form repeating lines
    float2 warp = float2(
        sin(planeUV.y * 0.015 + cos(planeUV.x * 0.009) * 1.7),
        cos(planeUV.x * 0.014 + sin(planeUV.y * 0.008) * 1.9)
    ) * 12.0;
    float2 warpedUV = planeUV + warp;

    // 2. Smooth Distance-Based Macro Transition:
    // Close (< 80m): 24m detail scale for rich hand-touch granite crevices
    // Mid (80m - 450m): 110m geological formation scale
    // Far (> 450m): 420m massive batholith / massif scale
    float farBlend = smoothstep(120.0, 600.0, distToCam);
    float midBlend = smoothstep(30.0, 160.0, distToCam);

    // Detail layer: 24m scale
    float2 uvDetail = warpedUV * (1.0 / 24.0);
    // Mid layer: 110m scale, rotated by 34 degrees
    float2 uvMid = rotateUV(warpedUV * (1.0 / 110.0), 0.59) + float2(0.41, 0.77);
    // Far layer: 420m scale, rotated by 68 degrees
    float2 uvFar = rotateUV(warpedUV * (1.0 / 420.0), 1.18) + float2(0.83, 0.29);

    // Low-frequency spatial noise to break up intra-layer periodicity
    float n1 = sin(planeUV.x * 0.004 + sin(planeUV.y * 0.003) * 2.4);
    float n2 = cos(planeUV.y * 0.0035 + cos(planeUV.x * 0.004) * 2.1);
    float noise = smoothstep(0.25, 0.75, (n1 * n2) * 0.5 + 0.5);

    // Blend rotated offset for each active scale to eliminate directional bias
    float3 colDetail = lerp(tex.Sample(uvDetail).rgb, tex.Sample(rotateUV(uvDetail, 0.73) + float2(0.3, 0.5)).rgb, noise * 0.5);
    float3 colMid = lerp(tex.Sample(uvMid).rgb, tex.Sample(rotateUV(uvMid, 0.81) + float2(0.6, 0.2)).rgb, noise * 0.5);
    float3 colFar = lerp(tex.Sample(uvFar).rgb, tex.Sample(rotateUV(uvFar, 0.95) + float2(0.1, 0.8)).rgb, noise * 0.5);

    float3 colCloseMid = lerp(colDetail, colMid, midBlend);
    return lerp(colCloseMid, colFar, farBlend);
}

float3 sampleTriplanarNormMulti(Sampler2D tex, float2 planeUV, float distToCam) {
    float2 warp = float2(
        sin(planeUV.y * 0.015 + cos(planeUV.x * 0.009) * 1.7),
        cos(planeUV.x * 0.014 + sin(planeUV.y * 0.008) * 1.9)
    ) * 12.0;
    float2 warpedUV = planeUV + warp;

    float farBlend = smoothstep(120.0, 600.0, distToCam);
    float midBlend = smoothstep(30.0, 160.0, distToCam);

    float2 uvDetail = warpedUV * (1.0 / 24.0);
    float2 uvMid = rotateUV(warpedUV * (1.0 / 110.0), 0.59) + float2(0.41, 0.77);
    float2 uvFar = rotateUV(warpedUV * (1.0 / 420.0), 1.18) + float2(0.83, 0.29);

    float3 nDetail = tex.Sample(uvDetail).rgb * 2.0 - 1.0;
    float3 nMid = tex.Sample(uvMid).rgb * 2.0 - 1.0;
    float3 nFar = tex.Sample(uvFar).rgb * 2.0 - 1.0;

    float3 nCloseMid = lerp(nDetail, nMid, midBlend);
    return normalize(lerp(nCloseMid, nFar, farBlend));
}

float sampleTriplanarRoughMulti(Sampler2D tex, float2 planeUV, float distToCam) {
    float2 warp = float2(
        sin(planeUV.y * 0.015 + cos(planeUV.x * 0.009) * 1.7),
        cos(planeUV.x * 0.014 + sin(planeUV.y * 0.008) * 1.9)
    ) * 12.0;
    float2 warpedUV = planeUV + warp;

    float farBlend = smoothstep(120.0, 600.0, distToCam);
    float midBlend = smoothstep(30.0, 160.0, distToCam);

    float2 uvDetail = warpedUV * (1.0 / 24.0);
    float2 uvMid = rotateUV(warpedUV * (1.0 / 110.0), 0.59) + float2(0.41, 0.77);
    float2 uvFar = rotateUV(warpedUV * (1.0 / 420.0), 1.18) + float2(0.83, 0.29);

    float rDetail = tex.Sample(uvDetail).r;
    float rMid = tex.Sample(uvMid).r;
    float rFar = tex.Sample(uvFar).r;

    float rCloseMid = lerp(rDetail, rMid, midBlend);
    return lerp(rCloseMid, rFar, farBlend);
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

    // Step 9.1: Multi-Scale Sobel Normal Map Blending with Slope Sensitivity
    // Brings out deep, dramatic shadows in couloirs, fissures, and cliff steps without eating extra polygons
    float geomSlopeDeg = degrees(acos(clamp(geomN.y, 0.0, 1.0)));
    float sobelStrength = lerp(0.55, 0.85, smoothstep(22.0, 52.0, geomSlopeDeg));
    float3 N = normalize(geomN + macroNormalWorld * sobelStrength);

    // 2. Geomorphological 1:1 Echtwelt-Abgleich (Real-World Snow & Rock Distribution)
    // Accurate physical slope angle in degrees
    float slopeRad = acos(clamp(N.y, 0.0, 1.0));
    float slopeDeg = degrees(slopeRad);
    float elev = input.worldPos.y;
    float elevNorm = clamp((elev - pc.minElev) / max(pc.maxElev - pc.minElev, 1.0), 0.0, 1.0);

    // 1:1 Satellite Ground Truth (Orbit Reflectance Guidance)
    // Real-world satellite survey accurately identifies perennial snowfields vs bare rock cliffs
    float satLuma = dot(satColor.rgb, float3(0.299, 0.587, 0.114));
    float satSnowPresence = smoothstep(0.46, 0.66, satLuma);
    float satRockPresence = 1.0 - smoothstep(0.34, 0.50, satLuma);

    // Natural Alpine Slope Limits:
    // Compacted firn & alpine snow clings securely to slopes up to 38°-44°
    // Cliffs steeper than 44°-52° shed snow down into couloirs, exposing raw rock
    float cliffRockSlope = smoothstep(38.0, 50.0, slopeDeg);
    float snowAdhesionSlope = 1.0 - smoothstep(36.0, 48.0, slopeDeg);

    // High-Altitude Permanent Snowline (~5,400m in Khumbu/Everest)
    float aboveSnowline = smoothstep(5100.0, 5700.0, elev);

    // Step 9.1: Ground-Truth Geological Elevation Strata & Jetstream Wind Scour
    // Absolute altitude control:
    // > 8.600 m: Qomolangma-Kalkstein (dunkelgrau/schwarz, fast kein Schnee wegen Wind)
    // 8.200 m – 8.600 m: Das berühmte Yellow Band (heller, ocker-gelblicher Dolomit-Streifen)
    // Unter 8.200 m: Dunkler Gneis/Granit der tibetischen Zone & North Col Formation
    float ridgeExposure = geomMorph.b;
    float windScourSummit = smoothstep(8580.0, 8660.0, elev);
    float jetStreamBlowOff = windScourSummit * 0.95;
    float windScouredRidge = ridgeExposure * smoothstep(0.35, 0.90, elevNorm) * 0.55 + jetStreamBlowOff;

    // Couloir & Chute Fluting: in carved avalanche chute troughs, firn snow clings down steep slopes up to 60°
    float couloirFactor = geomMorph.r;
    float couloirSnowCling = couloirFactor * smoothstep(22.0, 60.0, slopeDeg) * 0.85;

    // Horizontal Rock Strata Elevation
    float strataH = elev - 0.22 * input.worldPos.z;

    // 1. Rock Weight: Sheer cliffs (>44°-50°) + wind-scoured ridges + satellite-verified rock faces
    float naturalRock = clamp(cliffRockSlope + windScouredRidge, 0.0, 1.0);
    float rockWeight = clamp(naturalRock * 0.65 + satRockPresence * 0.55 + windScouredRidge * 0.45, 0.0, 1.0);

    // 2. Snow Weight: All high-altitude terrain below cliff angle + couloirs + satellite snow
    float alpineSnow = clamp(snowAdhesionSlope * (0.65 + 0.35 * aboveSnowline) + couloirSnowCling, 0.0, 1.0);
    float snowWeight = clamp(lerp(alpineSnow, 1.0, satSnowPresence * 0.75 * (1.0 - cliffRockSlope * 0.85)), 0.0, 1.0) * (1.0 - rockWeight * 0.88);

    // Above 8,600m on the summit pyramid, hurricane jetstream winds blow snow off all exposed slopes (>10°),
    // leaving the dark Qomolangma limestone dramatically exposed
    float summitSnowRetain = 1.0 - windScourSummit * smoothstep(8.0, 22.0, slopeDeg) * 0.96;
    snowWeight *= summitSnowRetain;
    rockWeight = clamp(rockWeight + windScourSummit * smoothstep(10.0, 25.0, slopeDeg) * 0.70, 0.0, 1.0);

    // 3. Glacier Ice: Khumbu Valley glacier tongue (4,700m - 5,450m on gentle valley floors)
    float glacierElev = smoothstep(4650.0, 4950.0, elev) * (1.0 - smoothstep(5350.0, 5550.0, elev));
    float glacierWeight = glacierElev * (1.0 - smoothstep(16.0, 24.0, slopeDeg)) * (1.0 - rockWeight) * 0.85;

    // 4. Scree / Moraine: ONLY below the snowline (<5,600m) in valleys and on talus cones!
    // Above 5,700m in the Death Zone, scree is almost zero (high terrain is 100% snow/ice/rock)!
    float talusConeBoost = geomMorph.g * 1.5;
    float screeAllowed = (1.0 - aboveSnowline * 0.88);
    float screeWeight = max(0.0, 1.0 - rockWeight - snowWeight - glacierWeight) * screeAllowed;

    // Step 9.2: Dynamic Talus Scree Fans at Cliff Bases
    // At the angle of repose (24°-36°), rock debris falls from cliffs and accumulates in fans
    float screeFooting = geomMorph.g * smoothstep(44.0, 26.0, slopeDeg) * (1.0 - aboveSnowline * 0.65);
    screeWeight = clamp(screeWeight + screeFooting * 0.85, 0.0, 1.0);

    // Reallocate any remaining suppressed scree weight above snowline to snow
    snowWeight += max(0.0, 1.0 - rockWeight - snowWeight - glacierWeight - screeWeight);

    // Normalize material weights
    float totalW = rockWeight + snowWeight + screeWeight + glacierWeight;
    if (totalW > 0.0001) {
        rockWeight /= totalW;
        snowWeight /= totalW;
        screeWeight /= totalW;
        glacierWeight /= totalW;
    } else {
        snowWeight = 1.0;
    }

    // Step 9.2: Sharpened Slope-Adaptive Triplanar Blend Exponent
    // Steep vertical cliffs (>30°) demand high blend power (8.0-10.0) to eliminate vertical texture dripping/smearing!
    float triplanarPower = lerp(4.0, 10.0, smoothstep(22.0, 52.0, geomSlopeDeg));
    float3 blendWeights = pow(abs(N), float3(triplanarPower, triplanarPower, triplanarPower));
    blendWeights /= (blendWeights.x + blendWeights.y + blendWeights.z + 0.00001);

    // Sample Triplanar Rock Albedo with Multi-Scale Distance-Tiled Anti-Tiling (eliminates waffle/honeycomb tiling!)
    float3 rockAlbedoX = sampleTriplanarAlbedoMulti(texRockAlbedo, input.worldPos.zy, distToCam);
    float3 rockAlbedoY = sampleTriplanarAlbedoMulti(texRockAlbedo, input.worldPos.xz, distToCam);
    float3 rockAlbedoZ = sampleTriplanarAlbedoMulti(texRockAlbedo, input.worldPos.xy, distToCam);
    float3 triplanarRockAlbedo = rockAlbedoX * blendWeights.x + rockAlbedoY * blendWeights.y + rockAlbedoZ * blendWeights.z;

    // Sample Triplanar Rock Normal with Multi-Scale Distance-Tiled blending
    float3 rockNormX = sampleTriplanarNormMulti(texRockNormal, input.worldPos.zy, distToCam);
    float3 rockNormY = sampleTriplanarNormMulti(texRockNormal, input.worldPos.xz, distToCam);
    float3 rockNormZ = sampleTriplanarNormMulti(texRockNormal, input.worldPos.xy, distToCam);

    // World-space normal reorientation for triplanar projection
    float3 wNormX = float3(rockNormX.z * sign(N.x), rockNormX.y, rockNormX.x * sign(N.x));
    float3 wNormY = float3(rockNormY.x, rockNormY.z * sign(N.y), rockNormY.y);
    float3 wNormZ = float3(rockNormZ.x, rockNormZ.y, rockNormZ.z * sign(N.z));
    float3 triplanarRockNormal = normalize(wNormX * blendWeights.x + wNormY * blendWeights.y + wNormZ * blendWeights.z);

    // Triplanar Rock Roughness with Multi-Scale Distance-Tiling
    float rockRoughX = sampleTriplanarRoughMulti(texRockRoughness, input.worldPos.zy, distToCam);
    float rockRoughY = sampleTriplanarRoughMulti(texRockRoughness, input.worldPos.xz, distToCam);
    float rockRoughZ = sampleTriplanarRoughMulti(texRockRoughness, input.worldPos.xy, distToCam);
    float triplanarRockRoughness = rockRoughX * blendWeights.x + rockRoughY * blendWeights.y + rockRoughZ * blendWeights.z;

    // Step 9.2: Triplanar Rock Displacement/Height for Height-Blending
    float rockDispH_X = texRockDisp.Sample(input.worldPos.zy * (1.0 / 24.0)).r;
    float rockDispH_Y = texRockDisp.Sample(input.worldPos.xz * (1.0 / 24.0)).r;
    float rockDispH_Z = texRockDisp.Sample(input.worldPos.xy * (1.0 / 24.0)).r;
    float rockHeight = rockDispH_X * blendWeights.x + rockDispH_Y * blendWeights.y + rockDispH_Z * blendWeights.z;

    // 4. Geological Strata: Mount Everest Rock Formations & The "Yellow Band"
    // Authentic Himalayan Petrology: Cool down warm tones into cold slate grey, gneiss & granite
    float rockLuma = dot(triplanarRockAlbedo, float3(0.299, 0.587, 0.114));
    float3 coldRockAlbedo = lerp(triplanarRockAlbedo, float3(rockLuma * 0.94, rockLuma * 0.98, rockLuma * 1.04), 0.60) * 1.12;

    float3 strataTint = float3(0.96, 0.98, 1.00);

    if (strataH < 8200.0) {
        // Base to 8,200m: North Col Formation & Tibetan Crystalline (dark grey schist, gneiss, granite)
        float metaBand = sin(strataH * 0.08) * 0.04 * clamp(1.0 - distToCam / 700.0, 0.0, 1.0);
        strataTint = float3(0.82 + metaBand, 0.85 + metaBand, 0.89);
    }
    else if (strataH >= 8200.0 && strataH < 8600.0) {
        // 8,200m – 8,600m: The Yellow Band (Das berühmte Gelbe Band aus dolomitischem Marmor)
        float yellowRhythm = (sin(strataH * 0.12) * 0.09) * clamp(1.0 - distToCam / 600.0, 0.0, 1.0);
        strataTint = lerp(float3(1.36, 1.26, 0.86), float3(1.22, 1.14, 0.78), yellowRhythm + 0.5);
    }
    else {
        // > 8,600m: Qomolangma Formation (dunkelgrauer bis fast schwarzer Ordovizium-Kalkstein)
        float summitGrain = sin(strataH * 0.16) * 0.03 * clamp(1.0 - distToCam / 500.0, 0.0, 1.0);
        strataTint = float3(0.52 + summitGrain, 0.54 + summitGrain, 0.58);
    }

    triplanarRockAlbedo = clamp(coldRockAlbedo * strataTint, 0.0, 1.0);

    // 5. Anti-Tiling Planar & POM Detail Sampling for Snow, Scree & Glacier (World Metric Space)
    // Decoupled completely from mesh vertex resolution to eliminate grid/waffle aliasing!
    float2 snowUV = input.worldPos.xz * (1.0 / 18.0);
    float2 screeUV = input.worldPos.xz * (1.0 / 12.5);
    float2 glacierUV = input.worldPos.xz * (1.0 / 28.0);

    // Robust orthonormal tangent space (Gram-Schmidt):
    float3 upRef = (abs(geomN.y) < 0.99) ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(upRef, geomN));
    float3 bitangent = cross(geomN, tangent);
    float3 viewDirTS = normalize(float3(dot(V, tangent), dot(V, bitangent), dot(V, geomN)));

    // Parallax Occlusion Mapping (POM) on ground within 140m of camera
    float pomFactor = clamp(1.0 - distToCam / 140.0, 0.0, 1.0);
    if (pomFactor > 0.01) {
        float2 pomOffset = parallaxOcclusion(texSnowDisp, snowUV, viewDirTS, 0.035 * pomFactor);
        snowUV = lerp(snowUV, pomOffset, pomFactor);
    }

    // Sample Snow with Multi-Scale Anti-Tiling
    float3 snowAlbedo = sampleAntiTileMulti(texSnowAlbedo, input.worldPos.xz, distToCam, 18.0);
    float3 snowNorm = texSnowNormal.Sample(snowUV).rgb * 2.0 - 1.0;
    float snowRough = texSnowRoughness.Sample(snowUV).r * 0.85 + 0.15; // Crystalline firn ~0.62

    // Sample Scree / Moraine: Top-down blended with side projection to prevent stretching on 30° slopes
    float3 screeAlbedoTop = sampleAntiTileMulti(texScreeAlbedo, input.worldPos.xz, distToCam, 14.0);
    float3 screeAlbedoSide = sampleTriplanarAlbedoMulti(texScreeAlbedo, input.worldPos.zy, distToCam);
    float3 screeAlbedo = lerp(screeAlbedoTop, screeAlbedoSide, clamp((1.0 - N.y) * 1.5, 0.0, 1.0));

    // Authentic High-Himalayan Petrology: Above 5,000m, scree is cold grey schist/slate/granite (no brown mud!)
    float alpineMoraineGain = smoothstep(4200.0, 6200.0, input.worldPos.y);
    float screeLuma = dot(screeAlbedo, float3(0.299, 0.587, 0.114));
    screeAlbedo = lerp(screeAlbedo, float3(screeLuma * 0.88, screeLuma * 0.90, screeLuma * 0.94), alpineMoraineGain);

    float3 screeNorm = texScreeNormal.Sample(screeUV).rgb * 2.0 - 1.0;
    float screeRough = clamp(texScreeRoughness.Sample(screeUV).r * 1.15, 0.82, 0.96); // Matte talus ~0.92

    // Sample Glacier Ice with Multi-Scale Anti-Tiling
    float3 glacierAlbedo = sampleAntiTileMulti(texGlacierAlbedo, input.worldPos.xz, distToCam, 28.0);
    float3 glacierNorm = texGlacierNormal.Sample(glacierUV).rgb * 2.0 - 1.0;
    float glacierRough = clamp(texGlacierRoughness.Sample(glacierUV).r * 0.55, 0.20, 0.38); // Smooth ice ~0.28

    // Composite Planar Surfaces
    float3 planarAlbedo = snowAlbedo * snowWeight + screeAlbedo * screeWeight + glacierAlbedo * glacierWeight;
    float3 planarNorm = snowNorm * snowWeight + screeNorm * screeWeight + glacierNorm * glacierWeight;
    float planarRough = snowRough * snowWeight + screeRough * screeWeight + glacierRough * glacierWeight;

    // Step 9.2: Physical Height-Blending with Distance Fading
    // Close to camera (<120m): snow physically settles into rock crevices and depressions first.
    // At distance (>120m): fades smoothly into continuous macro blending, completely eliminating distant moiré/screen artifacts!
    float snowHeight = texSnowDisp.Sample(snowUV).r;
    float heightBlendDistFade = clamp(1.0 - distToCam / 120.0, 0.0, 1.0);
    float heightModulation = heightBlendDistFade * 0.18;

    float rockHeightScore = rockWeight + (rockHeight - 0.5) * heightModulation;
    float groundHeightScore = (1.0 - rockWeight) + (snowHeight - 0.5) * heightModulation;
    float heightDiff = rockHeightScore - groundHeightScore;
    float rockBlendLocal = smoothstep(-0.08, 0.08, heightDiff);
    float rockBlendFactor = lerp(rockWeight, rockBlendLocal, heightBlendDistFade);

    // Composite Triplanar Rock with Planar Ground using Height-Blend factor
    float3 detailAlbedo = triplanarRockAlbedo * rockBlendFactor + planarAlbedo * (1.0 - rockBlendFactor);
    float roughness = triplanarRockRoughness * rockBlendFactor + planarRough * (1.0 - rockBlendFactor);

    // Metric close-up micro-grain: active ONLY within 12 meters of camera
    // Completely eliminates distant Moiré / honeycomb interference patterns!
    float closeGrainDist = clamp(1.0 - distToCam / 12.0, 0.0, 1.0);
    float microGrain = (closeGrainDist > 0.01) ? 
        (texRockRoughness.Sample(input.worldPos.xz * 3.5).r * 0.05 - 0.025) * closeGrainDist : 0.0;
    detailAlbedo = clamp(detailAlbedo + microGrain, 0.0, 1.0);

    // 6. Seamless Multi-Scale Blending:
    // PBR detail materials are the PRIMARY visual definition across the ENTIRE mountain at all distances!
    // The satellite orthofoto is applied as a subtle macro tint (20%) so regional color variations remain intact.
    float3 satMacroTint = lerp(float3(1.0, 1.0, 1.0), satColor.rgb * 1.20, 0.20);
    float3 compositeAlbedo = detailAlbedo * satMacroTint;

    // Composite detailed normal safely (prevent any normalize(0,0,0) NaNs):
    float planarLenSq = dot(planarNorm, planarNorm);
    float3 surfacePerturb = (planarLenSq > 0.001) ? 
        normalize(tangent * planarNorm.x + bitangent * planarNorm.y + geomN * planarNorm.z) : geomN;

    float3 pbrNormal = (rockBlendFactor > 0.99) ? triplanarRockNormal :
                       (rockBlendFactor < 0.01) ? surfacePerturb :
                       normalize(lerp(surfacePerturb, triplanarRockNormal, rockBlendFactor));

    // Step 9.2: High-Frequency Micro-Detail Normal Maps with Distance Fading
    // Fades in within 90 meters: granular firn snow crystals and sharp granite mineral fissures
    float closeDetailFade = clamp(1.0 - distToCam / 90.0, 0.0, 1.0);
    if (closeDetailFade > 0.005) {
        // 1.2m granite micro-cracks
        float2 microRockUV = input.worldPos.xz * 1.6;
        float3 microRockN = texRockNormal.Sample(microRockUV).rgb * 2.0 - 1.0;

        // 0.6m crystalline firn snow grain
        float2 microSnowUV = input.worldPos.xz * 3.2;
        float3 microSnowN = texSnowNormal.Sample(microSnowUV).rgb * 2.0 - 1.0;

        float3 microNormTangent = lerp(microRockN, microSnowN, 1.0 - rockBlendFactor);
        float3 microWorldN = normalize(tangent * microNormTangent.x + bitangent * microNormTangent.y + pbrNormal * 1.2);
        pbrNormal = normalize(lerp(pbrNormal, microWorldN, closeDetailFade * 0.45));
    }

    float3 finalNormal = normalize(N + pbrNormal * 0.50);

    // Step 9.2: Deep Cavity & Horizon Ambient Occlusion (Sky View Factor)
    // Plunges tight couloirs, gorge floors, and undercut rock walls into realistic deep shadow
    float gorgeCavity = smoothstep(0.20, 0.70, geomMorph.r) * smoothstep(28.0, 62.0, geomSlopeDeg);
    float ao = clamp(geomMorph.a * (1.0 - gorgeCavity * 0.40), 0.16, 1.0);

    // 8. Stratospheric Himalayan Atmospheric Lighting & Irradiance
    // Soft diffuse terminator (Half-Lambert wrap) prevents harsh blocky polygonal shadow edges
    float rawNdotL = dot(finalNormal, L);
    float diffuseWrap = 0.28;
    float NdotL = smoothstep(-diffuseWrap, 0.92, rawNdotL);

    // Alpenglühen: High summits catch sunrise first while deep valleys are still in mountain shadow
    float dawnSummitBoost = clamp((input.worldPos.y - 4500.0) / 4200.0, 0.0, 1.0);
    float earthShadow = clamp(L.y * 14.0 + dawnSummitBoost * 0.70, 0.0, 1.0);
    float3 directLight = NdotL * pc.sunColor.rgb * earthShadow * (0.60 + 0.40 * ao);

    // Stratospheric Sky Model: Dark navy sky with altitude progression
    float altRatio = clamp((input.worldPos.y - 5000.0) / 3848.0, 0.0, 1.0);
    float skyHemisphere = clamp(finalNormal.y * 0.5 + 0.5, 0.0, 1.0);
    float3 lowAltSky = lerp(float3(0.18, 0.22, 0.30), float3(0.42, 0.54, 0.75), skyHemisphere);
    float3 highAltSky = lerp(float3(0.10, 0.13, 0.22), float3(0.24, 0.32, 0.52), skyHemisphere);
    float3 skyLight = lerp(lowAltSky, highAltSky, altRatio) * (ao * 0.80 + 0.20);

    // Surrounding Glacier & Snow Bounce (Lower Hemisphere Fill):
    // High alpine snowfields bounce up to 75% of sunlight back up into shadowed rock walls
    float groundBounceFactor = clamp(-finalNormal.y * 0.5 + 0.5, 0.0, 1.0);
    float3 snowGroundBounce = float3(0.24, 0.28, 0.34) * groundBounceFactor * (ao * 0.65 + 0.35);

    // Opposing peak cross-bounce: sunlit opposite valley flanks cast soft fill
    float crossBounceFactor = clamp(-rawNdotL * 0.5 + 0.5, 0.0, 1.0);
    float3 crossPeakBounce = pc.sunColor.rgb * 0.16 * crossBounceFactor * (ao * 0.60 + 0.40) * earthShadow;

    // Rich ambient composite with a solid visibility floor (prevents pitch-black monolithic shadow blocks)
    float3 totalAmbient = max(skyLight + snowGroundBounce + crossPeakBounce, float3(0.14, 0.16, 0.20));

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

    // Step 9.2: Subsurface Scattering (SSS) for Alpine Firn & Glacier Ice
    // Takes away the chalky plaster look; light penetrates into ice crystals and scatters softly
    float sssSnowWeight = snowWeight * (1.0 - rockBlendFactor);
    float3 sssLightDir = normalize(L + finalNormal * 0.40);
    float sssBackDot = clamp(dot(V, -sssLightDir), 0.0, 1.0);
    float3 sssForwardGlow = pow(sssBackDot, 4.0) * float3(0.78, 0.88, 1.05) * pc.sunColor.rgb * 0.50 * earthShadow;
    float sssWrapTerm = smoothstep(-0.35, 0.65, rawNdotL);
    float3 sssVolumetricTint = float3(0.72, 0.86, 1.0) * pc.sunColor.rgb * sssWrapTerm * 0.25 * earthShadow;
    float3 snowSSS = (sssForwardGlow + sssVolumetricTint) * sssSnowWeight * (ao * 0.70 + 0.30);

    float3 litColor = compositeAlbedo * (directLight + totalAmbient) + 
                      float3(specular + fresnel, specular + fresnel, specular + fresnel) + 
                      snowSSS;

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

    // Step 9.2: Physically Based Stratospheric Rayleigh & Mie Atmospheric Scattering
    // High Himalayan thin air has crystal clarity; distant massifs turn deep azure blue (Rayleigh)
    // while the thin ice haze casts a soft silver-golden forward Mie halo around the sun
    float3 betaRayleigh = float3(5.8e-6, 13.5e-6, 33.1e-6) * lerp(1.0, 0.35, altRatio);
    float3 rayleighOpticalDepth = betaRayleigh * distToCam;
    float3 rayleighTransmittance = exp(-rayleighOpticalDepth);

    float betaMie = 2.5e-6 * lerp(1.2, 0.25, altRatio);
    float mieOpticalDepth = betaMie * distToCam;
    float mieTransmittance = exp(-mieOpticalDepth);

    float3 totalTransmittance = clamp(rayleighTransmittance * mieTransmittance, 0.0, 1.0);

    float cosThetaRay = dot(V, -L);
    float phaseRayleigh = 0.75 * (1.0 + cosThetaRay * cosThetaRay);
    float gMie = 0.76;
    float phaseMieAtm = 1.5 * (1.0 - gMie * gMie) / (2.0 + gMie * gMie) * (1.0 + cosThetaRay * cosThetaRay) / pow(max(1.0 + gMie * gMie - 2.0 * gMie * cosThetaRay, 0.001), 1.5);

    float3 rayleighInscatter = float3(0.26, 0.50, 0.90) * (1.0 - rayleighTransmittance) * phaseRayleigh * pc.sunColor.rgb * (earthShadow * 0.6 + 0.4);
    float3 mieInscatter = pc.sunColor.rgb * (1.0 - mieTransmittance) * phaseMieAtm * 0.65 * earthShadow;
    float3 ambientHaze = lerp(float3(0.35, 0.48, 0.72), float3(0.12, 0.18, 0.42), altRatio) * (1.0 - rayleighTransmittance.b);

    float3 atmosphericInscatter = rayleighInscatter + mieInscatter + ambientHaze;
    float3 finalColor = litColor * totalTransmittance + atmosphericInscatter;

    // 12. Blizzard, Spindrift & Whiteout Storm Simulation
    float blizzard = pc.sunColor.w;
    if (blizzard > 0.01) {
        float2 stormCoord = input.worldPos.xz * 0.08 + float2(pc.time * 55.0, pc.time * 25.0);
        float spindriftNoise = sin(stormCoord.x) * cos(stormCoord.y) * 0.5 + 0.5;
        float spindriftPlume = pow(spindriftNoise, 3.0) * (geomMorph.b * 1.5 + (1.0 - N.y) * 0.8) * blizzard * 0.40;

        float stormFogDensity = lerp(0.000008, 0.00035, blizzard);
        float stormFogFactor = 1.0 - exp(-distToCam * stormFogDensity);
        float3 stormFogColor = float3(0.85, 0.88, 0.92);

        finalColor = lerp(finalColor, stormFogColor, clamp(stormFogFactor * (1.0 + blizzard * 4.5), 0.0, 0.96)) + 
                     float3(spindriftPlume, spindriftPlume, spindriftPlume);
    }

    // Step 9.2: Jetstream Summit Snow Plumes (Schneefahnen / Banner Clouds)
    // Mount Everest's legendary signature: translucent spindrift plumes blown off the summit arêtes into the sky
    float plumeElev = smoothstep(7900.0, 8750.0, input.worldPos.y);
    float ridgeSharpness = smoothstep(0.30, 0.85, geomMorph.b);
    float plumeEligibility = plumeElev * ridgeSharpness;

    if (plumeEligibility > 0.01) {
        float2 jetstreamWindDir = normalize(float2(0.92, 0.38));
        float2 plumeTraverse = input.worldPos.xz * 0.015 - jetstreamWindDir * (pc.time * 2.8);
        float p1 = sin(plumeTraverse.x * 2.2 + cos(plumeTraverse.y * 1.8) * 2.4);
        float p2 = cos(plumeTraverse.y * 3.1 + sin(plumeTraverse.x * 1.9) * 2.1);
        float p3 = sin((plumeTraverse.x + plumeTraverse.y) * 4.5);
        float wispyPlume = pow(clamp((p1 * 0.5 + p2 * 0.35 + p3 * 0.15) * 0.5 + 0.5, 0.0, 1.0), 2.5);

        float plumeSunScatter = pow(clamp(dot(V, -L) * 0.5 + 0.5, 0.0, 1.0), 3.0);
        float3 plumeColor = pc.sunColor.rgb * (0.85 + 0.65 * plumeSunScatter) * float3(0.92, 0.95, 1.0);
        float plumeAlpha = wispyPlume * plumeEligibility * 0.36 * earthShadow;
        finalColor = lerp(finalColor, plumeColor, plumeAlpha);
    }

    // 13. Step 9.2: ACES Filmic Tone Mapping with Anti-Blowout Highlight Preservation
    // Prevents harsh white clipping on sun-facing snow slopes; maintains crisp crest contours and normal relief
    float exposure = 0.82;
    float3 toneMapped = ACESFilm(finalColor * exposure);

    return float4(toneMapped, 1.0);
}





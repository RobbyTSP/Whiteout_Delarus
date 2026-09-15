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

// Step 13 (1:1 Part XI): Multi-Scale Domain-Warped Displacement Sampling
// Beseitigt hochfrequente Wiederholungs-Muster (Pünktchen / Kachelung) im Displacement vollständig!
float sampleTriplanarDispMulti(Sampler2D tex, float2 planeUV, float distToCam) {
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

    float dDetail = tex.Sample(uvDetail).r;
    float dMid = tex.Sample(uvMid).r;
    float dFar = tex.Sample(uvFar).r;

    float dCloseMid = lerp(dDetail, dMid, midBlend);
    return lerp(dCloseMid, dFar, farBlend);
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

    // Step 10 & 12: Physikalische Schnee-, Schutt- und Gletscherschichtung
    // 1. Dauerschneegrenze im Khumbu-Himalaya (~5.050m)
    float aboveSnowline = smoothstep(4900.0, 5200.0, elev);

    // 2. Jetstream Höhenwind-Verdriftung (Wind aus West-Nordwest 285°)
    float2 jetstreamWindDir = normalize(float2(0.88, 0.35));
    float windAlignment = dot(N.xz, jetstreamWindDir);

    // 3. Messerscharfe Schneerinnen (Couloirs & Gullies):
    // Schnee sammelt sich tief und hart begrenzt in den Erosionsrinnen (selbst auf 75° Steilwänden!)
    float couloirSnowCling = smoothstep(0.25, 0.55, geomMorph.r) * smoothstep(10.0, 78.0, slopeDeg);

    // 4. Orkanartiger Gipfelwind (>8.560m): Orkan reißt Schnee von freien Gipfelflächen
    float summitWindScour = smoothstep(8560.0, 8680.0, elev) * (1.0 - smoothstep(0.85, 0.98, N.y));

    // 5. Steilwand-Abwurf: Hänge über 28°-44° werfen Schnee gravitativ ab (außer in Couloirs!)
    float cliffRockSlope = smoothstep(28.0, 44.0, slopeDeg) * (1.0 - couloirSnowCling * 0.90);

    // Horizontal Rock Strata Elevation
    float strataH = elev - 0.22 * input.worldPos.z;

    // 6. Planare Bodenablagerung (Gletscher, Schuttmoräne, Altschnee/Firn)
    // Glacier Ice: Khumbu-Gletscherzunge (4.650m - 5.400m in flachen Talsohlen)
    float glacierElev = smoothstep(4650.0, 4950.0, elev) * (1.0 - smoothstep(5350.0, 5500.0, elev));
    float glacierWeight = glacierElev * (1.0 - smoothstep(12.0, 22.0, slopeDeg)) * 0.85;

    // Step 13 (1:1 Part XI): Talus / Schuttkegel am Wandfuß (30°–40°)
    // Unterhalb von Steilwänden bildet abbrechendes Gestein feines Geröll
    float talusSlopeWindow = smoothstep(26.0, 32.0, slopeDeg) * (1.0 - smoothstep(38.0, 44.0, slopeDeg));
    float talusConeWeight = geomMorph.g * talusSlopeWindow;
    float screeFooting = clamp(talusConeWeight * 1.4 + geomMorph.g * smoothstep(35.0, 20.0, slopeDeg) * 0.6, 0.0, 1.0) * (1.0 - aboveSnowline * 0.50);
    float screeWeight = screeFooting * (1.0 - glacierWeight);

    // Schnee bedeckt Hänge & Hochebenen oberhalb der Schneegrenze (z.B. Basecamp 5.300m zu 100% Schnee!)
    float snowWeight = clamp(aboveSnowline * (1.0 - glacierWeight) + (1.0 - screeWeight) * aboveSnowline * 0.5, 0.0, 1.0);

    // Normalisiere planare Bodenmaterialien
    float planarTotalW = snowWeight + screeWeight + glacierWeight;
    if (planarTotalW > 0.0001) {
        snowWeight /= planarTotalW;
        screeWeight /= planarTotalW;
        glacierWeight /= planarTotalW;
    } else {
        snowWeight = 1.0;
        screeWeight = 0.0;
        glacierWeight = 0.0;
    }

    // Step 13 (1:1 Part XI): Exponentiell geschärfte Blend-Gewichte pow(|N|, 8.0)
    // Verhindert vertikales Triplanar-Stretching an Steilflanken vollständig
    float3 blendWeights = pow(abs(N), float3(8.0, 8.0, 8.0));
    blendWeights /= (blendWeights.x + blendWeights.y + blendWeights.z + 0.000001);

    // Sample Triplanar Rock Albedo with Multi-Scale Distance-Tiled Anti-Tiling (eliminates waffle/honeycomb tiling!)
    float3 rockAlbedoX = sampleTriplanarAlbedoMulti(texRockAlbedo, input.worldPos.zy, distToCam);
    float3 rockAlbedoY = sampleTriplanarAlbedoMulti(texRockAlbedo, input.worldPos.xz, distToCam);
    float3 rockAlbedoZ = sampleTriplanarAlbedoMulti(texRockAlbedo, input.worldPos.xy, distToCam);
    float3 triplanarRockAlbedo = rockAlbedoX * blendWeights.x + rockAlbedoY * blendWeights.y + rockAlbedoZ * blendWeights.z;

    // Sample Triplanar Rock Normal with Multi-Scale Distance-Tiled blending
    float3 rockNormX = sampleTriplanarNormMulti(texRockNormal, input.worldPos.zy, distToCam);
    float3 rockNormY = sampleTriplanarNormMulti(texRockNormal, input.worldPos.xz, distToCam);
    float3 rockNormZ = sampleTriplanarNormMulti(texRockNormal, input.worldPos.xy, distToCam);

    // Step 13 (1:1 Part XI): Exakte Tangentenraum-Transformation für X, Y und Z
    // Beseitigt invertierte Normalen und wachsartige glatte Schalen
    float signX = (N.x >= 0.0) ? 1.0 : -1.0;
    float signY = (N.y >= 0.0) ? 1.0 : -1.0;
    float signZ = (N.z >= 0.0) ? 1.0 : -1.0;

    // X-Projektion: U = worldPos.z, V = worldPos.y -> Tangent = (0,0,-signX), Bitangent = (0,1,0), Normal = (signX,0,0)
    float3 wNormX = float3(rockNormX.z * signX, rockNormX.y, -rockNormX.x * signX);

    // Y-Projektion: U = worldPos.x, V = worldPos.z -> Tangent = (1,0,0), Bitangent = (0,0,-signY), Normal = (0,signY,0)
    float3 wNormY = float3(rockNormY.x, rockNormY.z * signY, -rockNormY.y * signY);

    // Z-Projektion: U = worldPos.x, V = worldPos.y -> Tangent = (signZ,0,0), Bitangent = (0,1,0), Normal = (0,0,signZ)
    float3 wNormZ = float3(rockNormZ.x * signZ, rockNormZ.y, rockNormZ.z * signZ);

    float3 triplanarRockNormal = normalize(wNormX * blendWeights.x + wNormY * blendWeights.y + wNormZ * blendWeights.z);

    // Triplanar Rock Roughness with Multi-Scale Distance-Tiling
    float rockRoughX = sampleTriplanarRoughMulti(texRockRoughness, input.worldPos.zy, distToCam);
    float rockRoughY = sampleTriplanarRoughMulti(texRockRoughness, input.worldPos.xz, distToCam);
    float rockRoughZ = sampleTriplanarRoughMulti(texRockRoughness, input.worldPos.xy, distToCam);
    float triplanarRockRoughness = rockRoughX * blendWeights.x + rockRoughY * blendWeights.y + rockRoughZ * blendWeights.z;

    // Step 13 (1:1 Part XI): Multi-Scale Anti-Tiled Triplanar Rock Displacement
    float rockDispH_X = sampleTriplanarDispMulti(texRockDisp, input.worldPos.zy, distToCam);
    float rockDispH_Y = sampleTriplanarDispMulti(texRockDisp, input.worldPos.xz, distToCam);
    float rockDispH_Z = sampleTriplanarDispMulti(texRockDisp, input.worldPos.xy, distToCam);
    float rockHeight = rockDispH_X * blendWeights.x + rockDispH_Y * blendWeights.y + rockDispH_Z * blendWeights.z;

    // Step 12: 4. Knochentrockener dunkler Granit & harter Albedo-Kontrast
    // Der Fels ist nicht mehr ertrunken: Authentischer Albedo-Kontrast (Fels ~0.14-0.19 vs Schnee ~0.88-0.92)
    float rockLuma = dot(triplanarRockAlbedo, float3(0.299, 0.587, 0.114));
    // Neutralize warm earthy brown into authentic cold Himalayan metamorphic slate/gneiss
    float3 coldRockBase = lerp(triplanarRockAlbedo, float3(rockLuma * 0.90, rockLuma * 0.93, rockLuma * 1.00), 0.78);

    // Step 13 (1:1 Part XI): Geologische Schichtung über Weltkoordinate Y
    // > 8.600m: Fast schwarzer, schroffer Kalkstein (Qomolangma-Formation, jetstream-abgeweht)
    // 8.200m - 8.600m: Markantes, hellgelbliches Yellow Band (Dolomit-Marmor)
    // < 8.200m: Dunkelgrauer Gneis und Granit
    float elevY = input.worldPos.y;
    float yellowBandWeight = smoothstep(8180.0, 8260.0, elevY) * (1.0 - smoothstep(8560.0, 8640.0, elevY));
    float summitQomolangmaWeight = smoothstep(8580.0, 8660.0, elevY);

    float3 baseGraniteGneiss = float3(0.38, 0.40, 0.44); // Dunkelgrauer Gneis/Granit
    float3 yellowBandDolomite = float3(0.85, 0.74, 0.48); // Hellgelblicher Dolomit-Marmor
    float3 summitLimestone   = float3(0.18, 0.19, 0.22); // Fast schwarzer Qomolangma-Kalkstein

    float3 strataColor = lerp(baseGraniteGneiss, yellowBandDolomite, yellowBandWeight);
    strataColor = lerp(strataColor, summitLimestone, summitQomolangmaWeight);

    triplanarRockAlbedo = clamp(coldRockBase * strataColor * 1.15, 0.0, 1.0);

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

    // Step 12: Triplanar Snow Sampling to eliminate vertical stretching in couloirs and steep flanks
    float3 snowAlbedoX = sampleTriplanarAlbedoMulti(texSnowAlbedo, input.worldPos.zy, distToCam);
    float3 snowAlbedoY = sampleAntiTileMulti(texSnowAlbedo, input.worldPos.xz, distToCam, 18.0);
    float3 snowAlbedoZ = sampleTriplanarAlbedoMulti(texSnowAlbedo, input.worldPos.xy, distToCam);
    float3 snowAlbedo = snowAlbedoX * blendWeights.x + snowAlbedoY * blendWeights.y + snowAlbedoZ * blendWeights.z;

    float3 snowNormX = sampleTriplanarNormMulti(texSnowNormal, input.worldPos.zy, distToCam);
    float3 snowNormY = texSnowNormal.Sample(snowUV).rgb * 2.0 - 1.0;
    float3 snowNormZ = sampleTriplanarNormMulti(texSnowNormal, input.worldPos.xy, distToCam);
    // Step 13 (1:1 Part XI): Exakte Vorzeichen/Parität auch für Schnee
    float3 wSnowNormX = float3(snowNormX.z * signX, snowNormX.y, -snowNormX.x * signX);
    float3 wSnowNormY = float3(snowNormY.x, snowNormY.z * signY, -snowNormY.y * signY);
    float3 wSnowNormZ = float3(snowNormZ.x * signZ, snowNormZ.y, snowNormZ.z * signZ);
    float3 triplanarSnowNormal = normalize(wSnowNormX * blendWeights.x + wSnowNormY * blendWeights.y + wSnowNormZ * blendWeights.z);

    // Step 13 (1:1 Part XI): Firn & Harsch (Roughness ≈ 0.35 - 0.55 für Glitzern bei flachem Sonnenstand)
    float snowRough = clamp(texSnowRoughness.Sample(snowUV).r * 0.20 + 0.38, 0.35, 0.55);

    // Sample Scree / Moraine: Top-down blended with side projection to prevent stretching on 30° slopes
    float3 screeAlbedoTop = sampleAntiTileMulti(texScreeAlbedo, input.worldPos.xz, distToCam, 14.0);
    float3 screeAlbedoSide = sampleTriplanarAlbedoMulti(texScreeAlbedo, input.worldPos.zy, distToCam);
    float3 screeAlbedo = lerp(screeAlbedoTop, screeAlbedoSide, clamp((1.0 - N.y) * 1.5, 0.0, 1.0));

    // Authentic High-Himalayan Petrology: Above 5,000m, scree is cold grey schist/slate/granite (no brown mud!)
    float alpineMoraineGain = smoothstep(4200.0, 6200.0, input.worldPos.y);
    float screeLuma = dot(screeAlbedo, float3(0.299, 0.587, 0.114));
    screeAlbedo = lerp(screeAlbedo, float3(screeLuma * 0.88, screeLuma * 0.90, screeLuma * 0.94), alpineMoraineGain);

    float3 screeNorm = texScreeNormal.Sample(screeUV).rgb * 2.0 - 1.0;
    float screeRough = clamp(texScreeRoughness.Sample(screeUV).r * 0.15 + 0.85, 0.85, 0.96); // Matte talus ~0.92

    // Sample Glacier Ice with Multi-Scale Anti-Tiling
    float3 glacierAlbedo = sampleAntiTileMulti(texGlacierAlbedo, input.worldPos.xz, distToCam, 28.0);
    float3 glacierNorm = texGlacierNormal.Sample(glacierUV).rgb * 2.0 - 1.0;
    float glacierRough = clamp(texGlacierRoughness.Sample(glacierUV).r * 0.35 + 0.25, 0.25, 0.45); // Smooth ice ~0.30

    // Composite Planar Surfaces
    float3 planarAlbedo = snowAlbedo * snowWeight + screeAlbedo * screeWeight + glacierAlbedo * glacierWeight;
    float planarRough = snowRough * snowWeight + screeRough * screeWeight + glacierRough * glacierWeight;

    // Step 13 (1:1 Part XI): Leopardenmuster & fehlerhaftes Noise-Clipping vollständig eliminieren
    // 1. Slope als Primärfilter: An Steilwänden über 45° darf kein Schnee haften!
    //    Couloirs (Lawinenrinnen) behalten ihre tiefen Schneebänder
    float couloirSnowChute = smoothstep(0.24, 0.54, geomMorph.r) * smoothstep(12.0, 78.0, slopeDeg);
    float slopeSnowEligibility = 1.0 - smoothstep(28.0, 45.0, slopeDeg);

    // Schneeablagerungspotenzial:
    // Hochebenen & flache Hänge über 5.050m erhalten vollen Schnee,
    // Steilwände werfen Schnee ab, Couloirs fangen Schnee auf
    float snowAmount = (slopeSnowEligibility + couloirSnowChute * 0.85) * aboveSnowline * (1.0 - summitWindScour * 0.95);
    if (slopeDeg > 45.0 && couloirSnowChute < 0.15) {
        snowAmount = 0.0;
    }

    // 2. Physical Height-Blend statt Noise-Cutout:
    //    Schnee sammelt sich physikalisch immer zuerst in den Tälern und Rillen der Gesteinsstruktur:
    //    rockHeight aus triplanarer Displacement-Map (0 = Spaltentiefe, 1 = Felsspitze)
    //    rockCreviceDepth = 1.0 - rockHeight kehrt die Höhe um (1.0 = tiefste Felsspalte)
    float rockCreviceDepth = 1.0 - rockHeight;
    float blendSharpness = 6.0;
    float heightDifference = (rockCreviceDepth + snowAmount) - 1.0;
    float snowMask = saturate(heightDifference * blendSharpness);

    // Primärfilter-Absicherung: Reiner Fels an Steilwänden über 45°
    if (slopeDeg > 45.0 && couloirSnowChute < 0.15) {
        snowMask = 0.0;
    }

    // rockBlendFactor: 1.0 = nackter Fels, 0.0 = geschlossene Schneedecke
    float rockBlendFactor = 1.0 - snowMask;

    // Step 13 (1:1 Part XI): Granit/Kalkstein extrem rau (0.88 - 0.96, rein diffus, absolut null Glanz)
    float rockRoughPBR = clamp(triplanarRockRoughness * 0.08 + 0.90, 0.88, 0.96);

    // Composite Triplanar Rock with Planar Ground using Height-Blend factor
    float3 detailAlbedo = triplanarRockAlbedo * rockBlendFactor + planarAlbedo * (1.0 - rockBlendFactor);
    float roughness = rockRoughPBR * rockBlendFactor + planarRough * (1.0 - rockBlendFactor);

    // Metric close-up micro-grain: active ONLY within 12 meters of camera
    // Completely eliminates distant Moiré / honeycomb interference patterns!
    float closeGrainDist = clamp(1.0 - distToCam / 12.0, 0.0, 1.0);
    float microGrain = (closeGrainDist > 0.01) ? 
        (texRockRoughness.Sample(input.worldPos.xz * 3.5).r * 0.05 - 0.025) * closeGrainDist : 0.0;
    detailAlbedo = clamp(detailAlbedo + microGrain, 0.0, 1.0);

    // Step 12: 1. Kein Satelliten-Bleaching auf dunklem Fels: Tönung nur auf Schnee/Eis
    float3 satMacroTint = lerp(float3(1.0, 1.0, 1.0), satColor.rgb * 0.88, 0.12 * (1.0 - rockBlendFactor));
    float3 compositeAlbedo = detailAlbedo * satMacroTint;

    // Normalen-Kombination in World Space (verhindert Verzerrungen an Steilhängen vollständig):
    float3 screeWorldNorm = normalize(tangent * screeNorm.x + bitangent * screeNorm.y + geomN * screeNorm.z);
    float3 glacierWorldNorm = normalize(tangent * glacierNorm.x + bitangent * glacierNorm.y + geomN * glacierNorm.z);
    float3 groundWorldNorm = normalize(triplanarSnowNormal * snowWeight + screeWorldNorm * screeWeight + glacierWorldNorm * glacierWeight);
    float3 pbrNormal = normalize(lerp(groundWorldNorm, triplanarRockNormal, rockBlendFactor));

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

    // Step 12: 4. Tiefes HBAO & Schluchten-Schwärze
    // Schmale Couloirs, Schluchtenböden und Wandfüße erhalten tiefe, unbarmherzige Kontaktschatten
    float gorgeCavity = smoothstep(0.12, 0.55, geomMorph.r) * smoothstep(16.0, 52.0, geomSlopeDeg);
    float talusFootingOcclusion = geomMorph.g * smoothstep(36.0, 16.0, geomSlopeDeg) * 0.50;
    float valleyDepthOcclusion = smoothstep(5400.0, 4200.0, input.worldPos.y) * (1.0 - smoothstep(0.65, 0.92, N.y)) * 0.45;
    float localNormalCrevice = clamp(1.0 - dot(N, geomN), 0.0, 1.0) * 0.45;

    // Tiefen-Kontaktschatten (HBAO)
    float contactAO = clamp(pow(geomMorph.a, 2.2) * (1.0 - gorgeCavity * 0.72) * (1.0 - talusFootingOcclusion) * (1.0 - valleyDepthOcclusion) * (1.0 - localNormalCrevice), 0.015, 1.0);
    float ao = contactAO;

    // 8. Stratosphärische Beleuchtung: Harter Terminator für scharfe alpine Kanten
    float rawNdotL = dot(finalNormal, L);
    // Hard Lambertian cosine law for rock (strictly zero wrap, zero wax/plastic sheen!)
    // Snow retains subtle micro-crystal edge diffusion
    float rockNdotL = max(0.0, rawNdotL);
    float snowNdotL = smoothstep(-0.04, 0.98, rawNdotL);
    float NdotL = lerp(snowNdotL, rockNdotL, rockBlendFactor);

    // Alpenglühen: High summits catch sunrise first while deep valleys are still in mountain shadow
    float dawnSummitBoost = clamp((input.worldPos.y - 4500.0) / 4200.0, 0.0, 1.0);
    float earthShadow = clamp(L.y * 14.0 + dawnSummitBoost * 0.70, 0.0, 1.0);
    float3 directLight = NdotL * pc.sunColor.rgb * earthShadow * (0.35 + 0.65 * contactAO);

    // Stratospheric Sky Model: Realistisch dunkles Himmelslicht (keine künstliche flache Aufhellung!)
    float altRatio = clamp((input.worldPos.y - 5000.0) / 3848.0, 0.0, 1.0);
    float skyHemisphere = clamp(finalNormal.y * 0.5 + 0.5, 0.0, 1.0);
    float3 lowAltSky = lerp(float3(0.04, 0.06, 0.10), float3(0.12, 0.18, 0.28), skyHemisphere);
    float3 highAltSky = lerp(float3(0.018, 0.026, 0.05), float3(0.055, 0.085, 0.16), skyHemisphere);
    float3 skyLight = lerp(lowAltSky, highAltSky, altRatio) * pow(contactAO, 1.45);

    // Schnee-Bodenreflektion stark gedämpft in Spalten
    float groundBounceFactor = clamp(-finalNormal.y * 0.5 + 0.5, 0.0, 1.0);
    float3 snowGroundBounce = float3(0.10, 0.12, 0.16) * groundBounceFactor * pow(contactAO, 1.8);

    // Gegenhang-Reflektion in Schluchten
    float crossBounceFactor = clamp(-rawNdotL * 0.5 + 0.5, 0.0, 1.0);
    float3 crossPeakBounce = pc.sunColor.rgb * 0.09 * crossBounceFactor * pow(contactAO, 1.4) * earthShadow;

    // Tiefes, sattes Schatten-Fundament (tiefe Schluchten werden dunkel!)
    float3 totalAmbient = (skyLight + snowGroundBounce + crossPeakBounce);
    totalAmbient = max(totalAmbient, float3(0.008, 0.010, 0.015) * contactAO);

    // Step 13 (1:1 Part XI): PBR Specular & Subsurface Scattering (Translucency)
    // Fels: strikt Specular = 0.0, Fresnel = 0.0 (kein Plastik-/Wachsglanz)
    // Firn/Harsch: leicht diffus spiegelnd bei flachem Sonnenstand
    float3 H = normalize(L + V);
    float NdotH = clamp(dot(finalNormal, H), 0.0, 1.0);
    float specPower = lerp(16.0, 180.0, pow(1.0 - roughness, 2.0));

    // Glitzern bei flachem Sonnenstand (Low Sun Glint)
    float lowSunGlint = 1.0 - smoothstep(0.12, 0.75, L.y);
    float snowSpecCoeff = 0.08 + 0.16 * lowSunGlint;
    float specIntensity = (snowWeight * snowSpecCoeff + glacierWeight * 0.75) * (1.0 - rockBlendFactor);

    // Diamond dust sparkle: microscopic ice crystal facets shimmering in direct sunlight
    float RdotV = clamp(dot(reflect(-L, finalNormal), V), 0.0, 1.0);
    float sparkle = pow(RdotV, 128.0) * snowWeight * 0.40 * NdotL * earthShadow * (1.0 - rockBlendFactor);
    float specular = pow(NdotH, specPower) * specIntensity * NdotL * earthShadow + sparkle;

    // Fresnel sheen: subtle glancing rim on ice, essentially zero on snow, strictly zero on rock
    float VdotN = clamp(dot(V, finalNormal), 0.0, 1.0);
    float fresnel = pow(1.0 - VdotN, 5.0) * (snowWeight * 0.04 + glacierWeight * 0.50) * (1.0 - rockBlendFactor);

    // Subsurface Scattering (Translucency) für Schnee an Bergkämmen (kein kreidiger Gips-Look)
    float sssSnowWeight = snowWeight * pow(1.0 - rockBlendFactor, 1.8);
    float3 sssLightDir = normalize(L + finalNormal * 0.35);
    float sssForwardDot = clamp(dot(V, -sssLightDir), 0.0, 1.0);
    float ridgeTranslucency = geomMorph.b * 0.6 + 0.4;
    float3 sssForwardGlow = pow(sssForwardDot, 3.5) * float3(0.82, 0.90, 1.05) * pc.sunColor.rgb * 0.30 * earthShadow * ridgeTranslucency;
    float sssWrapTerm = smoothstep(-0.20, 0.80, rawNdotL);
    float3 sssVolumetricTint = float3(0.74, 0.88, 1.02) * pc.sunColor.rgb * sssWrapTerm * 0.14 * earthShadow;
    float3 snowSSS = (sssForwardGlow + sssVolumetricTint) * sssSnowWeight * (ao * 0.65 + 0.35);

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

    // Step 11: Exponential Height-Fog & Rayleigh Distanzdunst
    // In deep valleys, air density and moisture are higher; fog decreases exponentially with altitude
    float fogCamAlt = max(0.0, pc.cameraPos.y - 4200.0);
    float fogWorldAlt = max(0.0, input.worldPos.y - 4200.0);
    float avgFogDensity = (exp(-fogCamAlt / 1800.0) + exp(-fogWorldAlt / 1800.0)) * 0.5;

    float expFogOpticalDepth = distToCam * 0.000035 * avgFogDensity;
    float expFogFactor = 1.0 - exp(-expFogOpticalDepth);

    // Rayleigh scattering in Himalayan air
    float3 betaRayleigh = float3(6.5e-6, 14.5e-6, 36.0e-6) * lerp(1.0, 0.32, altRatio);
    float3 rayleighOpticalDepth = betaRayleigh * distToCam;
    float3 rayleighTransmittance = exp(-rayleighOpticalDepth);

    float betaMie = 2.8e-6 * lerp(1.2, 0.22, altRatio);
    float mieOpticalDepth = betaMie * distToCam;
    float mieTransmittance = exp(-mieOpticalDepth);

    float3 totalTransmittance = clamp(rayleighTransmittance * mieTransmittance, 0.0, 1.0);

    float cosThetaRay = dot(V, -L);
    float phaseRayleigh = 0.75 * (1.0 + cosThetaRay * cosThetaRay);
    float gMie = 0.76;
    float phaseMieAtm = 1.5 * (1.0 - gMie * gMie) / (2.0 + gMie * gMie) * (1.0 + cosThetaRay * cosThetaRay) / pow(max(1.0 + gMie * gMie - 2.0 * gMie * cosThetaRay, 0.001), 1.5);

    float3 rayleighInscatter = float3(0.24, 0.48, 0.88) * (1.0 - rayleighTransmittance) * phaseRayleigh * pc.sunColor.rgb * (earthShadow * 0.6 + 0.4);
    float3 mieInscatter = pc.sunColor.rgb * (1.0 - mieTransmittance) * phaseMieAtm * 0.65 * earthShadow;
    float3 ambientHaze = lerp(float3(0.38, 0.52, 0.78), float3(0.10, 0.15, 0.38), altRatio) * (1.0 - rayleighTransmittance.b);

    // Distant Rayleigh blueing & Exponential Height-Fog Horizon Blend
    float3 distantHazeColor = lerp(float3(0.48, 0.66, 0.88), float3(0.14, 0.20, 0.42), altRatio);
    float sunElevAtm = clamp(pc.sunDir.y, 0.0, 1.0);
    float sunsetHaze = (1.0 - smoothstep(0.06, 0.38, sunElevAtm));
    float3 warmSunsetHaze = pc.sunColor.rgb * float3(1.15, 0.62, 0.34);
    distantHazeColor = lerp(distantHazeColor, warmSunsetHaze, sunsetHaze * 0.70);

    float3 atmosphericInscatter = rayleighInscatter + mieInscatter + ambientHaze;
    float3 terrainColor = litColor * totalTransmittance + atmosphericInscatter;

    // 12. Blizzard, Spindrift & Whiteout Storm Simulation
    float blizzard = pc.sunColor.w;

    // Step 13 (1:1 Part XI): Distanzabhängiger Rayleigh-Dunst & Kontrastverlust über 30+ km
    // Bringt das Auge dazu, die gigantische Weite von 30-35 km Weitblick sofort plastisch wahrzunehmen
    float distAerialScale = 1.0 - exp(-distToCam * 0.000042);
    float3 distantMassifBlue = lerp(float3(0.32, 0.48, 0.82), float3(0.12, 0.18, 0.40), altRatio);
    terrainColor = lerp(terrainColor, distantMassifBlue * pc.sunColor.rgb, distAerialScale * 0.38 * (1.0 - blizzard));

    float3 finalColor = lerp(terrainColor, distantHazeColor, expFogFactor * 0.75);

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





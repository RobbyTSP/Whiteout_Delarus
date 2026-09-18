// Whiteout Delarus - Step 18 (1:1 Part XVI): CDLOD / Nanite-Level Micro-Terrain (0.25m Climbing Resolution)
// Vulkan 1.3+ / 1.4 Dynamic Rendering (Slang)
// Features:
// - Dynamic 0.25m Continuous Distance LOD Grid (80m x 80m climbing vicinity)
// - Bicubic Catmull-Rom DEM Elevation Synthesis
// - Stepped Limestone/Dolomite Bedding Strata (The Yellow Band & Qomolangma Formation)
// - Orthogonal Tectonic Joint Fractures (Handholds & Climbing Cracks)
// - Scree Talus Gravel Micro-Relief & Wind Sastrugi Snow Ridges
// - Continuous Distance LOD Morphing into 35km Base Terrain (Seamless Overlap)
// - Xanthoria elegans High-Alpine Golden-Orange Lichen Colonies
// - Climbing Edge Chalk/Frost Highlights on Step Convexities

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

// 20 Textures (Shared with base terrain)
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
[[vk::binding(20, 0)]] Sampler2D texMultiBounceGI;
[[vk::binding(21, 0)]] Sampler2D texSnowDeformation;

struct VertexInput {
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 normal   : NORMAL;
    [[vk::location(2)]] float2 uv       : TEXCOORD0;
};

struct VertexOutput {
    float4 sv_position : SV_Position;
    float3 worldPos    : WORLDPOS;
    float3 worldNormal : NORMAL;
    float3 macroNormal : NORMAL1;
    float2 uv          : TEXCOORD0;
    float3 microDetail : TEXCOORD1; // x = strataStep, y = jointCrack, z = screeRelief
};

// High-Precision Smooth C1 Filtering of the 1024x1024 Float32 DEM
// Eliminates polygon faceting from the 33.75m base texels and yields C1-smooth mountain flanks
float sampleDemSmooth(float2 uv) {
    float2 texSize = float2(1024.0, 1024.0);
    float2 coord = uv * texSize - 0.5;
    float2 f = frac(coord);
    float2 smoothF = f * f * (3.0 - 2.0 * f);
    float2 smoothUV = (floor(coord) + 0.5 + smoothF) / texSize;
    return texElevationDEM.SampleLevel(smoothUV, 0.0).r;
}

// Procedural Nanite-Level Alpine Micro-Relief (0.25m Climbing Features)
float evaluateMicroRelief(float2 posXZ, float baseH, float2 demUV, out float outStrata, out float outCrack, out float outScree) {
    float4 morph = texGeomorphology.SampleLevel(demUV, 0.0);
    float screeMask = morph.g;
    float couloir = morph.r;
    float arete = morph.b;

    // 1. Sedimentary & Metamorphic Strata Steps (15° Northeast Dip of Qomolangma Limestone & Yellow Band)
    // Realistic 1.6m - 2.8m climbing ledges with natural footholds
    float strataCoord = baseH * 0.966 + (posXZ.x * 0.18 + posXZ.y * 0.18);
    float stepPhase = strataCoord / 1.8;
    float stepFrac = frac(stepPhase);
    float stepLedge = (smoothstep(0.68, 0.96, stepFrac) - stepFrac) * 0.16;
    outStrata = smoothstep(0.85, 0.98, stepFrac);

    // 2. Orthogonal Tectonic Joint Fractures (Frost-wedged vertical cracks & climbing hand-jams)
    float jX = sin(posXZ.x * 1.1 + cos(posXZ.y * 0.8) * 1.5);
    float jZ = cos(posXZ.y * 1.2 + sin(posXZ.x * 0.8) * 1.5);
    float crack = -pow(clamp(abs(jX * jZ), 0.0, 1.0), 3.2) * 0.18;
    outCrack = crack;

    // 3. Talus Scree Pebble Micro-Relief (Frost-shattered gravel bumps on rubble fans)
    float pebble = (sin(posXZ.x * 3.8) * cos(posXZ.y * 3.4) +
                    sin(posXZ.x * 7.6 + posXZ.y * 6.8) * 0.35) * 0.08 * screeMask;
    outScree = pebble;

    // 4. Sastrugi Wind Snow Ripples (Aerodynamic snow ripples aligned with prevailing jet-stream)
    float sastrugi = sin((posXZ.x * 0.707 - posXZ.y * 0.707) * 1.8) * 0.06 * (1.0 - screeMask);

    // 5. Hotspot Sculpting Enhancement:
    // A. Hillary Step (-8535, -7935, 8790m): 12m vertical cliff with prominent stepped climbing ledges
    float dHillary = length(posXZ - float2(-8535.0, -7935.0));
    float hillaryHotspot = 1.0 - smoothstep(12.0, 42.0, dHillary);
    if (hillaryHotspot > 0.01) {
        stepLedge *= (1.0 + hillaryHotspot * 1.5);
        crack *= (1.0 + hillaryHotspot * 1.2);
    }

    // B. Third Step (-8690, -7820, 8690m): Sheer limestone pinnacle
    float dThird = length(posXZ - float2(-8690.0, -7820.0));
    float thirdHotspot = 1.0 - smoothstep(12.0, 38.0, dThird);
    if (thirdHotspot > 0.01) {
        crack *= (1.0 + thirdHotspot * 1.6);
    }

    // C. South Col (-7740, -4995, 7906m): Windswept shale plateau
    float dCol = length(posXZ - float2(-7740.0, -4995.0));
    float colHotspot = 1.0 - smoothstep(15.0, 55.0, dCol);
    if (colHotspot > 0.01) {
        pebble *= (1.0 + colHotspot * 1.8);
    }

    float totalDisplace = stepLedge + crack + pebble + sastrugi;

    // 6. CDLOD Morphing: Continuous Distance LOD blend
    // Smoothly morph displacement to 0.0 between 28m and 37m radius from camera
    float distCamXZ = length(posXZ - pc.cameraPos.xz);
    float edgeMorph = smoothstep(28.0, 37.0, distCamXZ);
    return totalDisplace * (1.0 - edgeMorph);
}

[shader("vertex")]
VertexOutput vertexMain(VertexInput input) {
    VertexOutput output;

    // Grid center snapped to 0.25m world coordinates based on camera position
    float2 snapCenter = floor(pc.cameraPos.xz / 0.25) * 0.25;
    float2 worldXZ = snapCenter + input.position.xz;

    float meshWidth = 34560.0;
    float meshDepth = 34560.0;
    float2 demUV = (worldXZ / float2(meshWidth, meshDepth)) + 0.5;

    // 1. High-precision base DEM elevation
    float baseElev = sampleDemSmooth(demUV);

    // 2. Nanite-level micro-relief
    float strataVal = 0.0;
    float crackVal = 0.0;
    float screeVal = 0.0;
    float microDisp = evaluateMicroRelief(worldXZ, baseElev, demUV, strataVal, crackVal, screeVal);

    float distCamXZ = length(worldXZ - pc.cameraPos.xz);
    float edgeMorph = smoothstep(68.0, 78.0, distCamXZ);
    float finalDisp = (microDisp + 0.02) * (1.0 - edgeMorph);

    // Step 20 Dynamic Snow Deformation (Elasto-plastic footstep indentation & rim)
    float2 deformUV = worldXZ / 64.0;
    float snowFade = 1.0 - smoothstep(58.0, 68.0, distCamXZ);
    float4 deform = texSnowDeformation.SampleLevel(deformUV, 0.0);
    float snowDeformY = (deform.r + deform.b) * (1.0 - edgeMorph) * snowFade;

    float finalY = baseElev + finalDisp + snowDeformY;
    float3 finalWorldPos = float3(worldXZ.x, finalY, worldXZ.y);

    // 3. Analytical Micro-Normal Calculation via 2nd-Order Central Differences
    float eps = 0.35;
    float dummyS, dummyC, dummyR;
    float2 uvR = ((worldXZ + float2(eps, 0.0)) / float2(meshWidth, meshDepth)) + 0.5;
    float2 uvL = ((worldXZ - float2(eps, 0.0)) / float2(meshWidth, meshDepth)) + 0.5;
    float2 uvU = ((worldXZ + float2(0.0, eps)) / float2(meshWidth, meshDepth)) + 0.5;
    float2 uvD = ((worldXZ - float2(0.0, eps)) / float2(meshWidth, meshDepth)) + 0.5;

    float hR = sampleDemSmooth(uvR) + evaluateMicroRelief(worldXZ + float2(eps, 0.0), sampleDemSmooth(uvR), uvR, dummyS, dummyC, dummyR);
    float hL = sampleDemSmooth(uvL) + evaluateMicroRelief(worldXZ - float2(eps, 0.0), sampleDemSmooth(uvL), uvL, dummyS, dummyC, dummyR);
    float hU = sampleDemSmooth(uvU) + evaluateMicroRelief(worldXZ + float2(0.0, eps), sampleDemSmooth(uvU), uvU, dummyS, dummyC, dummyR);
    float hD = sampleDemSmooth(uvD) + evaluateMicroRelief(worldXZ - float2(0.0, eps), sampleDemSmooth(uvD), uvD, dummyS, dummyC, dummyR);

    float3 tangentX = float3(2.0 * eps, hR - hL, 0.0);
    float3 tangentZ = float3(0.0, hU - hD, 2.0 * eps);
    float3 microNormal = normalize(cross(tangentZ, tangentX));

    // 4. Macro Geodetic Mountain Normal from DEM (for accurate slope gating of snow vs rock)
    float demEps = 6.0;
    float2 demUvR = ((worldXZ + float2(demEps, 0.0)) / float2(meshWidth, meshDepth)) + 0.5;
    float2 demUvL = ((worldXZ - float2(demEps, 0.0)) / float2(meshWidth, meshDepth)) + 0.5;
    float2 demUvU = ((worldXZ + float2(0.0, demEps)) / float2(meshWidth, meshDepth)) + 0.5;
    float2 demUvD = ((worldXZ - float2(0.0, demEps)) / float2(meshWidth, meshDepth)) + 0.5;

    float demHR = sampleDemSmooth(demUvR);
    float demHL = sampleDemSmooth(demUvL);
    float demHU = sampleDemSmooth(demUvU);
    float demHD = sampleDemSmooth(demUvD);

    float demDx = (demHR - demHL) / (2.0 * demEps);
    float demDz = (demHU - demHD) / (2.0 * demEps);
    float3 macroNormal = normalize(float3(-demDx, 1.0, -demDz));

    output.sv_position = mul(pc.proj, mul(pc.view, float4(finalWorldPos, 1.0)));
    output.worldPos = finalWorldPos;
    output.worldNormal = microNormal;
    output.macroNormal = macroNormal;
    output.uv = demUV;
    output.microDetail = float3(strataVal, crackVal, screeVal);

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
    float distCamXZ = length(input.worldPos.xz - pc.cameraPos.xz);
    if (distCamXZ > 39.2) {
        discard;
    }

    float3 N = normalize(input.worldNormal);
    float3 V = normalize(pc.cameraPos.xyz - input.worldPos);
    float3 L = normalize(pc.sunDir.xyz);
    float distToCam = length(pc.cameraPos.xyz - input.worldPos);

    float slopeRad = acos(clamp(N.y, -1.0, 1.0));
    float slopeDeg = degrees(slopeRad);
    float elev = input.worldPos.y;

    // Macro textures (ESRI satellite, geomorphology & macro normal)
    float4 satColor = texSatellite.Sample(input.uv);
    float4 geomMorph = texGeomorphology.Sample(input.uv);
    float3 macroN = normalize(input.macroNormal);
    float macroSlopeDeg = degrees(acos(clamp(macroN.y, -1.0, 1.0)));

    // 1. PBR Material Triplanar Samples
    float triScale = 0.045; // High-resolution 0.25m micro scale
    float4 rockAlbedoSample = sampleTriplanar(texRockAlbedo, input.worldPos, N, triScale);
    float rockRoughSample = sampleTriplanar(texRockRoughness, input.worldPos, N, triScale).r;

    float4 snowAlbedoSample = sampleTriplanar(texSnowAlbedo, input.worldPos, N, triScale * 0.8);
    float snowRoughSample = sampleTriplanar(texSnowRoughness, input.worldPos, N, triScale * 0.8).r;

    float4 screeAlbedoSample = sampleTriplanar(texScreeAlbedo, input.worldPos, N, triScale * 1.2);
    float screeRoughSample = sampleTriplanar(texScreeRoughness, input.worldPos, N, triScale * 1.2).r;

    // 2. High-Altitude Rock Colors (Granite basement vs. Yellow Band vs. Qomolangma Limestone)
    float elevY = input.worldPos.y;
    float yellowBandWeight = smoothstep(8180.0, 8260.0, elevY) * (1.0 - smoothstep(8560.0, 8640.0, elevY));
    float summitQomolangmaWeight = smoothstep(8580.0, 8660.0, elevY);

    float3 baseGraniteGneiss = float3(0.38, 0.40, 0.44); // Dunkelgrauer Gneis/Granit
    float3 yellowBandDolomite = float3(0.85, 0.74, 0.48); // Hellgelblicher Dolomit-Marmor
    float3 summitLimestone   = float3(0.18, 0.19, 0.22); // Fast schwarzer Qomolangma-Kalkstein

    float3 strataColor = lerp(baseGraniteGneiss, yellowBandDolomite, yellowBandWeight);
    strataColor = lerp(strataColor, summitLimestone, summitQomolangmaWeight);

    float rockLuma = dot(rockAlbedoSample.rgb, float3(0.299, 0.587, 0.114));
    float3 coldRockBase = lerp(rockAlbedoSample.rgb, float3(rockLuma * 0.90, rockLuma * 0.93, rockLuma * 1.00), 0.78);
    float3 rockAlbedo = clamp(coldRockBase * strataColor * 1.15, 0.0, 1.0);
    float rockRoughness = clamp(rockRoughSample * 0.08 + 0.90, 0.88, 0.96);

    // 3. Physical Slope & Altitude Material Blending
    // Exact 1:1 match with base terrain:
    // A. Hurricane summit wind scour (>8560m) strips exposed snow from summit ridges
    float summitWindScour = smoothstep(8560.0, 8680.0, elev) * (1.0 - smoothstep(0.85, 0.98, macroN.y));
    // B. Steep walls > 44° shed snow gravitatively; couloirs retain snow chutes
    float couloirSnowCling = smoothstep(0.24, 0.54, geomMorph.r) * smoothstep(12.0, 78.0, macroSlopeDeg);
    float slopeSnowEligibility = 1.0 - smoothstep(28.0, 44.0, macroSlopeDeg);
    float aboveSnowline = smoothstep(4900.0, 5200.0, elev);

    float snowAmount = (slopeSnowEligibility + couloirSnowCling * 0.85) * aboveSnowline * (1.0 - summitWindScour * 0.95);
    if (macroSlopeDeg > 44.0 && couloirSnowCling < 0.15) {
        snowAmount = 0.0;
    }

    // Physical Height-Blend using rock micro-crevices
    float rockHeight = sampleTriplanar(texRockDisp, input.worldPos, N, triScale).r;
    float rockCreviceDepth = 1.0 - rockHeight;
    float blendSharpness = 6.0;
    float heightDifference = (rockCreviceDepth + snowAmount) - 1.0;
    float snowPresence = saturate(heightDifference * blendSharpness);

    if (macroSlopeDeg > 44.0 && couloirSnowCling < 0.15) {
        snowPresence = 0.0;
    }

    float screeMask = smoothstep(22.0, 36.0, slopeDeg) * (1.0 - smoothstep(38.0, 52.0, slopeDeg)) * geomMorph.g;
    snowPresence = clamp(snowPresence * (1.0 - screeMask * 0.7), 0.0, 1.0);

    float3 albedo = rockAlbedo;
    float roughness = rockRoughness;
    float metallic = 0.0;

    // Blend Scree
    if (screeMask > 0.01) {
        float3 screeColor = screeAlbedoSample.rgb * float3(0.18, 0.17, 0.16);
        albedo = lerp(albedo, screeColor, screeMask);
        roughness = lerp(roughness, 0.92, screeMask);
    }

    float snowCavityAO = 1.0;

    // Blend Snow
    if (snowPresence > 0.01) {
        float3 pureSnow = snowAlbedoSample.rgb * float3(0.96, 0.97, 1.0);
        albedo = lerp(albedo, pureSnow, snowPresence);
        roughness = lerp(roughness, clamp(snowRoughSample * 0.20 + 0.38, 0.35, 0.55), snowPresence);

        // Step 20 Dynamic Snow Footprint Compaction & Rim Micro-Normal
        float2 deformUV = input.worldPos.xz / 64.0;
        float distCam = length(input.worldPos.xz - pc.cameraPos.xz);
        float snowFade = 1.0 - smoothstep(28.0, 31.0, distCam);
        float4 deform = texSnowDeformation.Sample(deformUV) * snowFade;

        // Boot tread & raised rim gradient normal perturbation
        if (abs(deform.r) > 0.001 || deform.b > 0.001) {
            float dStep = 1.0 / 1024.0;
            float hR = texSnowDeformation.Sample(deformUV + float2(dStep, 0.0)).r + texSnowDeformation.Sample(deformUV + float2(dStep, 0.0)).b;
            float hL = texSnowDeformation.Sample(deformUV - float2(dStep, 0.0)).r + texSnowDeformation.Sample(deformUV - float2(dStep, 0.0)).b;
            float hU = texSnowDeformation.Sample(deformUV + float2(0.0, dStep)).r + texSnowDeformation.Sample(deformUV + float2(0.0, dStep)).b;
            float hD = texSnowDeformation.Sample(deformUV - float2(0.0, dStep)).r + texSnowDeformation.Sample(deformUV - float2(0.0, dStep)).b;
            float2 slope = float2(hR - hL, hU - hD) / 0.125;
            N = normalize(N - float3(slope.x, 0.0, slope.y) * snowPresence * snowFade * 1.6);

            // Footstep cavity ambient occlusion (prevents depression washout in intense direct sun)
            snowCavityAO = 1.0 - clamp(-deform.r * 6.5, 0.0, 0.75);
        }

        // Compaction hardening: firm, crystalline firn ice floor
        if (deform.g > 0.02) {
            float comp = deform.g * snowPresence;
            float3 icyCompacted = float3(0.32, 0.55, 0.85); // High-contrast firn ice cyan-blue
            albedo = lerp(albedo, icyCompacted, comp * 0.75);
            roughness = lerp(roughness, 0.15, comp * 0.90); // Slick crystalline compaction glaze
        }

        // Raised displacement rim (fresh loose unconsolidated snow berm)
        if (deform.b > 0.002) {
            float rimFactor = clamp(deform.b * 16.0, 0.0, 0.65) * snowPresence;
            albedo = lerp(albedo, float3(1.0, 1.0, 1.0) * 1.30, rimFactor);
            roughness = lerp(roughness, 0.65, rimFactor);
        }

        albedo *= snowCavityAO;
    }

    // =========================================================================
    // 4. Xanthoria elegans Lichen Colony Simulation
    // =========================================================================
    // High-altitude golden-orange crustose lichen thriving between 4,800m and 8,300m
    // Grows on sheltered rock faces (slope 28°-65°) where snow cannot settle
    float isRock = (1.0 - snowPresence) * (1.0 - screeMask * 0.5);
    float lichenAlt = smoothstep(4800.0, 5300.0, elev) * (1.0 - smoothstep(8100.0, 8350.0, elev));
    float lichenSlope = smoothstep(26.0, 36.0, slopeDeg) * (1.0 - smoothstep(64.0, 78.0, slopeDeg));

    if (isRock > 0.05 && lichenAlt > 0.01 && lichenSlope > 0.01) {
        float2 lUV = input.worldPos.xz * 3.2;
        float cell1 = abs(sin(lUV.x * 4.2 + cos(lUV.y * 3.6) * 2.2));
        float cell2 = abs(cos(lUV.y * 4.8 + sin(lUV.x * 3.9) * 2.2));
        float colony = clamp(cell1 * cell2, 0.0, 1.0);
        float colonyRing = smoothstep(0.40, 0.72, colony);

        float lichenFactor = colonyRing * isRock * lichenAlt * lichenSlope;

        // Authentic Xanthoria elegans color palette: vibrant orange rim with golden-amber center
        float3 lichenOrange = float3(0.94, 0.46, 0.06);
        float3 lichenGold   = float3(0.98, 0.68, 0.12);
        float3 lichenColor  = lerp(lichenOrange, lichenGold, colony);

        albedo = lerp(albedo, lichenColor, lichenFactor * 0.85);
        roughness = lerp(roughness, 0.94, lichenFactor * 0.80);
    }

    // =========================================================================
    // 5. Climbing Edge Frost / Chalk Scuff Highlights
    // =========================================================================
    // Protruding step edges (high convexity) catch subtle rock scuffs & climbing abrasion
    float edgeHighlight = input.microDetail.x * (1.0 - snowPresence) * 0.12;
    if (edgeHighlight > 0.01) {
        albedo = lerp(albedo, albedo * 1.28 + float3(0.025, 0.025, 0.02), edgeHighlight);
        roughness = lerp(roughness, 0.78, edgeHighlight);
    }

    // =========================================================================
    // 6. Photometric Lighting & 35-km Cone-Tracing Shadows
    // =========================================================================
    float rawNdotL = dot(N, L);
    float rockNdotL = max(0.0, rawNdotL);
    float snowNdotL = smoothstep(-0.04, 0.98, rawNdotL);
    float NdotL = lerp(rockNdotL, snowNdotL, snowPresence);

    float dawnSummitBoost = clamp((input.worldPos.y - 4500.0) / 4200.0, 0.0, 1.0);
    float earthShadow = clamp(L.y * 14.0 + dawnSummitBoost * 0.70, 0.0, 1.0);
    float shadow = evaluateConeTracingShadows(input.worldPos, L);
    float sunVisibility = shadow * earthShadow;

    float contactAO = (geomMorph.a * 0.45 + 0.55) * snowCavityAO;

    // Direct Sun Radiance matching base terrain exactly
    float3 directLight = albedo * pc.sunColor.rgb * NdotL * sunVisibility * (0.35 + 0.65 * contactAO) * (1.0 - pc.sunColor.w * 0.85);

    // Specular Highlight (Snow sparkle vs. rock matte)
    float3 H = normalize(V + L);
    float NdotH = clamp(dot(N, H), 0.0, 1.0);
    float specPower = lerp(16.0, 180.0, pow(1.0 - roughness, 2.0));
    float lowSunGlint = 1.0 - smoothstep(0.12, 0.75, L.y);
    float snowSpecCoeff = 0.08 + 0.16 * lowSunGlint;
    float specIntensity = snowPresence * snowSpecCoeff;
    float specular = pow(NdotH, specPower) * specIntensity * NdotL * sunVisibility;

    // Ambient Sky & Snow Reflectance
    float altRatio = clamp((input.worldPos.y - 5000.0) / 3848.0, 0.0, 1.0);
    float skyHemisphere = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
    float3 lowAltSky = lerp(float3(0.04, 0.06, 0.10), float3(0.12, 0.18, 0.28), skyHemisphere);
    float3 highAltSky = lerp(float3(0.018, 0.026, 0.05), float3(0.055, 0.085, 0.16), skyHemisphere);
    float3 skyLight = lerp(lowAltSky, highAltSky, altRatio);

    float groundBounceFactor = clamp(-N.y * 0.5 + 0.5, 0.0, 1.0);
    float3 snowGroundBounce = float3(0.10, 0.12, 0.16) * groundBounceFactor;
    float3 giRadiance = texMultiBounceGI.Sample(input.uv).rgb;
    float3 ambient = albedo * (skyLight + snowGroundBounce + giRadiance) * (0.35 + 0.65 * contactAO);

    // Subsurface Scattering (Snow translucency on step edges)
    float3 sssLightDir = normalize(L + N * 0.35);
    float sssForwardDot = clamp(dot(V, -sssLightDir), 0.0, 1.0);
    float3 sssForwardGlow = pow(sssForwardDot, 3.5) * float3(0.82, 0.90, 1.05) * pc.sunColor.rgb * 0.30 * sunVisibility;
    float3 snowSSS = sssForwardGlow * snowPresence * 0.5;

    float3 finalHDR = directLight + specular + ambient + snowSSS;

    // Aerial Rayleigh Haze (near-field camera distance < 39m)
    float hazeDist = min(distToCam / 2500.0, 1.0);
    float3 hazeColor = float3(0.42, 0.58, 0.82);
    finalHDR = lerp(finalHDR, hazeColor, hazeDist * 0.05);

    return float4(finalHDR, 1.0);
}

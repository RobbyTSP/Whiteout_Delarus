// Whiteout Delarus - Himalayan 1:1 Geomorphological Multi-Layer PBR & Triplanar Terrain Shader (Slang)
// Vulkan 1.3+ / 1.4 Dynamic Rendering

struct PushConstants {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float4 cameraPos;   // xyz = camera world pos, w = cloudDensity (0..1)
    float4 sunDir;      // xyz = normalized sun direction, w = windSpeed (km/h)
    float4 sunColor;    // rgb = sun irradiance spectrum, w = blizzardFactor (0..1)
    float time;
    float minElev;      // 3651.0m
    float maxElev;      // 8753.0m
    float cloudBase;    // Wolkenmeer cloud base elevation (~4900m)
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> pc;

// 19 Textures: ESRI Satellite, Macro Normal, 4x ambientCG PBR sets, and Gaea Geomorphology Map
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

// 18: Geomorphology Map (R: Couloirs/Flow, G: Talus Scree, B: Ridge Crests, A: Wind Scour)
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

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
    float3 geomN = normalize(input.worldNormal);
    float3 V = normalize(pc.cameraPos.xyz - input.worldPos);
    float3 L = normalize(pc.sunDir.xyz);
    float distToCam = length(pc.cameraPos.xyz - input.worldPos);

    // 1. Sample Macro Textures (ESRI Satellite + Macro Normal + Geomorphology)
    float4 satColor = texSatellite.Sample(input.uv);
    float3 macroNormSample = texMacroNormal.Sample(input.uv).rgb * 2.0 - 1.0;
    float4 geomMorph = texGeomorphology.Sample(input.uv);
    // geomMorph.r = Couloir flow / gully channels (Gaea Hydro-flow)
    // geomMorph.g = Talus scree deposition (Thermal rockfall)
    // geomMorph.b = Sharp ridge crests / arêtes
    // geomMorph.a = Jet stream wind scour

    float3 N = normalize(geomN + macroNormSample * 0.40);

    // 2. Geomorphological Biome Splatting & Physical Erosion Weights
    float slope = 1.0 - clamp(N.y, 0.0, 1.0);
    float elevNorm = clamp((input.worldPos.y - pc.minElev) / max(pc.maxElev - pc.minElev, 1.0), 0.0, 1.0);

    // Couloir Avalanche Chutes:
    // Macro couloirs are guided smoothly by the Gaea geomorphology map (zero moiré at distance)
    float macroCouloir = geomMorph.r;

    // High-frequency procedural couloir ribs active only near the camera (< 350m) to avoid aliasing
    float fluteFade = clamp(1.0 - (distToCam - 20.0) / 330.0, 0.0, 1.0);
    float2 slopeFallLine = normalize(geomN.xz + float2(0.0001, 0.0001));
    float2 perpAcrossSlope = float2(-slopeFallLine.y, slopeFallLine.x);
    float fluteCoord = dot(input.worldPos.xz, perpAcrossSlope);
    float microFlute = (sin(fluteCoord * 0.18) * 0.6 + sin(fluteCoord * 0.42) * 0.4) * fluteFade;

    // Combined couloir chute factor
    float couloirFactor = clamp(macroCouloir * 1.4 + microFlute * 0.35, 0.0, 1.0);

    // Couloir zone occurs on steep slopes (30° to 70°)
    float couloirZone = smoothstep(0.18, 0.45, slope) * (1.0 - smoothstep(0.70, 0.90, slope));
    // In chute troughs, snow clings to the mountain wall
    float fluteSnowCling = couloirFactor * couloirZone * 0.55;

    // Jet Stream Wind Scour vs Leeward Snow Drift
    float windScour = geomMorph.a; // Scoured bare rock on high windward faces
    float windSnowDeposition = (1.0 - windScour) * smoothstep(0.35, 0.60, elevNorm);

    // Steep cliff rock weight (> 40°): bare rock exposed on vertical faces & windward cliffs
    float rockWeight = smoothstep(0.22, 0.46, slope) + (windScour * 0.25);

    // Snow weight: high elevation snowfields + couloir trough accumulation + leeward deposits
    float baseSnowWeight = smoothstep(0.34, 0.56, elevNorm);
    float snowWeight = clamp(baseSnowWeight + fluteSnowCling + windSnowDeposition * 0.15, 0.0, 1.0) * (1.0 - rockWeight * 0.85);

    // Scree / Talus fans: deposited at the base of cliffs and couloirs (angle of repose 25° - 38°)
    float talusConeBoost = geomMorph.g * 1.5;
    float screeWeight = smoothstep(0.12, 0.28, slope) * (1.0 - smoothstep(0.38, 0.52, slope)) * (1.0 - snowWeight) * 0.70 + talusConeBoost * (1.0 - snowWeight);

    // Glacier ice tongue (Khumbu Valley, 4800-5450m gentle slope)
    float glacierMask = smoothstep(0.24, 0.36, elevNorm) * (1.0 - smoothstep(0.46, 0.60, elevNorm));
    float glacierWeight = glacierMask * (1.0 - rockWeight) * (1.0 - snowWeight * 0.6) * 0.70;

    // Normalize splatting weights
    float totalW = rockWeight + snowWeight + screeWeight + glacierWeight + 0.0001;
    rockWeight /= totalW;
    snowWeight /= totalW;
    screeWeight /= totalW;
    glacierWeight /= totalW;

    // 3. Triplanar PBR Mapping for Rock Cliffs (Completely Eliminates Vertical Stretching!)
    // Uniform world-space tiling along X, Y, Z axes
    float rockScale = 1.0 / 25.0; // 25 meters per texture repetition
    float2 uvX = input.worldPos.zy * rockScale;
    float2 uvY = input.worldPos.xz * rockScale;
    float2 uvZ = input.worldPos.xy * rockScale;

    // Sharp triplanar projection blend weights
    float3 blendWeights = pow(abs(geomN), float3(4.0, 4.0, 4.0));
    blendWeights /= (blendWeights.x + blendWeights.y + blendWeights.z + 0.0001);

    // Sample Triplanar Rock Albedo, Normal, and Roughness
    float3 rockAlbedoX = texRockAlbedo.Sample(uvX).rgb;
    float3 rockAlbedoY = texRockAlbedo.Sample(uvY).rgb;
    float3 rockAlbedoZ = texRockAlbedo.Sample(uvZ).rgb;
    float3 triplanarRockAlbedo = rockAlbedoX * blendWeights.x + rockAlbedoY * blendWeights.y + rockAlbedoZ * blendWeights.z;

    float3 rockNormX = texRockNormal.Sample(uvX).rgb * 2.0 - 1.0;
    float3 rockNormY = texRockNormal.Sample(uvY).rgb * 2.0 - 1.0;
    float3 rockNormZ = texRockNormal.Sample(uvZ).rgb * 2.0 - 1.0;

    // World-space normal reorientation for triplanar projection
    float3 wNormX = float3(rockNormX.z * sign(geomN.x), rockNormX.y, rockNormX.x * sign(geomN.x));
    float3 wNormY = float3(rockNormY.x, rockNormY.z * sign(geomN.y), rockNormY.y);
    float3 wNormZ = float3(rockNormZ.x, rockNormZ.y, rockNormZ.z * sign(geomN.z));
    float3 triplanarRockNormal = normalize(wNormX * blendWeights.x + wNormY * blendWeights.y + wNormZ * blendWeights.z);

    float rockRoughX = texRockRoughness.Sample(uvX).r;
    float rockRoughY = texRockRoughness.Sample(uvY).r;
    float rockRoughZ = texRockRoughness.Sample(uvZ).r;
    float triplanarRockRoughness = rockRoughX * blendWeights.x + rockRoughY * blendWeights.y + rockRoughZ * blendWeights.z;

    // 4. Geological Strata: Mount Everest Rock Formations & The "Yellow Band"
    // Dipping plane (~15° northward dip of South Tibetan Detachment system)
    float strataH = input.worldPos.y - 0.22 * input.worldPos.z;
    float3 strataTint = float3(0.92, 0.92, 0.95);

    // Band 1: Greater Himalayan Crystalline (< 7,000m) - Dark paragneiss, granite, schist
    // Band 2: North Col Formation (7,000m - 8,200m) - Pelitic schists & phyllites
    if (strataH >= 7000.0 && strataH < 8180.0) {
        float metaBand = sin(strataH * 0.08) * 0.05 * clamp(1.0 - distToCam / 600.0, 0.0, 1.0);
        strataTint = float3(0.88 + metaBand, 0.86 + metaBand, 0.82);
    }
    // Band 3: The Yellow Band (8,200m - 8,600m) - Distinctive golden recrystallized dolomitic marble!
    else if (strataH >= 8180.0 && strataH < 8600.0) {
        float yellowRhythm = (sin(strataH * 0.14) * 0.15) * clamp(1.0 - distToCam / 500.0, 0.0, 1.0);
        // Rich golden-amber / warm limestone tone
        strataTint = lerp(float3(1.42, 1.22, 0.76), float3(1.26, 1.08, 0.68), yellowRhythm + 0.5);
    }
    // Band 4: Qomolangma Formation (> 8,600m) - Summit Pyramid Ordovician limestone & summit marble
    else if (strataH >= 8600.0) {
        strataTint = float3(1.15, 1.12, 1.08);
    }

    // Apply geological strata tint to exposed rock faces
    triplanarRockAlbedo = clamp(triplanarRockAlbedo * strataTint, 0.0, 1.0);

    // 5. Planar / POM Detail UVs for Snow, Scree & Glacier
    float2 detailUV = input.uv * 1150.0;
    float2 microUV = input.uv * 4500.0;

    // Tangent space view vector approximation for POM
    float3 tangent = normalize(cross(float3(0, 0, 1), geomN));
    float3 bitangent = cross(geomN, tangent);
    float3 viewDirTS = normalize(float3(dot(V, tangent), dot(V, bitangent), dot(V, geomN)));

    // Parallax Occlusion Mapping (POM) active within 150m of player on ground
    float pomFactor = clamp(1.0 - distToCam / 150.0, 0.0, 1.0);
    float2 pbrUV = detailUV;
    if (pomFactor > 0.01) {
        float2 pomOffset = parallaxOcclusion(texSnowDisp, detailUV, viewDirTS, 0.035 * pomFactor);
        pbrUV = lerp(detailUV, pomOffset, pomFactor);
    }

    // Sample Snow, Scree, Glacier PBR Maps
    float3 snowAlbedo = texSnowAlbedo.Sample(pbrUV).rgb;
    float3 snowNorm = texSnowNormal.Sample(pbrUV).rgb * 2.0 - 1.0;
    float snowRough = texSnowRoughness.Sample(pbrUV).r;

    float3 screeAlbedo = texScreeAlbedo.Sample(pbrUV).rgb;
    float3 screeNorm = texScreeNormal.Sample(pbrUV).rgb * 2.0 - 1.0;
    float screeRough = texScreeRoughness.Sample(pbrUV).r;

    float3 glacierAlbedo = texGlacierAlbedo.Sample(pbrUV).rgb;
    float3 glacierNorm = texGlacierNormal.Sample(pbrUV).rgb * 2.0 - 1.0;
    float glacierRough = texGlacierRoughness.Sample(pbrUV).r;

    // Blend Planar Snow/Scree/Glacier
    float3 planarAlbedo = snowAlbedo * snowWeight +
                          screeAlbedo * screeWeight +
                          glacierAlbedo * glacierWeight;

    float3 planarNorm = snowNorm * snowWeight +
                        screeNorm * screeWeight +
                        glacierNorm * glacierWeight;

    float planarRough = snowRough * snowWeight +
                        screeRough * screeWeight +
                        glacierRough * glacierWeight;

    // Composite Triplanar Rock with Planar Surfaces
    float3 detailAlbedo = triplanarRockAlbedo * rockWeight + planarAlbedo;
    float roughness = triplanarRockRoughness * rockWeight + planarRough;

    // High-frequency micro-grain
    float microGrain = texRockRoughness.Sample(microUV).r * 0.10 - 0.05;
    detailAlbedo = clamp(detailAlbedo + microGrain, 0.0, 1.0);

    // 6. Combine Macro Satellite Imagery with High-Resolution Geomorphology Detail
    float detailBlend = clamp(1.0 - (distToCam - 30.0) / 1400.0, 0.0, 1.0);
    float3 compositeAlbedo = lerp(satColor.rgb, detailAlbedo * satColor.rgb * 1.30, detailBlend);

    // Apply distinctive Yellow Band geological marble strata to exposed rock (visible at macro and micro scale)
    float yellowBandMask = smoothstep(8160.0, 8240.0, strataH) * (1.0 - smoothstep(8580.0, 8640.0, strataH));
    float3 strataMacroTint = lerp(float3(1.0, 1.0, 1.0), float3(1.36, 1.18, 0.72), yellowBandMask * rockWeight * 0.70);
    compositeAlbedo *= strataMacroTint;

    // Perturb surface normal: blend triplanar rock normal on cliffs and planar normal on flats
    float3 surfacePerturb = normalize(tangent * planarNorm.x + bitangent * planarNorm.y + geomN * planarNorm.z);
    float3 blendedDetailNormal = normalize(lerp(surfacePerturb, triplanarRockNormal, rockWeight));
    float3 finalNormal = normalize(lerp(N, blendedDetailNormal, detailBlend * 0.70));

    // 7. Stratospheric Himalayan Atmospheric Lighting & Solar Irradiance
    float NdotL = max(dot(finalNormal, L), 0.0);
    
    // Alpenglühen: High summits catch sunrise first while deep valleys are still in mountain shadow
    float dawnSummitBoost = clamp((input.worldPos.y - 4500.0) / 4200.0, 0.0, 1.0);
    float earthShadow = clamp(L.y * 14.0 + dawnSummitBoost * 0.70, 0.0, 1.0);
    float3 directLight = NdotL * pc.sunColor.rgb * earthShadow;

    // Stratospheric Sky Model: Above 6500m, atmospheric pressure is ~1/3 sea-level.
    // The overhead sky darkens from azure into deep dark indigo/navy with stark solar contrast.
    float altRatio = clamp((input.worldPos.y - 5000.0) / 3848.0, 0.0, 1.0);
    float skyHemisphere = clamp(finalNormal.y * 0.5 + 0.5, 0.0, 1.0);
    float3 lowAltSky = lerp(float3(0.12, 0.15, 0.20), float3(0.32, 0.44, 0.62), skyHemisphere);
    float3 highAltSky = lerp(float3(0.04, 0.05, 0.10), float3(0.14, 0.20, 0.38), skyHemisphere);
    float3 skyLight = lerp(lowAltSky, highAltSky, altRatio);

    // Microfacet Specular: Diamond dust ice crystals on firn snow & sun glint
    float3 H = normalize(L + V);
    float NdotH = max(dot(finalNormal, H), 0.0);
    float specPower = lerp(12.0, 96.0, 1.0 - roughness);
    float specIntensity = (snowWeight * 0.50 + glacierWeight * 0.90 + rockWeight * 0.12);
    
    // Snow glitter sparkle from microfacet grain
    float sparkle = pow(max(dot(reflect(-L, finalNormal), V), 0.0), 128.0) * snowWeight * 0.45;
    float specular = pow(NdotH, specPower) * specIntensity * NdotL * earthShadow + sparkle;

    // Fresnel sheen at glancing angles (ice reflection & snow rim light)
    float fresnel = pow(1.0 - max(dot(V, finalNormal), 0.0), 5.0) * (snowWeight * 0.35 + glacierWeight * 0.65);

    float3 litColor = compositeAlbedo * (directLight + skyLight) + float3(specular + fresnel, specular + fresnel, specular + fresnel);

    // 8. Volumetric Valley Cloud Sea (Wolkenmeer):
    // Dense cloud ocean blankets the valleys below 5400m, while the 8000m giants pierce the clouds!
    float cloudDensitySetting = pc.cameraPos.w;
    if (cloudDensitySetting > 0.05) {
        float cloudDeckBottom = pc.cloudBase - 250.0;
        float cloudDeckTop = pc.cloudBase + 600.0;

        float3 rayDir = normalize(input.worldPos - pc.cameraPos.xyz);
        float rayDist = distToCam;

        // Slab intersection
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

            // Forward Mie Scattering phase function for luminous cloud tops
            float cosTheta = dot(rayDir, L);
            float phaseMie = 0.5 * (1.0 + cosTheta * cosTheta) / pow(1.0 + 0.55 * 0.55 - 2.0 * 0.55 * cosTheta, 1.5);
            float3 sunlitCloud = pc.sunColor.rgb * (0.85 + 0.40 * phaseMie);
            float3 ambientCloud = lerp(float3(0.18, 0.22, 0.32), float3(0.55, 0.65, 0.82), skyHemisphere);
            float3 cloudColor = sunlitCloud + ambientCloud;

            litColor = lerp(cloudColor, litColor, cloudTransmittance);
        }
    }

    // 9. Altitude-Aware Himalayan Rayleigh Scattering / Distance Fog
    float fogDensity = lerp(0.000030, 0.000015, altRatio);
    float fogFactor = 1.0 - exp(-distToCam * fogDensity);
    float3 fogColor = lerp(float3(0.55, 0.68, 0.84), float3(0.28, 0.38, 0.58), altRatio);
    float3 finalColor = lerp(litColor, fogColor, clamp(fogFactor, 0.0, 0.80));

    // 10. Blizzard, Spindrift & Whiteout Storm Simulation
    float blizzard = pc.sunColor.w;
    if (blizzard > 0.01) {
        // Spindrift blown off ridge crests and sharp peaks
        float2 stormCoord = input.worldPos.xz * 0.08 + float2(pc.time * 55.0, pc.time * 25.0);
        float spindriftNoise = sin(stormCoord.x) * cos(stormCoord.y) * 0.5 + 0.5;
        float ridgeExposure = geomMorph.b * 1.5 + (1.0 - N.y) * 0.8;
        float spindriftPlume = pow(spindriftNoise, 3.0) * ridgeExposure * blizzard * 0.40;

        // Severe whiteout visibility collapse (30m survival conditions)
        float stormFogDensity = lerp(fogDensity, 0.00035, blizzard);
        float stormFogFactor = 1.0 - exp(-distToCam * stormFogDensity);
        float3 stormFogColor = float3(0.85, 0.88, 0.92);

        finalColor = lerp(finalColor, stormFogColor, clamp(stormFogFactor * (1.0 + blizzard * 4.5), 0.0, 0.96)) + 
                     float3(spindriftPlume, spindriftPlume, spindriftPlume);
    }

    return float4(finalColor, 1.0);
}

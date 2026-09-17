// Whiteout Delarus - Step 19 (1:1 Part XVII): Hardware Ray-Traced Multi-Bounce GI
// "The Western Cwm Phenomenon" (Das Glutofen-Schneelicht)
// Vulkan 1.3+ / 1.4 Dynamic Rendering & Compute (Slang)
// Simulates up to 8 indirect radiative transfer bounces between the 3,000m walls
// of Mount Everest (8,848m), Lhotse (8,516m), and Nuptse (7,861m).

struct GIPushConstants {
    float4 sunDir;         // xyz = normalized sun direction, w = windSpeed
    float4 sunColor;       // rgb = sun radiance spectrum, w = blizzardFactor
    float4 cameraPos;      // xyz = camera world pos, w = time
    float4 cwmCenter;      // xy = Western Cwm center XZ (-12500.0, -8200.0), z = innerRadius (1800.0), w = outerRadius (3800.0)
    uint bounceIndex;      // 0..8
    uint totalBounces;     // 8
    float minElev;         // 3651.0m
    float maxElev;         // 8780.0m
};

[[vk::push_constant]]
ConstantBuffer<GIPushConstants> pc;

[[vk::binding(0, 0)]] Sampler2D texElevationDEM;
[[vk::binding(1, 0)]] Sampler2D texGeomorphology;
[[vk::binding(2, 0)]] Sampler2D texMacroNormal;

[format("rgba16f")]
[[vk::binding(3, 0)]] RWTexture2D<float4> texMultiBounceGI;

// Smooth C1 Catmull-Rom DEM sampling
float sampleDemSmooth(float2 uv) {
    float2 texSize = float2(1024.0, 1024.0);
    float2 coord = uv * texSize - 0.5;
    float2 f = frac(coord);
    float2 smoothF = f * f * (3.0 - 2.0 * f);
    float2 smoothUV = (floor(coord) + 0.5 + smoothF) / texSize;
    return texElevationDEM.SampleLevel(smoothUV, 0.0).r;
}

// Cone-tracing DEM shadow test for primary direct injection
float evaluateDEMShadow(float3 worldPos, float3 L) {
    if (L.y <= 0.01) return 0.0;

    float meshWidth = 34560.0;
    float meshDepth = 34560.0;
    float2 startUV = (worldPos.xz / float2(meshWidth, meshDepth)) + 0.5;

    float stepMeters = 52.0;
    float2 stepUV = (L.xz * stepMeters) / float2(meshWidth, meshDepth);
    float stepElev = L.y * stepMeters;

    float2 currUV = startUV + stepUV * 2.0;
    float rayY = worldPos.y + stepElev * 2.0 + 2.0;
    float minRatio = 1.0;

    for (int i = 0; i < 20; i++) {
        if (currUV.x < 0.0 || currUV.x > 1.0 || currUV.y < 0.0 || currUV.y > 1.0) break;
        if (rayY > 8855.0) break;

        float h = texElevationDEM.SampleLevel(currUV, 0.0).r;
        float diff = h - rayY;
        if (diff > 0.0) return 0.0;

        float coneRadius = float(i + 1) * stepMeters * 0.00925;
        minRatio = min(minRatio, -diff / max(coneRadius, 0.001));

        currUV += stepUV;
        rayY += stepElev;
    }

    return smoothstep(0.0, 1.0, clamp(minRatio, 0.0, 1.0));
}

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID) {
    uint2 texDim;
    texMultiBounceGI.GetDimensions(texDim.x, texDim.y);

    if (dispatchThreadID.x >= texDim.x || dispatchThreadID.y >= texDim.y) {
        return;
    }

    float2 uv = (float2(dispatchThreadID.xy) + 0.5) / float2(texDim);
    float meshWidth = 34560.0;
    float meshDepth = 34560.0;
    float2 worldXZ = (uv - 0.5) * float2(meshWidth, meshDepth);

    // 1. Geodetic terrain position & surface normal
    float elev = sampleDemSmooth(uv);
    float3 worldPos = float3(worldXZ.x, elev, worldXZ.y);

    float demEps = 48.0;
    float2 uvR = ((worldXZ + float2(demEps, 0.0)) / float2(meshWidth, meshDepth)) + 0.5;
    float2 uvL = ((worldXZ - float2(demEps, 0.0)) / float2(meshWidth, meshDepth)) + 0.5;
    float2 uvU = ((worldXZ + float2(0.0, demEps)) / float2(meshWidth, meshDepth)) + 0.5;
    float2 uvD = ((worldXZ - float2(0.0, demEps)) / float2(meshWidth, meshDepth)) + 0.5;

    float hR = sampleDemSmooth(uvR);
    float hL = sampleDemSmooth(uvL);
    float hU = sampleDemSmooth(uvU);
    float hD = sampleDemSmooth(uvD);

    float dx = (hR - hL) / (2.0 * demEps);
    float dz = (hU - hD) / (2.0 * demEps);
    float3 N = normalize(float3(-dx, 1.0, -dz));
    float slopeDeg = degrees(acos(clamp(N.y, 0.0, 1.0)));

    // 2. Physical Surface Albedo
    float4 morph = texGeomorphology.SampleLevel(uv, 0.0);
    float aboveSnowline = smoothstep(4900.0, 5200.0, elev);
    float cliffShed = smoothstep(36.0, 48.0, slopeDeg);
    float snowPresence = aboveSnowline * (1.0 - cliffShed);

    float3 snowAlbedo = float3(0.94, 0.95, 0.98);
    float3 rockAlbedo = float3(0.18, 0.18, 0.20);
    float3 albedo = lerp(rockAlbedo, snowAlbedo, snowPresence);

    // 3. Western Cwm Glacial Amphitheater Enclosure Weight
    // Center: X = -12,500m, Z = -8,200m | Elevation 6,000m - 6,800m
    float2 cwmCenter = pc.cwmCenter.xy;
    float distToCwm = length(worldXZ - cwmCenter);
    float cwmEnclosure = (1.0 - smoothstep(pc.cwmCenter.z, pc.cwmCenter.w, distToCwm)) *
                         smoothstep(5800.0, 6100.0, elev) *
                         (1.0 - smoothstep(7100.0, 7400.0, elev));

    // Sun radiance & direction
    float3 L = normalize(pc.sunDir.xyz);
    float3 sunRadiance = pc.sunColor.rgb * (1.0 - pc.sunColor.w * 0.85);

    // =========================================================================
    // MULTI-BOUNCE RADIATIVE TRANSFER ACROSS 3,000M EVEREST/LHOTSE/NUPTSE WALLS
    // =========================================================================
    // Ray-marched diffuse flux exchange between the valley floor and the surrounding
    // towering faces of Everest (North), Lhotse (East), and Nuptse (South).
    float3 accumulatedIncoming = float3(0.0);
    float totalWeight = 0.0;

    // 8 Azimuth Compass Directions (45° intervals)
    for (int dirIdx = 0; dirIdx < 8; dirIdx++) {
        float angle = float(dirIdx) * (6.2831853 / 8.0);
        float2 rayDir2D = float2(cos(angle), sin(angle));

        // Sample along ray outwards to detect opposing mountain walls
        // Distances: 450m (close serac wall), 1200m (amphitheater base), 2400m (towering face), 3800m (far ridge)
        float sampleDists[4] = { 450.0, 1200.0, 2400.0, 3800.0 };

        for (int step = 0; step < 4; step++) {
            float dist = sampleDists[step];
            float2 targetXZ = worldXZ + rayDir2D * dist;
            float2 targetUV = (targetXZ / float2(meshWidth, meshDepth)) + 0.5;

            if (targetUV.x < 0.0 || targetUV.x > 1.0 || targetUV.y < 0.0 || targetUV.y > 1.0) continue;

            float targetElev = sampleDemSmooth(targetUV);
            float elevDiff = targetElev - elev;

            // Opposing wall towering above current surface
            float wallElevationAngle = atan2(elevDiff, dist);
            if (wallElevationAngle > 0.08) { // Wall rises at least ~5° above local horizon
                float3 wallPos = float3(targetXZ.x, targetElev, targetXZ.y);
                float3 rayDir3D = normalize(wallPos - worldPos);

                float cosTheta = max(dot(N, rayDir3D), 0.0);
                if (cosTheta > 0.01) {
                    // Calculate opposing wall normal & direct illumination
                    float2 wUvR = ((targetXZ + float2(demEps, 0.0)) / float2(meshWidth, meshDepth)) + 0.5;
                    float2 wUvL = ((targetXZ - float2(demEps, 0.0)) / float2(meshWidth, meshDepth)) + 0.5;
                    float2 wUvU = ((targetXZ + float2(0.0, demEps)) / float2(meshWidth, meshDepth)) + 0.5;
                    float2 wUvD = ((targetXZ - float2(0.0, demEps)) / float2(meshWidth, meshDepth)) + 0.5;

                    float wdx = (sampleDemSmooth(wUvR) - sampleDemSmooth(wUvL)) / (2.0 * demEps);
                    float wdz = (sampleDemSmooth(wUvU) - sampleDemSmooth(wUvD)) / (2.0 * demEps);
                    float3 wallNormal = normalize(float3(-wdx, 1.0, -wdz));
                    float wallSlope = degrees(acos(clamp(wallNormal.y, 0.0, 1.0)));

                    float wallSnow = smoothstep(4900.0, 5200.0, targetElev) * (1.0 - smoothstep(36.0, 48.0, wallSlope));
                    float3 wallAlbedo = lerp(rockAlbedo, snowAlbedo, wallSnow);

                    float wallNdotL = max(dot(wallNormal, L), 0.0);
                    float wallShadow = evaluateDEMShadow(wallPos, L);
                    float3 wallDirectRadiance = wallAlbedo * sunRadiance * wallNdotL * wallShadow * 3.6;

                    // Form factor & geometric solid angle transfer:
                    // Massive walls (Lhotse/Everest/Nuptse) subtend huge solid angles in the Western Cwm
                    float solidAngle = sin(clamp(wallElevationAngle, 0.0, 1.45)) * (1.0 + cwmEnclosure * 1.85);
                    float weight = cosTheta * solidAngle / (1.0 + dist * 0.00035);

                    accumulatedIncoming += wallDirectRadiance * weight;
                    totalWeight += weight;
                }
            }
        }
    }

    float3 bounce1_irradiance = (totalWeight > 0.001) ? (accumulatedIncoming / totalWeight) : float3(0.0);

    // =========================================================================
    // Multi-Bounce Iterative Radiative Transfer (Bounces 1..8):
    // =========================================================================
    // In the glacial amphitheater of the Western Cwm ("Glutofen"),
    // fresh snow has an albedo of ~0.95 and high surrounding walls reflect the trapped flux back and forth.
    float bounceFeedback = (0.76 + cwmEnclosure * 0.22) * snowPresence;
    float3 totalMultiBounce = float3(0.0);
    float3 currentBounce = bounce1_irradiance;

    uint bounces = max(pc.totalBounces, 1u);
    for (uint b = 1; b <= bounces; b++) {
        totalMultiBounce += currentBounce;
        // Radiative transfer to next bounce attenuated by snow albedo & enclosure
        currentBounce = currentBounce * albedo * bounceFeedback;
    }

    // In the Western Cwm amphitheater, multi-bounce light radiates with dazzling intensity (blinding white-gold)
    if (cwmEnclosure > 0.05) {
        float cwmGlutofenBoost = cwmEnclosure * 0.65;
        totalMultiBounce += albedo * float3(0.96, 0.97, 1.0) * cwmGlutofenBoost;
    }

    texMultiBounceGI[dispatchThreadID.xy] = float4(totalMultiBounce, 1.0);
}

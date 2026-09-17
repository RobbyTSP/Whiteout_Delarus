// Whiteout Delarus - Step 20 (1:1 Part XVIII): Dynamic Snow Physics & MPM Deformation
// Simulates elasto-plastic snow deformation under crampons/boots, raised displacement rims,
// compaction hardening, and wind-blown spindrift erosion/filling in a rolling 64m local buffer.

struct FootstepImpulse {
    float4 posRadius; // xyz = world pos, w = radius / length (m)
    float4 dirDepth;  // xy = normalized dir (cos/sin yaw), z = indentation depth (m), w = compaction (0..1)
};

struct SnowPhysicsPushConstants {
    float4 snowCenterSpan; // xy = center XZ, z = span (64.0m), w = deltaTime
    float4 windParams;     // xy = wind direction * windSpeed, z = blizzardFactor, w = footstepCount
    FootstepImpulse footsteps[4]; // Up to 4 active footsteps per frame
};

[[vk::push_constant]]
ConstantBuffer<SnowPhysicsPushConstants> pc;

[format("rgba16f")]
[[vk::binding(0, 0)]] RWTexture2D<float4> texSnowDeformation;

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID) {
    uint2 texDim;
    texSnowDeformation.GetDimensions(texDim.x, texDim.y);

    if (dispatchThreadID.x >= texDim.x || dispatchThreadID.y >= texDim.y) {
        return;
    }

    float span = pc.snowCenterSpan.z; // 64.0m
    float texelSize = span / float(texDim.x); // 0.0625m
    float2 camXZ = pc.snowCenterSpan.xy;
    float2 texelBaseWorld = (float2(dispatchThreadID.xy) + 0.5) * texelSize;

    // Toroidal world shift: lock texels to fixed world coordinates modulo span
    float2 worldXZ = texelBaseWorld + floor((camXZ - texelBaseWorld) / span + 0.5) * span;
    float distToCam = length(worldXZ - camXZ);
    float dt = pc.snowCenterSpan.w;

    // Read current state:
    // r = indentation depth (negative meters)
    // g = compaction hardening (0..1, firm icy base)
    // b = raised displaced rim (positive meters)
    // a = fresh wind drift powder (0..1)
    float4 state = texSnowDeformation[dispatchThreadID.xy];

    // Boundary clear: smoothly clear texels entering/leaving the 64m local window
    if (distToCam > 30.5) {
        state = lerp(state, float4(0.0), clamp((distToCam - 30.5) / 1.5, 0.0, 1.0));
    }

    // =========================================================================
    // 1. Elasto-Plastic Footstep Stamping (Crampons & Mountaineering Boots)
    // =========================================================================
    uint stepCount = min(uint(pc.windParams.w), 4u);
    for (uint i = 0; i < stepCount; i++) {
        FootstepImpulse foot = pc.footsteps[i];
        float2 footPos = foot.posRadius.xz;
        float bootLength = foot.posRadius.w;
        if (bootLength <= 0.001) continue;

        float bootWidth = bootLength * 0.42;
        float2 fwd = foot.dirDepth.xy;
        float2 right = float2(-fwd.y, fwd.x);

        float2 d = worldXZ - footPos;
        float localX = dot(d, right);
        float localY = dot(d, fwd);

        // Anatomical boot sole outline: slightly wider at forefoot, narrower at heel
        float soleHalfWidth = (bootWidth * 0.5) * (1.0 + 0.18 * smoothstep(-0.06, 0.12, localY));
        float soleHalfLength = bootLength * 0.5;

        // Box distance to boot contour
        float qX = max(abs(localX) - soleHalfWidth, 0.0);
        float qY = max(abs(localY) - soleHalfLength, 0.0);
        float distToBoot = length(float2(qX, qY));

        // Inside boot print (crushing snow crust and compacting firm sublayer)
        if (abs(localX) <= soleHalfWidth && abs(localY) <= soleHalfLength) {
            float heelWeight = 1.0 + 0.25 * smoothstep(-0.14, -0.04, localY);
            float maxDepth = -foot.dirDepth.z * heelWeight;

            // Micro-crampon tread teeth pattern (grooved indentations inside bootprint)
            float treadFluting = sin(localY * 110.0) * 0.012;
            float targetDepth = maxDepth + treadFluting;

            state.r = min(state.r, targetDepth);
            state.g = max(state.g, foot.dirDepth.w); // compaction (icy firm base)
            state.b = 0.0; // no rim inside the footstep
        } else if (distToBoot < 0.12) {
            // Displaced snow berm (Verdrängungswulst): snow pushed out and up around boot perimeter
            float rimRatio = sin(clamp(distToBoot / 0.12, 0.0, 1.0) * 3.14159);
            float rimHeight = foot.dirDepth.z * 0.45 * rimRatio;
            state.b = max(state.b, rimHeight);
        }
    }

    // =========================================================================
    // 2. Wind Spindrift Erosion & Powder Accumulation
    // =========================================================================
    // High-altitude winds carry loose spindrift that slowly fills depressions
    float windSpeed = length(pc.windParams.xy);
    float blizzard = pc.windParams.z;
    float fillRate = (windSpeed * 0.0004 + blizzard * 0.0028) * dt;

    if (state.r < 0.0) {
        state.r = min(state.r + fillRate, 0.0f);
    }
    if (state.b > 0.0) {
        state.b = max(state.b - fillRate * 1.4f, 0.0f);
    }
    if (state.g > 0.0) {
        state.g = max(state.g - fillRate * 0.6f, 0.0f);
    }

    texSnowDeformation[dispatchThreadID.xy] = state;
}

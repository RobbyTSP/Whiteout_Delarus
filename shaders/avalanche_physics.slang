// Whiteout Delarus - Step 20 (1:1 Part XVIII): Real-Time GPU Powder Avalanche Physics
// Simulates 8,192 powder avalanche billows cascading down the 1,100m Lhotse Face couloirs
// into the Western Cwm with physical gravity, DEM collision, air entrainment, and turbulent vorticity.

struct AvalancheParticle {
    float4 posRadius;  // xyz = world position, w = radius / scale (m)
    float4 velLife;    // xyz = velocity (m/s), w = remaining life (0..1)
    float4 params;     // x = density/opacity, y = vorticity/spin, z = turbulence, w = active flag
};

struct AvalanchePushConstants {
    float4 originTrigger; // xyz = avalanche release point, w = triggerActive (1.0 or 0.0)
    float4 windDirSpeed;  // xyz = normalized wind direction, w = windSpeed (km/h)
    float deltaTime;
    float totalTime;
    uint particleCount;   // 8192
    float blastFactor;    // 0..1 whiteout blast intensity
};

[[vk::push_constant]]
ConstantBuffer<AvalanchePushConstants> pc;

[[vk::binding(0, 0)]] Sampler2D texElevationDEM;
[[vk::binding(1, 0)]] RWStructuredBuffer<AvalancheParticle> bufParticles;

// Fast pseudo-random generator
float hash(uint seed) {
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return float(seed) / 4294967295.0;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID) {
    uint idx = dispatchThreadID.x;
    if (idx >= pc.particleCount) return;

    AvalancheParticle p = bufParticles[idx];
    float dt = min(pc.deltaTime, 0.05);

    // =========================================================================
    // 1. Particle Spawning & Release on the Lhotse Face (7,500m -> 6,400m)
    // =========================================================================
    bool isDead = (p.velLife.w <= 0.0) || (p.params.w < 0.5);

    if (isDead && pc.originTrigger.w > 0.5) {
        float r1 = hash(idx * 17u + uint(pc.totalTime * 60.0));
        float r2 = hash(idx * 31u + uint(pc.totalTime * 40.0));
        float r3 = hash(idx * 53u + uint(pc.totalTime * 25.0));

        // Progressive distribution along the 1,100m Lhotse couloir chute:
        float tChute = hash(idx * 47u);
        float3 topPos = pc.originTrigger.xyz;
        // Runout path: connects crown fracture down to valley floor
        float3 botPos = (topPos.x > -10500.0 && topPos.x < -9000.0 && topPos.y > 7000.0)
            ? float3(-11650.0, 6420.0, -7550.0) // Lhotse Face couloir runout
            : topPos + float3(-1600.0, -900.0, (r2 - 0.5) * 400.0);

        float3 chutePos = lerp(topPos, botPos, tChute);
        float lateralSpread = lerp(45.0, 190.0, tChute);
        float2 slabOffset = float2((r1 - 0.5) * lateralSpread, (r2 - 0.5) * (lateralSpread * 0.65));
        float3 releasePos = chutePos + float3(slabOffset.x, (r3 - 0.5) * 35.0, slabOffset.y);

        p.posRadius.xyz = releasePos;
        p.posRadius.w = lerp(8.0, 32.0, tChute) * lerp(0.85, 1.25, r1);

        // Velocity down the steep 50° couloir (towards West: -X and -Y)
        float speed = lerp(32.0, 65.0, tChute);
        p.velLife.xyz = float3(-speed * 0.85, -speed * 0.48, (r3 - 0.5) * 12.0);
        p.velLife.w = lerp(0.95, 0.20, tChute);

        p.params.x = lerp(0.65, 0.98, r1); // Opacity
        p.params.y = (r2 - 0.5) * 3.5;      // Vorticity spin
        p.params.z = r3;                    // Random phase
        p.params.w = 1.0;                   // Active!

        bufParticles[idx] = p;
        return;
    }

    if (p.params.w < 0.5) return; // Inactive particle

    // =========================================================================
    // 2. Physical Avalanche Dynamics: Gravity, Aerodynamic Drag & Expansion
    // =========================================================================
    float3 pos = p.posRadius.xyz;
    float3 vel = p.velLife.xyz;
    float radius = p.posRadius.w;

    // Gravity acceleration
    vel += float3(0.0, -9.81, 0.0) * dt;

    // Wind influence & entrainment
    float3 wind = pc.windDirSpeed.xyz * (pc.windDirSpeed.w / 3.6); // km/h to m/s
    vel += (wind - vel) * 0.04 * dt;

    // Aerodynamic drag
    float speed = length(vel);
    vel -= vel * (0.012 + 0.0004 * speed) * dt;

    // Turbulent cloud expansion: powder billows entrain air and grow as they tumble
    radius = min(radius + (1.2 + speed * 0.035) * dt, 75.0);

    // =========================================================================
    // 3. Terrain Collision with 35-km DEM Heightfield
    // =========================================================================
    float meshWidth = 34560.0;
    float meshDepth = 34560.0;
    float2 demUV = (pos.xz / float2(meshWidth, meshDepth)) + 0.5;

    if (demUV.x >= 0.0 && demUV.x <= 1.0 && demUV.y >= 0.0 && demUV.y <= 1.0) {
        float terrainElev = texElevationDEM.SampleLevel(demUV, 0.0).r;
        float groundContactY = terrainElev + radius * 0.72;

        if (pos.y <= groundContactY) {
            pos.y = groundContactY;

            // Surface normal from DEM
            float eps = 32.0;
            float2 uvR = ((pos.xz + float2(eps, 0.0)) / float2(meshWidth, meshDepth)) + 0.5;
            float2 uvL = ((pos.xz - float2(eps, 0.0)) / float2(meshWidth, meshDepth)) + 0.5;
            float2 uvU = ((pos.xz + float2(0.0, eps)) / float2(meshWidth, meshDepth)) + 0.5;
            float2 uvD = ((pos.xz - float2(0.0, eps)) / float2(meshWidth, meshDepth)) + 0.5;

            float hR = texElevationDEM.SampleLevel(uvR, 0.0).r;
            float hL = texElevationDEM.SampleLevel(uvL, 0.0).r;
            float hU = texElevationDEM.SampleLevel(uvU, 0.0).r;
            float hD = texElevationDEM.SampleLevel(uvD, 0.0).r;

            float3 N = normalize(float3(-(hR - hL) / (2.0 * eps), 1.0, -(hU - hD) / (2.0 * eps)));

            // Tangential couloir guide: deflect velocity along slope
            float vDotN = dot(vel, N);
            if (vDotN < 0.0) {
                vel -= vDotN * N; // Cancel penetration
            }

            // Normal billow bounce: high-speed powder impacts kick billowing clouds upward!
            vel += N * min(speed * 0.22, 14.0);

            // Friction along snow/ice face
            vel *= (1.0 - 0.025 * dt);
        }
    }

    // =========================================================================
    // 4. Turbulent Vorticity Swirl & Life Decay
    // =========================================================================
    float swirlAngle = pc.totalTime * p.params.y + p.params.z * 6.28;
    float3 swirlDir = float3(cos(swirlAngle), sin(swirlAngle * 1.5) * 0.5, sin(swirlAngle));
    pos += (vel + swirlDir * (radius * 0.15)) * dt;

    // Decay life over ~25-35 seconds
    p.velLife.w -= (0.028 + 0.008 * (1.0 - p.params.z)) * dt;
    if (p.velLife.w <= 0.0) {
        p.params.w = 0.0; // Mark inactive
    }

    p.posRadius = float4(pos, radius);
    p.velLife.xyz = vel;
    bufParticles[idx] = p;
}

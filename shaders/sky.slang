// Whiteout Delarus - Step 11 (1:1 Part IX): Stratospheric Dynamic Sky & Atmospheric Dome (Slang)
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
    float maxElev;      // 8780.0m
    float cloudBase;    // Wolkenmeer cloud base elevation (~4900m)
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> pc;

struct SkyVertexOutput {
    float4 sv_position : SV_Position;
    float2 clipPos     : TEXCOORD0;
};

[shader("vertex")]
SkyVertexOutput vertexMain(uint vertexId : SV_VertexID) {
    SkyVertexOutput output;
    // Fullscreen triangle covering clip space [-1, 1]
    // vertexId 0: UV (0, 0) -> clip (-1, -1)
    // vertexId 1: UV (2, 0) -> clip ( 3, -1)
    // vertexId 2: UV (0, 2) -> clip (-1,  3)
    float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
    output.clipPos = uv * 2.0 - 1.0;
    // Far plane depth in Vulkan [0, 1] NDC
    output.sv_position = float4(output.clipPos, 1.0, 1.0);
    return output;
}

// ACES Filmic Tone Mapping (identical curve to terrain shader for optical coherence)
float3 ACESFilm(float3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

[shader("fragment")]
float4 fragmentMain(SkyVertexOutput input) : SV_Target {
    // 1. Unproject camera ray into World Space
    float xCam = input.clipPos.x / pc.proj[0][0];
    float yCam = input.clipPos.y / pc.proj[1][1];

    // In view matrix:
    // row 0 is Camera Right, row 1 is Camera Up, row 2 is -Forward
    float3 right = float3(pc.view[0][0], pc.view[0][1], pc.view[0][2]);
    float3 up    = float3(pc.view[1][0], pc.view[1][1], pc.view[1][2]);
    float3 fwd   = -float3(pc.view[2][0], pc.view[2][1], pc.view[2][2]);
    float3 rayDir = normalize(fwd + right * xCam + up * yCam);

    float3 L = normalize(pc.sunDir.xyz);
    float cosThetaSun = dot(rayDir, L);

    // 2. Stratospheric Altitude Ratio (4,500m base to 8,848m summit)
    float camAlt = clamp((pc.cameraPos.y - 4500.0) / 4348.0, 0.0, 1.0);

    // 3. Dynamic Zenith Color Progression
    // In deep valleys: rich alpine azure; in high stratosphere: dark indigo-navy
    float3 valleyZenith = float3(0.12, 0.24, 0.54);
    float3 stratoZenith = float3(0.022, 0.040, 0.13);
    float3 zenithColor = lerp(valleyZenith, stratoZenith, camAlt * 0.85);

    // 4. Horizon Rayleigh & Mie Atmospheric Scattering
    // Atmospheric optical path length increases towards the horizon
    float horizonFactor = clamp(1.0 - max(rayDir.y, 0.0), 0.0, 1.0);
    float horizonGlowPower = pow(horizonFactor, 3.2);

    // Daylight horizon color: brilliant crisp azure/cyan
    float3 dayHorizonColor = float3(0.52, 0.70, 0.90);

    // Sunrise / Sunset Alpenglow Horizon (low sun elevation):
    // Short wavelengths are scattered away, leaving fiery orange, amber, and rose-gold horizon glow
    float sunElev = clamp(L.y, 0.0, 1.0);
    float duskDawnFactor = 1.0 - smoothstep(0.05, 0.40, sunElev);

    // Forward sunset glow along sun azimuth
    float horizSunAlign = clamp(dot(normalize(float2(rayDir.x, rayDir.z)), normalize(float2(L.x, L.z))), -1.0, 1.0);
    float sunsetForwardGlow = pow(clamp(horizSunAlign * 0.5 + 0.5, 0.0, 1.0), 2.2);

    float3 sunsetHorizonColor = pc.sunColor.rgb * float3(1.20, 0.62, 0.32) * (0.65 + 0.85 * sunsetForwardGlow);

    // Gegendämmerung / Belt of Venus (anti-sun pink band on opposite horizon at sunset)
    float antiSunAlign = clamp(-horizSunAlign, 0.0, 1.0);
    float beltOfVenus = pow(antiSunAlign, 2.0) * duskDawnFactor * smoothstep(-0.05, 0.20, rayDir.y) * (1.0 - smoothstep(0.15, 0.45, rayDir.y));
    float3 beltOfVenusColor = float3(0.85, 0.48, 0.65) * beltOfVenus * 0.75;

    // Earth's shadow below Belt of Venus
    float earthShadowBand = duskDawnFactor * smoothstep(-0.15, 0.02, -rayDir.y) * antiSunAlign;
    float3 earthShadowColor = float3(0.035, 0.055, 0.14);

    // Base sky gradient
    float3 horizonColor = lerp(dayHorizonColor, sunsetHorizonColor, duskDawnFactor);
    float3 skyGrad = lerp(zenithColor, horizonColor, horizonGlowPower);
    skyGrad += beltOfVenusColor;
    skyGrad = lerp(skyGrad, earthShadowColor, earthShadowBand * 0.65);

    // Below-horizon valley haze blending
    if (rayDir.y < 0.0) {
        float valleyDip = clamp(-rayDir.y * 3.5, 0.0, 1.0);
        float3 valleyMist = lerp(horizonColor * 0.85, float3(0.18, 0.23, 0.34), valleyDip);
        skyGrad = lerp(skyGrad, valleyMist, valleyDip);
    }

    // 5. Sun Disc & Solar Glare (Mie Scattering Corona)
    if (L.y > -0.05) {
        // Direct Sun Disc (sharp 0.5° angular diameter)
        float sunDisc = smoothstep(0.9997, 0.99995, cosThetaSun) * 35.0;

        // Circumsolar Corona (ice crystal forward diffraction)
        float corona = pow(clamp(cosThetaSun, 0.0, 1.0), 384.0) * 6.0;

        // Wide Atmospheric Glare / Aureole
        float aureole = pow(clamp(cosThetaSun, 0.0, 1.0), 28.0) * 1.35;

        float3 solarLighting = (sunDisc + corona + aureole) * pc.sunColor.rgb;
        skyGrad += solarLighting;
    }

    // 6. Blizzard / Storm Overcast Simulation
    float blizzard = pc.sunColor.w;
    float cloudDensity = pc.cameraPos.w;
    float overcast = max(blizzard, smoothstep(0.60, 0.95, cloudDensity) * 0.75);

    if (overcast > 0.01) {
        float3 stormSky = float3(0.82, 0.86, 0.90);
        skyGrad = lerp(skyGrad, stormSky, overcast * 0.92);
    }

    // 7. ACES Tone Mapping
    float exposure = 0.82;
    float3 finalSky = ACESFilm(skyGrad * exposure);

    return float4(finalSky, 1.0);
}

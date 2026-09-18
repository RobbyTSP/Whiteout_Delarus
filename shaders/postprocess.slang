// Whiteout Delarus - Step 22 (1:1 Part XX): Visceral Mountaineer Cryo-Optics (Slang)
// Vulkan 1.3+ / 1.4 Dynamic Rendering
// - Fullscreen Triangle Rasterization
// - Physical Category-4 Glacier Goggles with Brewster's Angle Polarization Filter
// - High-Altitude Amber Contrast Curve & Ergonomic Leather Side-Shield Vignette
// - Breath Condensation Diffusion & Upward Rising Mist
// - Sub-Zero Fractal Dendritic Ice Crystals (Gefrierende Eisblumen)
// - Photokeratitis (Snow Blindness) Over-Exposure & Corneal Whiteout Flare
// - Death Zone Hypoxia (>8,000m): Tunnel Vision, Cyanotic Desaturation & Cardiovascular Pulse Surge
// - Hexagonal 6-Point Diffraction Sunstars (60° Ice Crystals) & Anamorphic Glare
// - ACES Filmic Tone Mapping Curve

struct PostProcessOutput {
    float4 sv_position : SV_Position;
    float2 uv          : TEXCOORD0;
};

[shader("vertex")]
PostProcessOutput vertexMain(uint vertexId : SV_VertexID) {
    PostProcessOutput output;
    // Fullscreen triangle covering clip space [-1, 1]
    float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
    float2 clipPos = uv * 2.0 - 1.0;
    output.uv = uv;
    output.sv_position = float4(clipPos, 0.0, 1.0);
    return output;
}

[[vk::binding(0, 0)]]
Sampler2D hdrTexture;

[[vk::binding(1, 0)]]
StructuredBuffer<float> exposureBuffer; // [0] = adaptedLum, [1] = exposure, [2] = targetLum

struct PostProcessPushConstants {
    float4 sunScreenPos; // xy = screen UV [0, 1], z = isVisible (0/1), w = sunIntensity
    float2 resolution;   // screen width, height
    float time;          // elapsed time
    float blizzard;      // blizzard factor [0, 1]
    float4 cryoParams1;  // x = gogglesEquipped (0/1), y = gogglesFog (0..1), z = frostAmount (0..1), w = snowBlindness (0..1)
    float4 cryoParams2;  // x = hypoxiaFactor (0..1), y = heartbeatPulse (0..1), z = oxygenSaturation (0..100), w = altitude (m)
};

[[vk::push_constant]]
ConstantBuffer<PostProcessPushConstants> pc;

// ACES Filmic Tone Mapping Curve (Academy Color Encoding System standard)
float3 ACESFilm(float3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Fast 2D pseudo-random hash
float hash21(float2 p) {
    p = frac(p * float2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return frac(p.x * p.y);
}

// 2D Value Noise with smooth Hermite interpolation
float noise2D(float2 p) {
    float2 i = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    
    float a = hash21(i);
    float b = hash21(i + float2(1.0, 0.0));
    float c = hash21(i + float2(0.0, 1.0));
    float d = hash21(i + float2(1.0, 1.0));
    
    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

// Procedural dendritic fractal frost crystal generator (Hexagonal 60° ice lattice)
// Frost forms along cold leather/metal frame margins and lower corners, keeping center clear
float calcFrostDendrites(float2 uv, float frostAmount, float aspect) {
    if (frostAmount <= 0.002) return 0.0;
    
    float2 p = (uv - 0.5) * float2(aspect, 1.0);
    
    // Distance towards goggle frame margins (super-ellipse coordinate)
    float dx = abs(p.x) / (aspect * 0.46);
    float dy = abs(p.y) / 0.48;
    float marginDist = pow(pow(dx, 3.2) + pow(dy, 3.2), 1.0 / 3.2);
    
    // Frost nucleates along cold perimeter (especially lower corners & bottom edge)
    float cornerBias = marginDist + max(0.0, uv.y - 0.45) * 0.25;
    
    // Hexagonal crystal angle (60° branching symmetry of ice crystals)
    float angle = atan2(p.y, p.x);
    float hexAngle = abs(frac(angle * (3.0 / 3.14159265) + 0.5) - 0.5) * 2.0; // [0, 1]
    
    // Domain-warped coordinates for organic crystallization
    float2 q = p * 16.0;
    q += float2(sin(q.y * 3.8 + hexAngle * 3.0), cos(q.x * 3.8)) * 0.35;
    
    float n1 = noise2D(q);
    float n2 = noise2D(q * 2.5 + float2(n1 * 1.5, -n1 * 1.5));
    float n3 = noise2D(q * 5.5 + float2(n2 * 2.0, n2 * 2.0));
    
    // Ridge noise for sharp needle-like ice branches
    float needle1 = 1.0 - abs(n2 * 2.0 - 1.0);
    float needle2 = 1.0 - abs(n3 * 2.0 - 1.0);
    float dendrite = (needle1 * 0.65 + needle2 * 0.35) * (0.70 + 0.5 * hexAngle);
    
    // Growth threshold: frost grows inward from edge (marginDist ~ 1.0 down to ~ 0.65)
    float growthThreshold = 1.05 - frostAmount * 0.45;
    float frostMask = smoothstep(growthThreshold, growthThreshold - 0.16, cornerBias + dendrite * 0.30);
    
    // Ensure central sightline is always crystal clear
    frostMask *= smoothstep(0.40, 0.70, cornerBias);
    
    return clamp(frostMask, 0.0, 1.0);
}

// Breath condensation mist rising from lower mask
float calcBreathFog(float2 uv, float fogAmount, float aspect, float time) {
    if (fogAmount <= 0.002) return 0.0;
    
    // Breath billows up from the bottom center (mouth/balaclava)
    float horizDist = abs(uv.x - 0.5) * 1.4;
    float vertDist = (1.0 - uv.y); // 1.0 at bottom, 0.0 at top
    
    // Soft plume shape: concentrated in bottom half
    float plumeShape = smoothstep(0.15, 0.85, vertDist) * smoothstep(1.1, 0.0, horizDist + (1.0 - vertDist) * 0.6);
    
    // Droplet condensation micro-noise
    float2 mistUV = uv * float2(24.0 * aspect, 24.0);
    float m1 = noise2D(mistUV + float2(0.0, time * 0.05));
    float m2 = noise2D(mistUV * 2.2 - float2(0.0, time * 0.03));
    float mistTexture = m1 * 0.6 + m2 * 0.4;
    
    float fog = plumeShape * (0.4 + 0.6 * mistTexture) * fogAmount * 1.3;
    return clamp(fog, 0.0, 0.88);
}

[shader("fragment")]
float4 fragmentMain(PostProcessOutput input) : SV_Target {
    float2 uv = input.uv;
    float aspect = pc.resolution.x / max(pc.resolution.y, 1.0);

    // Unpack Cryo-Optics Push Constants
    bool gogglesEquipped = (pc.cryoParams1.x > 0.5);
    float fogAmount = pc.cryoParams1.y;
    float frostAmount = pc.cryoParams1.z;
    float snowBlindness = pc.cryoParams1.w;

    float hypoxiaFactor = pc.cryoParams2.x;
    float heartbeatPulse = pc.cryoParams2.y;
    float o2Saturation = pc.cryoParams2.z;
    float altitude = pc.cryoParams2.w;

    float adaptedLum = max(exposureBuffer[0], 0.001);
    float exposure = clamp(exposureBuffer[1], 0.0008, 12.0);

    // =========================================================================
    // 1. Breath Condensation & Frost Dendrites Masks
    // =========================================================================
    float breathFogMask = gogglesEquipped ? calcBreathFog(uv, fogAmount, aspect, pc.time) : 0.0;
    float frostDendriteMask = gogglesEquipped ? calcFrostDendrites(uv, frostAmount, aspect) : 0.0;

    // Breath Condensation Diffusion Blur:
    // Light passing through condensed micro-droplets scatters into a soft diffuse glow
    float3 hdrColor = float3(0.0);
    if (breathFogMask > 0.01) {
        float2 blurStep = float2(1.0 / pc.resolution.x, 1.0 / pc.resolution.y) * (breathFogMask * 5.0);
        float3 baseTap = hdrTexture.SampleLevel(uv, 0.0).rgb;
        float3 tap1 = hdrTexture.SampleLevel(uv + float2(blurStep.x, blurStep.y), 0.0).rgb;
        float3 tap2 = hdrTexture.SampleLevel(uv + float2(-blurStep.x, blurStep.y), 0.0).rgb;
        float3 tap3 = hdrTexture.SampleLevel(uv + float2(blurStep.x, -blurStep.y), 0.0).rgb;
        float3 tap4 = hdrTexture.SampleLevel(uv + float2(-blurStep.x, -blurStep.y), 0.0).rgb;
        float3 blurred = (tap1 + tap2 + tap3 + tap4) * 0.25;
        hdrColor = lerp(baseTap, blurred, breathFogMask * 0.85);
    } else {
        hdrColor = hdrTexture.SampleLevel(uv, 0.0).rgb;
    }

    // =========================================================================
    // 2. Anamorphic Glare & Hexagonal Diffraction Sunstars
    // =========================================================================
    float3 glare = float3(0.0);

    // A. Sun Diffraction Sunstar & Anamorphic Streak
    if (pc.sunScreenPos.z > 0.5) {
        float2 sunUV = pc.sunScreenPos.xy;

        float2 pStep = float2(1.0 / pc.resolution.x, 1.0 / pc.resolution.y) * 4.0;
        float lum0 = dot(hdrTexture.SampleLevel(sunUV, 0.0).rgb, float3(0.2126, 0.7152, 0.0722));
        float lum1 = dot(hdrTexture.SampleLevel(sunUV + float2(pStep.x, 0.0), 0.0).rgb, float3(0.2126, 0.7152, 0.0722));
        float lum2 = dot(hdrTexture.SampleLevel(sunUV - float2(pStep.x, 0.0), 0.0).rgb, float3(0.2126, 0.7152, 0.0722));
        float lum3 = dot(hdrTexture.SampleLevel(sunUV + float2(0.0, pStep.y), 0.0).rgb, float3(0.2126, 0.7152, 0.0722));
        float lum4 = dot(hdrTexture.SampleLevel(sunUV - float2(0.0, pStep.y), 0.0).rgb, float3(0.2126, 0.7152, 0.0722));
        float avgSunLum = (lum0 * 2.0 + lum1 + lum2 + lum3 + lum4) / 6.0;

        float sunOcclusion = smoothstep(1.5, 6.5, avgSunLum);

        if (sunOcclusion > 0.001) {
            float2 dSun = uv - sunUV;
            dSun.x *= aspect;

            float distSun = length(dSun);
            if (distSun > 0.0001 && distSun < 1.3) {
                float angle = atan2(dSun.y, dSun.x);

                // Hexagonal 6-point diffraction rays (60° ice crystal prism faces)
                float hexSpikes = pow(clamp(cos(6.0 * angle) * 0.5 + 0.5, 0.0, 1.0), 80.0);

                float3 sunstarColor = float3(
                    hexSpikes * exp(-distSun * 4.8) * 1.18,
                    hexSpikes * exp(-distSun * 5.2) * 1.04,
                    hexSpikes * exp(-distSun * 5.7) * 0.88
                ) * 3.2;

                // Anamorphic horizontal glare streak
                float horizStreak = exp(-pow(abs(dSun.y) / 0.0018, 2.0)) * exp(-abs(dSun.x) * 2.8);
                float3 streakColor = float3(1.15, 0.98, 0.76) * horizStreak * 2.2;

                // Secondary diagonal cross-rays
                float diagCross = pow(clamp(cos(4.0 * (angle + 0.35)) * 0.5 + 0.5, 0.0, 1.0), 64.0) * exp(-distSun * 7.5);
                float3 diagColor = float3(0.85, 0.95, 1.20) * diagCross * 0.65;

                glare += (sunstarColor + streakColor + diagColor) * pc.sunScreenPos.w * sunOcclusion * (1.0 - pc.blizzard * 0.85);
            }
        }
    }

    // B. Local Micro-Glint Sunstars (specular sparkles on firn & sérac ice)
    float localLum = dot(hdrColor, float3(0.2126, 0.7152, 0.0722));
    float glintThreshold = adaptedLum * 4.5;
    if (localLum > glintThreshold && localLum > 4.0) {
        float glintExcess = (localLum - glintThreshold) / localLum;
        float2 px = float2(1.0 / pc.resolution.x, 1.0 / pc.resolution.y);

        float3 sampleL = hdrTexture.SampleLevel(uv - float2(px.x * 2.5, 0.0), 0.0).rgb;
        float3 sampleR = hdrTexture.SampleLevel(uv + float2(px.x * 2.5, 0.0), 0.0).rgb;
        float3 sampleU = hdrTexture.SampleLevel(uv - float2(0.0, px.y * 2.5), 0.0).rgb;
        float3 sampleD = hdrTexture.SampleLevel(uv + float2(0.0, px.y * 2.5), 0.0).rgb;

        float neighborAvg = (dot(sampleL, float3(0.333)) + dot(sampleR, float3(0.333)) +
                             dot(sampleU, float3(0.333)) + dot(sampleD, float3(0.333))) * 0.25;

        if (localLum > neighborAvg * 1.5) {
            float glintSharpness = clamp((localLum - neighborAvg * 1.5) / localLum, 0.0, 1.0);
            float3 microSparkle = hdrColor * glintSharpness * glintExcess * 0.45;
            glare += microSparkle;
        }
    }

    // =========================================================================
    // 3. Brewster-Winkel-Polarisation & Category-4 Gletscherbrille Filter
    // =========================================================================
    if (gogglesEquipped) {
        // Brewster Polarization: Linear vertical polarization absorbs horizontal snow reflections (78% attenuation)
        glare *= 0.22;

        // Cat-4 High-Altitude Mineral Glass: Amber-Brown Contrast Curve
        // Cuts short-wavelength blue atmospheric haze while enhancing sastrugi & crevasse contrast
        float3 amberTint = float3(1.08, 0.95, 0.74);
        hdrColor = hdrColor * amberTint;

        // Sastrugi snow relief curve (gamma shaping of snow midtones)
        hdrColor = pow(max(hdrColor, float3(0.0)), float3(1.06));
    }

    // =========================================================================
    // 4. Photometric Exposure Application
    // =========================================================================
    float3 exposedScene = hdrColor * exposure + glare;

    // =========================================================================
    // 5. Schneeblendung (Photokeratitis / Snow Blindness without Goggles)
    // =========================================================================
    if (!gogglesEquipped && snowBlindness > 0.005) {
        // Blinding overexposure from unmitigated ~90% snow albedo and high-altitude solar radiation
        float overExp = 1.0 + snowBlindness * 4.2;
        exposedScene *= overExp;

        // Washed out whiteout veil
        float whiteVeil = pow(snowBlindness, 1.3) * 2.2;
        exposedScene += float3(whiteVeil);

        // Corneal flare & retinal saturation: loss of contrast and painful washed-out white
        float bLum = dot(exposedScene, float3(0.333));
        exposedScene = lerp(exposedScene, float3(bLum * 1.45), snowBlindness * 0.76);
    }

    // =========================================================================
    // 6. Atem-Kondensation Layer (Translucent Warm Mist)
    // =========================================================================
    if (breathFogMask > 0.005) {
        float3 mistColor = float3(0.92, 0.94, 0.98) * (adaptedLum * 0.35 + 0.65);
        exposedScene = lerp(exposedScene, mistColor, breathFogMask * 0.78);
    }

    // =========================================================================
    // 7. Gefrierende Eisblumen (Frost Dendrites / Ice Crystals at Glass Margins)
    // =========================================================================
    if (frostDendriteMask > 0.005) {
        float2 pFrost = (uv - 0.5) * float2(aspect, 1.0);
        float frostAngle = atan2(pFrost.y, pFrost.x);

        // Micro-facet crystalline sparkle with subtle prismatic rainbow dispersion
        float3 frostSparkle = float3(
            1.0 + sin(frostAngle * 6.0 + 0.0) * 0.16,
            1.0 + sin(frostAngle * 6.0 + 2.094) * 0.16,
            1.0 + sin(frostAngle * 6.0 + 4.188) * 0.16
        );

        float3 iceColor = float3(0.94, 0.97, 1.03) * (adaptedLum * 0.55 + 0.65) * frostSparkle;
        exposedScene = lerp(exposedScene, iceColor, frostDendriteMask * 0.96);
    }

    // =========================================================================
    // 8. Alpine Goggle Frame Vignette (Leather Side-Shields / Lederner Seitenschutz)
    // =========================================================================
    if (gogglesEquipped) {
        float2 pFrame = (uv - 0.5) * float2(aspect, 1.0);
        
        // Super-ellipse distance function (ergonomic widescreen alpine frame)
        float dx = abs(pFrame.x) / (aspect * 0.45);
        float dy = abs(pFrame.y) / 0.48;
        float frameDist = pow(pow(dx, 3.4) + pow(dy, 3.4), 1.0 / 3.4);

        // Peripheral leather side-shield rim blocking lateral high-altitude glare
        float frameVignette = smoothstep(1.08, 0.88, frameDist);

        // Fine leather grain on outer perimeter
        float leatherNoise = (noise2D(pFrame * 35.0) - 0.5) * 0.12;
        frameVignette = clamp(frameVignette + leatherNoise * smoothstep(0.85, 1.06, frameDist), 0.0, 1.0);

        // Soft chromatic bevel right where mineral glass meets the frame
        float bevelZone = smoothstep(0.86, 0.98, frameDist) * (1.0 - smoothstep(0.98, 1.06, frameDist));
        if (bevelZone > 0.01) {
            float3 caShift = float3(1.07, 0.98, 0.91);
            exposedScene = lerp(exposedScene, exposedScene * caShift, bevelZone * 0.4);
        }

        exposedScene *= frameVignette;
    }

    // =========================================================================
    // 9. Hypoxie- & Höhenrausch-Shader in der Todeszone (>8,000m)
    // =========================================================================
    if (hypoxiaFactor > 0.005) {
        float2 pRadial = (uv - 0.5) * float2(aspect, 1.0);
        float r = length(pRadial);

        // A. Periphere Sichtfeldeinengung (Tunnelblick):
        // Oxygen starvation constricts peripheral retinal sensitivity
        float innerR = lerp(0.96, 0.52, hypoxiaFactor);
        float outerR = lerp(1.36, 0.88, hypoxiaFactor);

        // Cardiovascular heart rate pulse (systole/diastole wave) pumps the tunnel radius
        innerR -= heartbeatPulse * 0.05 * hypoxiaFactor;
        outerR -= heartbeatPulse * 0.07 * hypoxiaFactor;

        float tunnelMask = smoothstep(outerR, innerR, r);

        // B. Vaskuläre Randverdunklung & Blutdruck-Stoßwelle:
        // High capillary blood pressure surges into the peripheral retina during systole
        float vascularRing = smoothstep(innerR * 0.90, outerR, r) * (1.0 - smoothstep(outerR, outerR + 0.26, r));
        float3 vascularRed = float3(0.56, 0.02, 0.03); // Deep venous oxygen-depleted blood
        exposedScene = lerp(exposedScene, vascularRed, vascularRing * heartbeatPulse * hypoxiaFactor * 0.82);

        // C. Retinale Entsättigung (Cyanotic Desaturation):
        // Cones and cerebral visual cortex lose color fidelity under acute hypoxia
        float sceneLum = dot(exposedScene, float3(0.299, 0.587, 0.114));
        float3 cyanoticGray = float3(sceneLum * 0.88, sceneLum * 0.94, sceneLum * 1.06);
        exposedScene = lerp(exposedScene, cyanoticGray, hypoxiaFactor * 0.75);

        // D. Apply Tunnel Vision Darkening
        exposedScene *= tunnelMask;
    }

    // =========================================================================
    // 10. ACES Filmic Tone Mapping (HDR -> LDR Display)
    // =========================================================================
    float3 finalLdr = ACESFilm(exposedScene);

    return float4(finalLdr, 1.0);
}

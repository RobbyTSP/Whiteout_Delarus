// Whiteout Delarus - Step 16 (1:1 Part XIV): Stratospheric Atmosphere & Ice Crystal Halo Optics (Slang)
// Vulkan 1.3+ / 1.4 Dynamic Rendering
// - Barometric Pressure Model p(h) = 1013.25 * exp(-h / 7200) (Death Zone 337 hPa)
// - Cosmic Indigo-Black Zenith & Prominent Daytime Stars (>7,000m)
// - Chappuis Ozone Absorption (550-650nm) & Belt of Venus
// - Physical 22° Ice Crystal Halo with Chromatic Dispersion (Prism Angle 60°, delta_min ~ 21.84°)
// - Parhelia (Sun Dogs) at 22° Azimuth & Parhelic Cirrus Shimmer
// - Diamond-Dust Light Pillars at Sunset/Dawn

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

// ACES Filmic Tone Mapping (optical coherence with terrain PBR shader)
float3 ACESFilm(float3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// 3D Directional Hash for Procedural Celestial Sphere
float3 hash33(float3 p) {
    p = frac(p * float3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return frac((p.xxy + p.yxx) * p.zyx);
}

// Celestial Daytime & Nighttime Stars
float3 renderCelestialStars(float3 rayDir, float3 L, float camElev, float overcast, float time) {
    // 1. Daytime vs Nighttime Visibility
    // In the death zone (>7,000m), Rayleigh sky luminance drops to ~30%.
    // Only the brightest 10-20 stars and planets (Sirius, Canopus, Vega, Rigel, Venus, Jupiter)
    // are visible to the naked human eye in the dark cosmic zenith.
    float daySummitAlt = smoothstep(7000.0, 8600.0, camElev);
    float dayZenithCone = smoothstep(0.48, 0.94, rayDir.y);
    float cosThetaSun = dot(rayDir, L);
    float sunExclusion = 1.0 - smoothstep(0.20, 0.75, cosThetaSun);
    float dayVis = daySummitAlt * dayZenithCone * sunExclusion;

    // Night stars (when sun is below horizon)
    float nightVis = (1.0 - smoothstep(-0.10, 0.08, L.y)) * smoothstep(0.04, 0.85, rayDir.y);

    float totalVis = max(dayVis, nightVis) * (1.0 - overcast);
    if (totalVis <= 0.001) return float3(0.0);

    // 2. High-precision celestial grid
    float3 d = normalize(rayDir);
    float gridRes = 160.0;
    float3 cell = floor(d * gridRes);
    float3 fracCoord = frac(d * gridRes) - 0.5;

    float3 h = hash33(cell);

    // Sparsity threshold:
    // Daytime: only ~0.08% of cells (exclusively magnitude 0 to 1 stars)
    // Nighttime: ~1.2% of cells
    float threshold = lerp(0.988, 0.9991, dayVis);
    if (h.x < threshold) return float3(0.0);

    // Star position jitter within cell
    float2 starPos = (h.yz - 0.5) * 0.70;
    float dist = length(fracCoord.xy - starPos);

    // Sharp Gaussian pinpoint (diffraction-limited optical point)
    float core = exp(-dist * dist * 120.0);
    if (core < 0.001) return float3(0.0);

    // Atmospheric twinkling (Szintillation)
    float twinkle = sin(time * 5.5 + h.y * 73.1) * 0.25 + 0.75;

    // 4-Point Diffraction Star Spike for the brightest 0-magnitude stars & planets
    float spike = 0.0;
    if (h.x > 0.997) {
        float sx = exp(-abs(fracCoord.x - starPos.x) * 60.0) * exp(-abs(fracCoord.y - starPos.y) * 6.0);
        float sy = exp(-abs(fracCoord.y - starPos.y) * 60.0) * exp(-abs(fracCoord.x - starPos.x) * 6.0);
        spike = max(sx, sy) * 0.65;
    }

    // Stellar spectral temperature (Blue-white Sirius/Vega to orange-red Betelgeuse)
    float3 starColor = lerp(float3(0.82, 0.92, 1.25), float3(1.30, 0.85, 0.55), h.z);
    float starIntensity = (core * 2.8 + spike) * twinkle * (0.8 + 1.2 * h.y);

    return starColor * starIntensity * totalVis;
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

    // =========================================================================
    // 2. Barometrische Höhenformel & Dichteverteilung der Todeszone (337 hPa)
    // =========================================================================
    // Barometrische Formel: p(h) = p0 * exp(-h / H_scale) mit H_scale = 7200m
    // Auf Meereshöhe: 1013 hPa (rho = 1.0)
    // Am Basislager (5.300m): ~485 hPa (rho ~ 0.479)
    // Am Südsattel / Camp 4 (8.000m): ~334 hPa (rho ~ 0.330)
    // Am Everest-Gipfel (8.848m): ~296 hPa (rho ~ 0.292)
    float camElev = clamp(pc.cameraPos.y, 0.0, 10000.0);
    float rhoRel = exp(-camElev / 7200.0);
    float baroPressureHpa = 1013.25 * rhoRel;

    // Relative Höhe über der Khumbu-Talbasis (4.500m bis 8.848m)
    float altRatio = clamp((pc.cameraPos.y - 4500.0) / 4348.0, 0.0, 1.0);

    // Zenith Rayleigh Luminanz nimmt proportional zur Moleküldichte ab
    // Im Tal: leuchtendes Alpen-Azurblau (0.095, 0.21, 0.48)
    // Auf 8.848m: Die Luft ist zu dünn für Streuung -> kosmisch tiefes Indigo-Schwarzblau (0.0035, 0.0065, 0.024)
    float3 valleyZenith = float3(0.095, 0.210, 0.480);
    float3 stratoZenith = float3(0.0035, 0.0065, 0.024);
    float zenithDarken = pow(altRatio, 1.25);
    float3 zenithColor = lerp(valleyZenith, stratoZenith, zenithDarken * 0.94);

    // =========================================================================
    // 3. Chappuis-Ozonabsorption (550–650 nm) & Dämmerungs-Spektrum
    // =========================================================================
    // Ozon absorbiert selektiv gelb-oranges Licht (Peak ~600nm)
    // Tangentialer Sonnenstrahlweg durch die Ozonschicht (15-30 km) bei tiefstehender Sonne
    float sunElev = clamp(L.y, 0.0, 1.0);
    float duskDawnFactor = 1.0 - smoothstep(0.04, 0.38, sunElev);

    float ozonePath = (1.0 / sqrt(max(L.y, 0.015) + 0.020)) * (1.0 + 1.8 * (1.0 - max(rayDir.y, 0.0)));
    // Absorptionskoeffizienten sigma_ozone: Rot ~0.065, Grün ~0.082, Blau ~0.005
    float3 ozoneTransmittance = exp(-float3(0.065, 0.082, 0.005) * ozonePath * 8.5);

    // Bei Sonnenauf-/untergang filtert Ozon das gelbliche Streulicht heraus -> tieferes Königsblau & Purpur am Zenit
    zenithColor = lerp(zenithColor, zenithColor * ozoneTransmittance * 1.55, duskDawnFactor * 0.70);

    // =========================================================================
    // 4. Horizont Rayleigh & Mie Scattering (Distanzdunst-Horizont)
    // =========================================================================
    // Slant-Path-Airmass durch die Atmosphäre
    float horizonFactor = clamp(1.0 - max(rayDir.y, 0.0), 0.0, 1.0);
    float horizonGlowPower = pow(horizonFactor, 3.4);

    // Tages-Horizont: Klares Azur/Cyan
    float3 dayHorizonColor = float3(0.50, 0.68, 0.88);

    // Sonnenauf- / Untergangs-Horizont
    float2 rayDirHoriz = (length(rayDir.xz) > 0.001) ? normalize(rayDir.xz) : float2(0.0, 1.0);
    float2 sunDirHoriz = (length(L.xz) > 0.001) ? normalize(L.xz) : float2(0.0, 1.0);
    float horizSunAlign = clamp(dot(rayDirHoriz, sunDirHoriz), -1.0, 1.0);
    float sunsetForwardGlow = pow(clamp(horizSunAlign * 0.5 + 0.5, 0.0, 1.0), 2.2);
    float3 sunsetHorizonColor = pc.sunColor.rgb * float3(1.22, 0.60, 0.32) * (0.60 + 0.90 * sunsetForwardGlow);

    // Gegendämmerung (Belt of Venus) mit Ozon-Pastellrosa
    float antiSunAlign = clamp(-horizSunAlign, 0.0, 1.0);
    float beltOfVenus = pow(antiSunAlign, 2.0) * duskDawnFactor * 
                       smoothstep(-0.05, 0.18, rayDir.y) * (1.0 - smoothstep(0.18, 0.46, rayDir.y));
    float3 beltOfVenusColor = float3(0.92, 0.44, 0.62) * ozoneTransmittance * beltOfVenus * 0.88;

    // Erdschattenbogen (unterhalb des Belt of Venus)
    float earthShadowBand = duskDawnFactor * smoothstep(-0.15, 0.02, -rayDir.y) * antiSunAlign;
    float3 earthShadowColor = float3(0.022, 0.038, 0.098);

    // Horizontale Himmelsmischung
    float3 horizonColor = lerp(dayHorizonColor, sunsetHorizonColor, duskDawnFactor);
    float3 skyGrad = lerp(zenithColor, horizonColor, horizonGlowPower);
    skyGrad += beltOfVenusColor;
    skyGrad = lerp(skyGrad, earthShadowColor, earthShadowBand * 0.65);

    // Unter-Horizont Talnebel
    if (rayDir.y < 0.0) {
        float valleyDip = clamp(-rayDir.y * 3.5, 0.0, 1.0);
        float3 valleyMist = lerp(horizonColor * 0.85, float3(0.16, 0.20, 0.30), valleyDip);
        skyGrad = lerp(skyGrad, valleyMist, valleyDip);
    }

    // =========================================================================
    // 5. Direkte Sonne & Mie-Aureole
    // =========================================================================
    if (L.y > -0.05) {
        // Direkte Sonnenscheibe (0.53° astronomischer Durchmesser)
        float sunDisc = smoothstep(0.9997, 0.99995, cosThetaSun) * 42.0;
        // Zirkumsolare Korona (Eiskristall-Vorwärtsbeugung)
        float corona = pow(clamp(cosThetaSun, 0.0, 1.0), 384.0) * 6.5;
        // Atmosphärische Aureole
        float aureole = pow(clamp(cosThetaSun, 0.0, 1.0), 32.0) * 1.45;

        float3 solarLighting = (sunDisc + corona + aureole) * pc.sunColor.rgb;
        skyGrad += solarLighting;
    }

    // =========================================================================
    // 6. Eiskristall-Atmosphärenoptik: 22°-Halo, Parhelia & Lichtsäulen
    // =========================================================================
    float blizzard = pc.sunColor.w;
    float cloudDensity = pc.cameraPos.w;
    float overcast = max(blizzard, smoothstep(0.60, 0.95, cloudDensity) * 0.75);

    // Diamantstaub-Dichte: Verstärkt in der Kälte der Hochlagen
    float diamondDustFactor = lerp(0.60, 0.95, altRatio) * (1.0 - overcast * 0.85);

    if (L.y > -0.08 && diamondDustFactor > 0.05) {
        // --- A. Physikalischer 22°-Halo mit Spektraler Dispersion ---
        // Brechungswinkel in hexagonalen Prismenkristallen (60°-Prismenwinkel)
        // delta_min = 2 * asin(n * sin(30°)) - 60°
        // n_rot = 1.307 -> 21.65° | n_gruen = 1.311 -> 22.05° | n_blau = 1.317 -> 22.55°
        float thetaRad = acos(clamp(cosThetaSun, -1.0, 1.0));
        float thetaDeg = degrees(thetaRad);

        // Chromatische Dispersionsbänder des 22°-Halos
        float haloR = exp(-pow((thetaDeg - 21.65) / 0.45, 2.0));
        float haloG = exp(-pow((thetaDeg - 22.05) / 0.58, 2.0));
        float haloB = exp(-pow((thetaDeg - 22.55) / 0.80, 2.0));

        // Scharfer innerer Cutoff (keine Brechungsstrahlen unter 21.4°)
        float haloInnerCut = smoothstep(21.35, 21.75, thetaDeg);
        // Sanfter diffuser Auslauf nach außen
        float haloOuterDecay = exp(-max(thetaDeg - 22.5, 0.0) * 0.52);
        float3 haloColor = float3(haloR, haloG, haloB) * haloInnerCut * haloOuterDecay;

        // "Loch im Himmel": Verdunklung zwischen Vorwärtsstreuung und Halo-Innenrand (8° bis 21.4°)
        float innerDarkening = 1.0 - 0.20 * smoothstep(7.0, 16.0, thetaDeg) * (1.0 - smoothstep(21.2, 22.0, thetaDeg));
        skyGrad *= innerDarkening;

        // Zarte Cirrus-Schleier-Modulation (organisches Brechungsmuster)
        float cirrusMod = 0.85 + 0.15 * sin(rayDir.x * 5.0 + rayDir.z * 3.5 + pc.time * 0.04);
        skyGrad += haloColor * 0.95 * cirrusMod * diamondDustFactor * pc.sunColor.rgb;

        // --- B. Nebensonnen (Parhelia / Sun Dogs) ---
        // Entstehen durch horizontal orientierte, flache Eisplättchen
        // Stehen auf gleicher Höhe wie die Sonne (Delta Y ~ 0) und im Azimutabstand von ~22°
        // Parhelia sind vor allem bei tiefem bis mittlerem Sonnenstand aktiv (L.y < 0.45)
        float sunDogActive = (1.0 - smoothstep(0.12, 0.48, L.y)) * smoothstep(-0.06, 0.02, L.y + 0.06);
        if (sunDogActive > 0.01 && length(rayDir.xz) > 0.05 && length(L.xz) > 0.05) {
            float deltaY = abs(rayDir.y - L.y);
            float parheliaElevProfile = exp(-pow(deltaY / 0.024, 2.0));

            // Azimut-Winkelabstand im Horizontalschnitt
            float cosAzimuth = clamp(dot(rayDirHoriz, sunDirHoriz), -1.0, 1.0);
            float azimDeg = degrees(acos(cosAzimuth));

            // Parhelien-Winkel weitet sich bei höherer Sonne leicht auf: thetaDog = 22.0° + 2.8° * (L.y / 0.4)^2
            float normSunY = clamp(L.y / 0.45, 0.0, 1.0);
            float thetaDog = 22.0 + 2.8 * pow(normSunY, 1.8);

            // Chromatische Dispersion der Nebensonnen: Roter Innenrand, weißes Zentrum, weiß-bläulicher Schweif
            float dogCoreR = exp(-pow((azimDeg - (thetaDog - 0.35)) / 0.52, 2.0));
            float dogCoreG = exp(-pow((azimDeg - thetaDog) / 0.62, 2.0));
            float dogCoreB = exp(-pow((azimDeg - (thetaDog + 0.45)) / 0.78, 2.0));
            float dogInnerCut = smoothstep(thetaDog - 0.70, thetaDog - 0.10, azimDeg);

            // Weißer Horizontalschweif entlang des Nebensonnenkreises (Parhelic Circle)
            float dogTail = smoothstep(thetaDog, thetaDog + 1.2, azimDeg) * exp(-(azimDeg - thetaDog) * 0.22);
            float3 dogSpectrum = (float3(dogCoreR, dogCoreG, dogCoreB) * dogInnerCut + float3(0.85, 0.92, 1.05) * dogTail);

            float3 sundogs = dogSpectrum * parheliaElevProfile * 2.2 * sunDogActive * diamondDustFactor * pc.sunColor.rgb;
            skyGrad += sundogs;
        }

        // --- C. Vertikale Lichtsäulen (Light Pillars) bei Dämmerung ---
        // Spiegelnde Reflexion an Unter- und Oberseiten flacher Eisplättchen bei tiefstehender Sonne
        if (L.y < 0.26 && length(L.xz) > 0.05) {
            float horizOffset = abs(rayDir.x * (-sunDirHoriz.y) + rayDir.z * sunDirHoriz.x);
            float horizForward = dot(rayDir.xz, sunDirHoriz);
            float pillarHoriz = exp(-pow(horizOffset / 0.024, 2.0)) * smoothstep(0.40, 0.90, horizForward);

            // Vertikale Lichtsäule ragt von der Sonne bis zu 22° senkrecht in den Himmel
            float pillarVert = smoothstep(-0.03, 0.04, rayDir.y - L.y) * exp(-max(rayDir.y - L.y, 0.0) / 0.20);
            float pillarActive = (1.0 - smoothstep(0.04, 0.25, L.y)) * smoothstep(-0.08, 0.01, L.y + 0.08);

            float3 lightPillar = pc.sunColor.rgb * float3(1.35, 0.85, 0.52) * pillarHoriz * pillarVert * pillarActive * 2.8 * diamondDustFactor;
            skyGrad += lightPillar;
        }

        // --- D. Diskrete zirkumsolare Diamantstaub-Glints ---
        if (cosThetaSun > 0.82 && diamondDustFactor > 0.25) {
            float3 glintCell = floor(rayDir * 180.0);
            float3 hGlint = hash33(glintCell);
            if (hGlint.x > 0.996) {
                float glintFlash = pow(sin(pc.time * 8.0 + hGlint.y * 50.0) * 0.5 + 0.5, 8.0);
                float2 glintPos = (hGlint.yz - 0.5) * 0.70;
                float dGlint = length((frac(rayDir * 180.0) - 0.5).xy - glintPos);
                float glintCore = exp(-dGlint * dGlint * 90.0);
                skyGrad += pc.sunColor.rgb * glintCore * glintFlash * 1.5 * diamondDustFactor;
            }
        }
    }

    // =========================================================================
    // 7. Stratosphärische Tages-Sterne (>7.000 m) & Kristalline Sternennacht
    // =========================================================================
    float3 celestialStars = renderCelestialStars(rayDir, L, pc.cameraPos.y, overcast, pc.time);
    skyGrad += celestialStars;

    // =========================================================================
    // 8. Blizzard & Schneesturm-Bewölkung
    // =========================================================================
    if (overcast > 0.01) {
        float3 stormSky = float3(0.82, 0.86, 0.90);
        skyGrad = lerp(skyGrad, stormSky, overcast * 0.92);
    }

    // =========================================================================
    // 9. ACES Filmic Tone Mapping
    // =========================================================================
    float exposure = 0.82;
    float3 finalSky = ACESFilm(skyGrad * exposure);

    return float4(finalSky, 1.0);
}

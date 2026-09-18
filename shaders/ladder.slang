// Whiteout Delarus - Step 25 (1:1 Part XXIII): 4-Section Sectional Aluminum Crevasse Ladder & Snowbridge Shader (Slang)
// Vulkan 1.3+ / 1.4 Dynamic Rendering
// Features:
// - Physical 6061-T6 Tubular Extruded Aluminum PBR (Metallic = 0.98, Roughness = 0.24)
// - Top-Surface Rung Snow Dusting & Firn Accumulation
// - Direct Sunlight Cook-Torrance GGX Specular Highlight
// - Stratospheric Atmospheric Aerial Perspective Fog & Shadowing

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

// Textures bound from terrain descriptor set
[[vk::binding(6, 0)]]  Sampler2D texSnowAlbedo;
[[vk::binding(8, 0)]]  Sampler2D texSnowRoughness;
[[vk::binding(20, 0)]] Sampler2D texMultiBounceGI;

struct VertexInput {
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 normal   : NORMAL;
    [[vk::location(2)]] float2 uv       : TEXCOORD0;
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

    // Normal transform
    float3x3 normalMatrix = (float3x3)pc.model;
    output.worldNormal = normalize(mul(normalMatrix, input.normal));
    output.uv = input.uv;
    output.sv_position = mul(pc.proj, mul(pc.view, worldPos));

    return output;
}

// GGX Microfacet Distribution
float D_GGX(float NdotH, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / max(3.14159265 * denom * denom, 0.0001);
}

// Schlick-GGX Geometric Attenuation
float G_SchlickSmith(float NdotV, float NdotL, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    float g1V = NdotV / (NdotV * (1.0 - k) + k);
    float g1L = NdotL / (NdotL * (1.0 - k) + k);
    return g1V * g1L;
}

// Fresnel Schlick
float3 F_Schlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
    float3 N = normalize(input.worldNormal);
    float3 V = normalize(pc.cameraPos.xyz - input.worldPos);
    float3 L = normalize(pc.sunDir.xyz);
    float3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.001);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    // =========================================================================
    // 1. Physical 6061-T6 Aluminum Material Characteristics
    // =========================================================================
    float3 aluminumAlbedo = float3(0.91, 0.925, 0.94); // High reflectance conductor
    float aluminumMetallic = 0.98;
    float aluminumRoughness = 0.22;
    float3 F0_metal = float3(0.91, 0.92, 0.94); // Conductor F0

    // Top-surface snow dusting (clinging firn on top of rungs and rails)
    float upFacing = clamp(N.y, 0.0, 1.0);
    float snowDusting = smoothstep(0.35, 0.85, upFacing);

    float3 snowAlbedo = float3(0.96, 0.97, 0.99);
    float3 baseColor = lerp(aluminumAlbedo, snowAlbedo, snowDusting * 0.85);
    float metallic = lerp(aluminumMetallic, 0.0, snowDusting);
    float roughness = lerp(aluminumRoughness, 0.85, snowDusting);
    float3 F0 = lerp(float3(0.04), F0_metal, metallic);

    // Cook-Torrance Direct Specular
    float D = D_GGX(NdotH, roughness);
    float G = G_SchlickSmith(NdotV, NdotL, roughness);
    float3 F = F_Schlick(VdotH, F0);
    float3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 0.001);

    // Diffuse component (zero for pure conductor, active on snow dusting)
    float3 kS = F;
    float3 kD = (float3(1.0) - kS) * (1.0 - metallic);
    float3 diffuse = kD * baseColor / 3.14159265;

    // Sun direct lighting
    float3 directLight = (diffuse + specular) * pc.sunColor.rgb * NdotL;

    // Ambient lighting & sky bounce
    float3 skyLight = float3(0.08, 0.14, 0.24) * baseColor * (0.5 + 0.5 * N.y);
    float3 groundBounce = float3(0.22, 0.24, 0.26) * baseColor * (0.5 - 0.5 * N.y);
    float3 ambient = (skyLight + groundBounce) * 0.65;

    float3 finalColor = directLight + ambient;

    // =========================================================================
    // 2. High-Altitude Atmospheric Distance Fog
    // =========================================================================
    float distToCam = length(pc.cameraPos.xyz - input.worldPos);
    float fogFactor = 1.0 - exp(-distToCam * 0.00015);
    float3 fogColor = float3(0.72, 0.82, 0.95);
    finalColor = lerp(finalColor, fogColor, clamp(fogFactor, 0.0, 0.85));

    return float4(finalColor, 1.0);
}

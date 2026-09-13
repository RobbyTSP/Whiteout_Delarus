// Whiteout Delarus - Himalayan Terrain Shader (Slang)
// Compiles to SPIR-V for Vulkan 1.3+ / 1.4

struct PushConstants {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float4 cameraPos;
    float4 sunDir;      // Direction towards the sun (normalized)
    float4 sunColor;    // Sun intensity & color
    float time;
    float minElev;      // Min terrain elevation in meters (e.g. 3651.0)
    float maxElev;      // Max terrain elevation in meters (e.g. 8753.0)
    float padding;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> pc;

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
    
    // Normal transform (assuming uniform scale on model)
    output.worldNormal = normalize(mul((float3x3)pc.model, input.normal));
    output.uv = input.uv;
    
    output.sv_position = mul(pc.proj, mul(pc.view, worldPos));
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
    float3 N = normalize(input.worldNormal);
    float3 L = normalize(pc.sunDir.xyz);
    float3 V = normalize(pc.cameraPos.xyz - input.worldPos);
    
    // Himalayan Elevation- & Slope-based biome coloring
    // Slopes > 45 degrees shed snow, exposing steep granite rock
    float slope = 1.0 - clamp(dot(N, float3(0.0, 1.0, 0.0)), 0.0, 1.0);
    float elevation = input.worldPos.y;
    
    // Normalized elevation factor [0..1]
    float elevNorm = clamp((elevation - pc.minElev) / max(pc.maxElev - pc.minElev, 1.0), 0.0, 1.0);
    
    // Base colors
    float3 rockColor   = float3(0.32, 0.30, 0.28); // Himalayan dark granite
    float3 snowColor   = float3(0.95, 0.96, 0.98); // High-altitude firn snow
    float3 screeColor  = float3(0.45, 0.40, 0.35); // Moraine gravel
    float3 valleyColor = float3(0.25, 0.28, 0.22); // Lower alpine scrub
    
    // Height & Slope blending
    float snowFactor = smoothstep(0.35, 0.65, elevNorm) * (1.0 - smoothstep(0.3, 0.6, slope));
    float rockFactor = smoothstep(0.2, 0.5, slope);
    
    float3 baseColor = lerp(screeColor, valleyColor, smoothstep(0.0, 0.3, 1.0 - elevNorm));
    baseColor = lerp(baseColor, rockColor, rockFactor);
    baseColor = lerp(baseColor, snowColor, snowFactor);
    
    // Lighting: Sun + Sky Ambient
    float NdotL = max(dot(N, L), 0.0);
    float3 diffuse = NdotL * pc.sunColor.rgb;
    
    // High-altitude mountain sky ambient light (deep blue sky hemisphere)
    float skyAmbientFactor = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
    float3 skyAmbient = lerp(float3(0.12, 0.15, 0.18), float3(0.25, 0.35, 0.50), skyAmbientFactor);
    
    // Specular highlight for snow & ice
    float3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    float specular = pow(NdotH, 32.0) * snowFactor * 0.4;
    
    float3 litColor = baseColor * (diffuse + skyAmbient) + float3(specular, specular, specular);
    
    // Distance Fog / Atmospheric Rayleigh Scattering (Himalayan haze)
    float dist = length(pc.cameraPos.xyz - input.worldPos);
    float fogFactor = 1.0 - exp(-dist * 0.00004); // subtle thin mountain air fog
    float3 fogColor = float3(0.55, 0.68, 0.82);
    
    float3 finalColor = lerp(litColor, fogColor, clamp(fogFactor, 0.0, 0.85));
    
    return float4(finalColor, 1.0);
}

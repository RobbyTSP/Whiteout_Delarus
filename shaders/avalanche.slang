// Whiteout Delarus - Step 20 (1:1 Part XVIII): Real-Time GPU Powder Avalanche Volumetric Renderer
// Renders 8,192 billowing powder snow particles with Mie forward scattering, camera billboarding,
// and soft spherical density profiles into the 16-Bit Half-Float HDR buffer.

struct AvalancheRenderPushConstants {
    float4x4 view;
    float4x4 proj;
    float4 cameraPos;   // xyz = camera pos, w = time
    float4 sunDir;      // xyz = sun dir, w = blizzardFactor
    float4 sunColor;    // rgb = sun color
};

[[vk::push_constant]]
ConstantBuffer<AvalancheRenderPushConstants> pc;

struct AvalancheParticle {
    float4 posRadius;  // xyz = world position, w = radius / scale (m)
    float4 velLife;    // xyz = velocity (m/s), w = remaining life (0..1)
    float4 params;     // x = density/opacity, y = vorticity/spin, z = turbulence, w = active flag
};

[[vk::binding(0, 0)]]
StructuredBuffer<AvalancheParticle> bufParticles;

struct VertexOutput {
    float4 position : SV_Position;
    float3 worldPos : WORLDPOS;
    float2 uv       : TEXCOORD0;
    float alpha     : ALPHA;
    float life      : LIFE;
};

static const float2 QUAD_VERTS[6] = {
    float2(-1.0, -1.0),
    float2( 1.0, -1.0),
    float2(-1.0,  1.0),
    float2(-1.0,  1.0),
    float2( 1.0, -1.0),
    float2( 1.0,  1.0)
};

[shader("vertex")]
VertexOutput vertexMain(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID) {
    VertexOutput output;

    AvalancheParticle p = bufParticles[instanceID];

    // Degenerate/inactive particle cull
    if (p.params.w < 0.5 || p.velLife.w <= 0.0) {
        output.position = float4(0.0, 0.0, -2.0, 1.0);
        output.worldPos = float3(0.0);
        output.uv = float2(0.0);
        output.alpha = 0.0;
        output.life = 0.0;
        return output;
    }

    float2 quadPos = QUAD_VERTS[vertexID % 6];
    float radius = p.posRadius.w;

    // Spherical Camera Billboard Basis
    float3 toCam = pc.cameraPos.xyz - p.posRadius.xyz;
    float distCam = length(toCam);
    if (distCam < 0.001) distCam = 1.0;
    toCam /= distCam;

    float3 up = float3(0.0, 1.0, 0.0);
    float3 camRight = cross(up, toCam);
    if (dot(camRight, camRight) < 0.001) {
        camRight = float3(1.0, 0.0, 0.0);
    } else {
        camRight = normalize(camRight);
    }
    float3 camUp = cross(toCam, camRight);

    float3 worldPos = p.posRadius.xyz + (camRight * quadPos.x + camUp * quadPos.y) * radius;

    output.position = mul(pc.proj, mul(pc.view, float4(worldPos, 1.0)));
    output.worldPos = worldPos;
    output.uv = quadPos;

    // Fade in as billow tumbles, fade out as life expires
    float life = p.velLife.w;
    float fadeIn = clamp((1.0 - life) / 0.08, 0.0, 1.0);
    float fadeOut = clamp(life / 0.15, 0.0, 1.0);
    output.alpha = p.params.x * fadeIn * fadeOut;
    output.life = life;

    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
    float rSq = dot(input.uv, input.uv);
    if (rSq > 1.0) discard;

    // Smooth volumetric spherical density
    float sphereZ = sqrt(1.0 - rSq);
    float density = pow(1.0 - rSq, 2.0) * input.alpha;

    // Mie Forward Scattering of Sunlight in Powder Cloud
    float3 V = normalize(pc.cameraPos.xyz - input.worldPos);
    float3 L = normalize(pc.sunDir.xyz);
    float cosTheta = dot(-V, L);
    float phaseMie = 0.5 * (1.0 + cosTheta * cosTheta) / pow(1.0 + 0.60 * 0.60 - 2.0 * 0.60 * cosTheta, 1.5);

    // Sunlit gleaming white-gold forward scattering
    float3 sunlitColor = pc.sunColor.rgb * (0.92 + 0.55 * phaseMie);

    // Deep alpine sky ambient in shadowed billow recesses
    float3 ambientColor = float3(0.42, 0.54, 0.72) * 0.65;

    float directWrap = sphereZ * 0.7 + 0.3;
    float3 powderColor = lerp(ambientColor, sunlitColor, directWrap);

    // Alpha blending with soft opacity limit
    float finalAlpha = clamp(density * 0.65, 0.0, 0.88);

    return float4(powderColor, finalAlpha);
}

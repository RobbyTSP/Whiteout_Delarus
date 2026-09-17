// Whiteout Delarus - Step 21 (1:1 Part XIX): 3D Gaussian Splatting Photogrammetry Hotspots
// Implements 3D-to-2D Elliptical Weighted Average (EWA) perspective projection,
// eigenvalue quad decomposition, Mahalanobis distance evaluation, and photometric HDR shading.

struct GaussianSplatPushConstants {
    float4x4 view;
    float4x4 proj;
    float4 cameraPos;     // xyz = camera world pos, w = totalTime
    float4 sunDir;        // xyz = normalized sun dir, w = blizzardFactor
    float4 sunColor;      // rgb = sun radiance, w = windSpeed
    float4 screenParams;  // x = screenWidth, y = screenHeight, z = focalX, w = focalY
};

[[vk::push_constant]]
ConstantBuffer<GaussianSplatPushConstants> pc;

struct GaussianSplatGPU {
    float4 posRadius; // xyz = world position, w = bounding radius (m)
    float4 rotQuat;   // xyzw = orientation quaternion
    float4 scaleOpac; // xyz = 3D semi-axes (sx, sy, sz), w = opacity (0..1)
    float4 colorSH;   // rgb = linear HDR base color, w = hotspot ID (0, 1, 2)
};

[[vk::binding(0, 0)]]
StructuredBuffer<GaussianSplatGPU> bufSplats;

[[vk::binding(1, 0)]]
StructuredBuffer<uint> bufSortedIndices;

struct VertexOutput {
    float4 position    : SV_Position;
    float3 worldPos    : WORLDPOS;
    float2 deltaPixels : TEXCOORD0;
    float3 invCov      : TEXCOORD1; // x = invCov00, y = invCov01, z = invCov11
    float3 color       : COLOR0;
    float  opacity     : OPACITY;
    float  hotspotId   : BLENDINDICES;
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

    uint splatIdx = bufSortedIndices[instanceID];
    GaussianSplatGPU g = bufSplats[splatIdx];

    float distToCam = length(g.posRadius.xyz - pc.cameraPos.xyz);
    // Distance culling: 3D Gaussian hotspots are photogrammetry near-field assets (120m radius)
    if (distToCam > 120.0 || g.scaleOpac.w <= 0.005) {
        output.position = float4(0.0, 0.0, -2.0, 1.0);
        output.worldPos = float3(0.0);
        output.deltaPixels = float2(0.0);
        output.invCov = float3(0.0);
        output.color = float3(0.0);
        output.opacity = 0.0;
        return output;
    }

    // 1. Transform position to View Space (GLM right-handed: camera looks down -Z)
    float4 viewPos4 = mul(pc.view, float4(g.posRadius.xyz, 1.0));
    float3 t = viewPos4.xyz;
    float zCam = -t.z;
    if (zCam <= 0.15) {
        output.position = float4(0.0, 0.0, -2.0, 1.0);
        output.worldPos = float3(0.0);
        output.deltaPixels = float2(0.0);
        output.invCov = float3(0.0);
        output.color = float3(0.0);
        output.opacity = 0.0;
        return output;
    }

    // 2. Compute 3D Covariance Matrix Sigma in World Space
    float qx = g.rotQuat.x;
    float qy = g.rotQuat.y;
    float qz = g.rotQuat.z;
    float qw = g.rotQuat.w;

    float3x3 R = float3x3(
        1.0 - 2.0 * (qy * qy + qz * qz), 2.0 * (qx * qy - qw * qz),       2.0 * (qx * qz + qw * qy),
        2.0 * (qx * qy + qw * qz),       1.0 - 2.0 * (qx * qx + qz * qz), 2.0 * (qy * qz - qw * qx),
        2.0 * (qx * qz - qw * qy),       2.0 * (qy * qz + qw * qx),       1.0 - 2.0 * (qx * qx + qy * qy)
    );

    float3 S = max(g.scaleOpac.xyz, float3(0.002));
    float3x3 M = float3x3(
        R[0] * S.x,
        R[1] * S.y,
        R[2] * S.z
    );

    float3x3 Sigma = mul(transpose(M), M);

    // 3. EWA Projection: Transform Sigma to Camera View Space
    float3x3 W = float3x3(
        pc.view[0].xyz,
        pc.view[1].xyz,
        pc.view[2].xyz
    );

    // Jacobian of perspective projection at t (with zCam = -t.z > 0)
    float fx = pc.screenParams.z;
    float fy = pc.screenParams.w;
    float zCam2 = zCam * zCam;

    float3 J0 = float3(fx / zCam, 0.0, -(fx * t.x) / zCam2);
    float3 J1 = float3(0.0, fy / zCam, -(fy * t.y) / zCam2);

    float3 JW0 = mul(J0, W);
    float3 JW1 = mul(J1, W);

    // 2D Screen-Space Covariance Matrix (pixels^2) with low-pass filter
    float cov00 = dot(JW0, mul(Sigma, JW0)) + 0.3;
    float cov01 = dot(JW0, mul(Sigma, JW1));
    float cov11 = dot(JW1, mul(Sigma, JW1)) + 0.3;

    float det = cov00 * cov11 - cov01 * cov01;
    if (det <= 0.0001) {
        output.position = float4(0.0, 0.0, -2.0, 1.0);
        output.worldPos = float3(0.0);
        output.deltaPixels = float2(0.0);
        output.invCov = float3(0.0);
        output.color = float3(0.0);
        output.opacity = 0.0;
        return output;
    }

    // 4. Eigenvalue Decomposition for Screen-Space Bounding Ellipse
    float trace = cov00 + cov11;
    float mid = 0.5 * trace;
    float disc = sqrt(max(0.1, mid * mid - det));
    float lambda1 = mid + disc;
    float lambda2 = max(0.1, mid - disc);

    // 3-sigma radius in screen pixels
    float r1 = min(3.0 * sqrt(lambda1), 512.0);
    float r2 = min(3.0 * sqrt(lambda2), 512.0);

    float2 e1 = float2(cov01, lambda1 - cov00);
    float lenE1 = length(e1);
    if (lenE1 < 0.001) {
        e1 = float2(1.0, 0.0);
    } else {
        e1 /= lenE1;
    }
    float2 e2 = float2(-e1.y, e1.x);

    float2 quadPos = QUAD_VERTS[vertexID % 6];
    float2 pixelOffset = (e1 * quadPos.x * r1 + e2 * quadPos.y * r2);

    // 5. Projected Clip-Space Extrusion (Vulkan NDC: [-1, 1], Y points down)
    float4 clipCenter = mul(pc.proj, viewPos4);
    if (clipCenter.w <= 0.001) {
        output.position = float4(0.0, 0.0, -2.0, 1.0);
        output.worldPos = float3(0.0);
        output.deltaPixels = float2(0.0);
        output.invCov = float3(0.0);
        output.color = float3(0.0);
        output.opacity = 0.0;
        return output;
    }

    float2 ndcOffset = (pixelOffset / pc.screenParams.xy) * 2.0;
    output.position = float4(clipCenter.xy + ndcOffset * clipCenter.w, clipCenter.z, clipCenter.w);
    output.worldPos = g.posRadius.xyz;
    output.deltaPixels = pixelOffset;

    // Inverse 2D covariance for Mahalanobis fragment evaluation
    float invDet = 1.0 / det;
    output.invCov = float3(cov11 * invDet, -cov01 * invDet, cov00 * invDet);

    // Dynamic wind fluttering for prayer flags (Hotspot 0)
    float3 color = g.colorSH.rgb;
    if (g.colorSH.w < 0.5) { // Summit prayer flags
        float flutterGlint = sin(pc.cameraPos.w * 8.0 + g.posRadius.x * 4.0) * 0.08;
        color = clamp(color + flutterGlint, 0.0, 1.5);
    }

    output.color = color;
    output.opacity = g.scaleOpac.w;
    output.hotspotId = g.colorSH.w;

    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
    // 1. Mahalanobis Distance in Screen Pixels
    float dx = input.deltaPixels.x;
    float dy = input.deltaPixels.y;
    float power = -0.5 * (dx * dx * input.invCov.x + 2.0 * dx * dy * input.invCov.y + dy * dy * input.invCov.z);

    // 3-sigma ellipse bound test
    if (power < -4.5) discard;

    float alpha = exp(power);
    alpha = smoothstep(0.04, 0.65, alpha) * input.opacity;
    if (alpha < 0.02) discard;

    // 2. Photometric HDR Illumination
    float3 V = normalize(pc.cameraPos.xyz - input.worldPos);
    float3 L = normalize(pc.sunDir.xyz);

    // Sunlit directional scattering
    float3 sunColor = pc.sunColor.rgb;
    float NdotL = clamp(L.y * 0.7 + 0.3, 0.0, 1.0);

    // Translucent Mie forward glow for thin prayer flag fabrics (Hotspot 0 only)
    float forwardGlow = 0.0;
    if (input.hotspotId < 0.5) {
        forwardGlow = pow(clamp(dot(-V, L), 0.0, 1.0), 3.5) * 0.45;
    }
    float3 directLight = (NdotL + forwardGlow) * (sunColor * 0.42);

    // Stratospheric high-altitude ambient sky light
    float3 ambientLight = float3(0.06, 0.08, 0.14);

    float3 finalColor = input.color * (ambientLight + directLight);

    return float4(finalColor, alpha);
}

// Whiteout Delarus - Step 17 (1:1 Part XV): Human Eye Adaptation & Exposure Compute Shader (Slang)
// Vulkan 1.3+ / 1.4 Dynamic Rendering
// - 64-Bin Histogram Reduction with Outlier Rejection (bottom 10% dark / top 2% specular glints)
// - Asymmetric Rhodopsin Pupil Kinetics (fast dark->bright constriction, slow bright->dark dilation)
// - ISO 100 Photometric Camera Exposure Evaluation

[[vk::binding(0, 0)]]
RWStructuredBuffer<uint> histogramBuffer; // 64 uints

[[vk::binding(1, 0)]]
RWStructuredBuffer<float> exposureBuffer; // [0] = adaptedLum, [1] = currentExposure, [2] = targetLum, [3] = padding

struct AdaptPushConstants {
    uint totalPixels;
    float deltaTime;
    float minLogLum;      // -8.0
    float maxLogLum;      // +16.0
    float adaptSpeedUp;   // 3.8 s^-1 (pupil constriction)
    float adaptSpeedDown; // 1.2 s^-1 (rhodopsin recovery)
    float exposureKey;    // Photometric calibration constant K (~1.15)
};

[[vk::push_constant]]
ConstantBuffer<AdaptPushConstants> pc;

[shader("compute")]
[numthreads(1, 1, 1)]
void computeMain() {
    uint totalCount = 0;
    for (uint i = 0; i < 64; i++) {
        totalCount += histogramBuffer[i];
    }

    if (totalCount == 0) {
        return;
    }

    // Discard outlier percentiles:
    // Low cutoff: bottom 10% (shadow voids / black sky borders)
    // High cutoff: top 2% (solar disc glints / specular spikes)
    float lowCutoff = float(totalCount) * 0.10;
    float highCutoff = float(totalCount) * 0.98;

    float accumulatedWeight = 0.0;
    float accumulatedLogLum = 0.0;
    uint pixelAccum = 0;

    float logRange = pc.maxLogLum - pc.minLogLum;
    float binStep = logRange / 64.0;

    for (uint i = 0; i < 64; i++) {
        uint count = histogramBuffer[i];
        uint prevAccum = pixelAccum;
        pixelAccum += count;

        // Clip count within [lowCutoff, highCutoff]
        float minP = max(float(prevAccum), lowCutoff);
        float maxP = min(float(pixelAccum), highCutoff);
        float validPixels = max(maxP - minP, 0.0);

        if (validPixels > 0.0) {
            float binLogLum = pc.minLogLum + (float(i) + 0.5) * binStep;
            accumulatedLogLum += binLogLum * validPixels;
            accumulatedWeight += validPixels;
        }

        // Clear histogram bin for next frame
        histogramBuffer[i] = 0;
    }

    float targetLum = 1.0;
    if (accumulatedWeight > 0.0) {
        float avgLogLum = accumulatedLogLum / accumulatedWeight;
        targetLum = exp2(avgLogLum);
    }

    targetLum = clamp(targetLum, 0.005, 50000.0);

    float prevAdaptedLum = exposureBuffer[0];
    if (prevAdaptedLum <= 0.0001 || isnan(prevAdaptedLum)) {
        prevAdaptedLum = targetLum;
    }

    // Asymmetric human pupil adaptation kinetics
    float speed = (targetLum > prevAdaptedLum) ? pc.adaptSpeedUp : pc.adaptSpeedDown;
    float dt = clamp(pc.deltaTime, 0.001, 0.1);
    float adaptFactor = 1.0 - exp(-dt * speed);
    float newAdaptedLum = lerp(prevAdaptedLum, targetLum, adaptFactor);

    // Photometric Camera Exposure: K / adaptedLum
    float newExposure = pc.exposureKey / max(newAdaptedLum, 0.001);
    // Clamp exposure to realistic dynamic range bounds
    newExposure = clamp(newExposure, 0.0008, 12.0);

    exposureBuffer[0] = newAdaptedLum;
    exposureBuffer[1] = newExposure;
    exposureBuffer[2] = targetLum;
    exposureBuffer[3] = 0.0;
}

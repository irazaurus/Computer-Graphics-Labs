Texture2D gInputImage : register(t0);
Texture2D gDepthMap   : register(t1);
Texture2D gNormalMap  : register(t2);

SamplerState gSampler : register(s0);

cbuffer PostProcessSettings : register(b0)
{
    // Blur settings
    float gFocusDistance;
    float gFocusRange;
    float gNearBlurStrength;
    float gFarBlurStrength;
    
    // Chromatic Aberration settings
    float2 gChromaticDirection;
    float gChromaticIntensity;
    float gChromaticDistanceScale;
    
    float gEffectIntensity; // 0 - no effects, 1 - full effects
    int gEffectType; // 0 - blur, 1 - aberration, 2 - all
    float gPadding[2];
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID)
{
    VertexOut vout;
    
    // Generating fullscreen triangle
    float2 texcoord = float2((vid << 1) & 2, vid & 2);
    vout.PosH = float4(texcoord * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    vout.TexC = texcoord;
    
    return vout;
}

float4 ChromaticAberration(float2 texCoord, float intensity, float2 direction)
{
    float2 texOffset = float2(1.0f / 1280.0f, 1.0f / 720.0f) * intensity;
    
    float2 offsetR = direction * texOffset * 1.5f;
    float2 offsetG = direction * texOffset * 0.5f;
    float2 offsetB = -direction * texOffset * 1.0f;
    
    float r = gInputImage.Sample(gSampler, texCoord + offsetR).r;
    float g = gInputImage.Sample(gSampler, texCoord + offsetG).g;
    float b = gInputImage.Sample(gSampler, texCoord + offsetB).b;
    
    return float4(r, g, b, 1.0f);
}

float4 LensBlur(float2 texCoord, float depth)
{
    // Focus distance, 0 in focus, >1 out of focus
    float focusDist = abs(depth - gFocusDistance);
    float2 direction = float2(1.0, 1.0);
    
    float blurStrength = 0.0f;
    if (depth < gFocusDistance)
    {
        blurStrength = smoothstep(0.0, gFocusRange, focusDist) * gNearBlurStrength;
    }
    else
    {
        blurStrength = smoothstep(0.0, gFocusRange, focusDist) * gFarBlurStrength;
    }
    
    if (blurStrength <= 0.0f)
        return gInputImage.Sample(gSampler, texCoord);
    
    float2 texOffset = float2(1.0f / 1280.0f, 1.0f / 720.0f) * blurStrength;
    
    const float weights[5] = { 0.227027f, 0.1945946f, 0.1216216f, 0.054054f, 0.016216f };
    
    float4 color = gInputImage.Sample(gSampler, texCoord) * weights[0];
    
    for (int i = 1; i < 5; ++i)
    {
        float2 offset = direction * texOffset * i;
        color += gInputImage.Sample(gSampler, texCoord + offset) * weights[i];
        color += gInputImage.Sample(gSampler, texCoord - offset) * weights[i];
    }
    
    return color;
}

float4 PS(VertexOut pin) : SV_Target
{
    uint2 pixelC = pin.PosH.xy;
    float4 color = gInputImage.Load(int3(pixelC, 0));
    
    if (gEffectIntensity <= 0.0f)
        return color;
    
    float depth = gDepthMap.Load(int3(pixelC, 0)).w;
    
    switch (gEffectType)
    {
        case 0: // Lens blur only
            return LensBlur(pin.TexC, depth);
            
        case 1: // Chromatic Aberration only
            return lerp(color,
                      ChromaticAberration(pin.TexC, gChromaticIntensity, gChromaticDirection),
                      gEffectIntensity);
            
        case 2: // Combined
            float4 blurred = LensBlur(pin.TexC, depth);
            float4 chromatic = ChromaticAberration(pin.TexC, gChromaticIntensity, gChromaticDirection);
            return lerp(color, lerp(blurred, chromatic, 0.5f), gEffectIntensity);
            
        default:
            return color;
    }
}
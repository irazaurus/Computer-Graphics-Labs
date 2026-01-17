#define PI 3.14159265f

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
    float gEffectType; // 0 - blur, 1 - aberration, 2 - all
    float2 gPadding;
    
    // Atmosphere settings
    float3 BetaRayleigh;
    float RayleighScaleHeight;
    float3 BetaMieSca;
    float MieScaleHeight;
    float3 BetaMieExt;
    float MieG;
    float3 SunDirection;
    float SunIntensity;
    
    float GroundLevelY;
    float AtmosphereTopY;
    float DensityScale;
    float _pad0;
    
    float3 GroundAlbedo;
    float _pad1;
};

cbuffer cbPass : register(b1)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
};

float3 RestoreWorldPosition(float2 UV, float depth)
{
    //magic DirectX texcoord mutations
    float4 clipPos;
    clipPos.x = UV.x * 2.0f - 1.0f;
    clipPos.y = 1.0f - UV.y * 2.0f;
    clipPos.z = depth;
    clipPos.w = 1.0f;

    //transform into world space
    float4 viewPos = mul(clipPos, gInvViewProj);
    viewPos.xyz /= viewPos.w;

    return viewPos.xyz;
}

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID)
{
    float2 verts[3] =
    {
        float2(-1, -1),
        float2(-1, 3),
        float2(3, -1)
    };
    
    
    VertexOut vout;
    vout.PosH = float4(verts[vid], 0, 1);
    return vout;
    
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

// slide 17
float RayleighPhase(float mu)
{
    return 3.0f / (16.0f * PI) * (1.0f + mu * mu);
}

// slide 20
float MiePhaseHG(float mu, float g)
{
    return (1.0f - g * g) / (4.0f * PI * pow(1.0f + g * g - 2.0f * g * mu, 1.5f));
}

bool SunVisible(float3 position, float3 sunDirection)
{
    if (sunDirection.y <= 0.0f)
        return false;
    
    return true;
}

// - transmittance – current color is attenuated by the atmosphere (the farther we look, the denser the air -> the darker it becomes through the atmosphere)
// - inscatter – air molecules scatter light -> a bluish tint, and the direct sunlight is scattered along its path toward you
struct AerialResult
{
    float3 inscatter;
    float3 transmittance;
};

AerialResult IntegrateAerial(float3 camPos, float tMax, float3 V)
{
    tMax = max(tMax, 0.5f);
    
    int N = clamp((int) ceil(tMax / 2500.0f), 16, 64);
    float dt = tMax / N;

    float3 pos = camPos + V * (0.5f * dt);
    float3 Absorption = float3(1, 1, 1);
    float3 L = float3(0, 0, 0);

    float mu = dot(SunDirection, V);
    
    float sunVisibilityFactor = SunVisible(camPos, SunDirection) ? 1.0f : 0.0f;
    float baseScattering = 0.01f * (1.0f - saturate(-SunDirection.y));
    
    float phaseR = RayleighPhase(mu) * sunVisibilityFactor + baseScattering;
    float phaseM = MiePhaseHG(mu, MieG) * sunVisibilityFactor;

    [loop]
    for (int i = 0; i < N; i++)
    {
        float h = max(0.0f, pos.y - GroundLevelY);
        float dR = exp(-h / max(1e-3, RayleighScaleHeight)) * DensityScale;
        float dM = exp(-h / max(1e-3, MieScaleHeight)) * DensityScale;
        
        float3 sigma_a = BetaRayleigh * dR + BetaMieExt * dM;

        // Absorption for sun light (slide 5)
        float3 T_light = exp(-sigma_a * 50000.0f);

        // In-Scattering (slide 7 in the formula to the right of the '+')
        float3 S = (phaseR * BetaRayleigh * dR + phaseM * BetaMieSca * dM) * SunIntensity * T_light;

        // Total light collected
        L += Absorption * S * dt;
        
        // Absorption (slide 5) - if expand the formula from the slide using a Taylor series, it becomes equivalent to the formula below (to the exponential) for small dt
        // This is also the Lambert–Beer law
        Absorption *= exp(-sigma_a * dt);

        pos += V * dt;
    }

    AerialResult r;
    r.inscatter = L;
    r.transmittance = Absorption;
    return r;
}


float4 PS(VertexOut pin) : SV_Target
{
    uint2 pixelC = pin.PosH.xy;
    float4 color = gInputImage.Load(int3(pixelC, 0));
    
    float depth = gDepthMap.Load(int3(pixelC, 0)).w;
    float2 UV = (float2) pixelC / gRenderTargetSize;
    float3 WorldPosition = RestoreWorldPosition(UV, depth);
    
    float3 normal = gNormalMap.Load(int3(pixelC, 0)).rgb;
    float normalLen2 = dot(normal, normal);
    
    // *** Atmosphere ***
    
    
    // If the depth in the texel is close to 1.0f, it is sky. But if we will have custom scheme in GBuffer depth channel, this check could not work.
    // So we can check the square of normal. If it is close to 0.0f, then we did not touch this texel and 99.9% probability it is a sky
    bool isSky = (depth >= 1.0f - 1e-6f) || (normalLen2 < 1e-6f);


    float3 camPos = gEyePosW;
    // Infinitely distant background point in the direction of the camera through this pixel (don't write 1.0f or -1.0f as it makes the equation reduce to 0)
    float3 P_far = RestoreWorldPosition(UV, 0.0f);
    // Normalized view vector
    float3 V = normalize(P_far - camPos);

    
    if (isSky)
    {
        float tMaxSky;

        // If V.y > 0 -> the ray is directed upward (into the sky). Then the intersection point of the ray with the upper boundary of the atmosphere is computed as follows
        if (V.y > 1e-6f)
            tMaxSky = (AtmosphereTopY - camPos.y) / V.y;
        // If V.y < 0, the ray is looking downward, toward the ground. Look for the intersection point with the lower boundary of the atmosphere
        else if (V.y < -1e-6f)
        {
            float tGround = (GroundLevelY - camPos.y) / V.y;
            tMaxSky = max(0.0f, tGround);
        }
        // If V.y ~ 0, the ray travels almost horizontally; it either does not intersect the atmosphere or will intersect it very far away. Therefore, a very large distance is assigned.
        else
            tMaxSky = 50000.0f;
        
        // Ray marching
        AerialResult arSky = IntegrateAerial(camPos, tMaxSky, V);
        
        float sunHeightFactor = saturate(SunDirection.y * 2.0f);
        arSky.inscatter *= sunHeightFactor;
        
        if (SunDirection.y < 0.0f)
        {
            float nightGlow = 0.01f * (1.0f - saturate(-SunDirection.y * 0.5f));
            arSky.inscatter += BetaRayleigh * nightGlow;
        }

        color.rgb = color.rgb * arSky.transmittance + arSky.inscatter;
    }
    // Otherwise pixel belongs to a scene object
    else
    {
        // For objects we take tMaxGeo = the distance from the camera to the object. So that the atmosphere is integrated only up to the object, not 50 km ahead
        float tMaxGeo = length(WorldPosition - camPos);
    
        // Ray marching
        AerialResult arGeo = IntegrateAerial(camPos, tMaxGeo, V);
    
        float3 objColor = color.rgb * arGeo.transmittance;
    
        float fogStartDepth = 0.1f;
        float fogEndDepth = 0.95f;
        float fogFactor = saturate((depth - fogStartDepth) / max(1e-3f, fogEndDepth - fogStartDepth));
    
        float fogIntensity = 10.0f;

        float3 fogAdd = arGeo.inscatter * fogIntensity;
    
        color.rgb = objColor + fogAdd * fogFactor;
    }
    
    if (gEffectIntensity <= 0.0f)
        return color;
    
    // Post Effects
    /*switch (gEffectType)
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
    }*/
    
    return color;
}
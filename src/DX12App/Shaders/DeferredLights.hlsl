#include "LightingUtil.hlsl"

#define PI 3.14159265359

Texture2DArray gShadowMap   : register(t0);
Texture2D gDiffuse          : register(t1);
Texture2D gZW               : register(t2);
Texture2D gNormal           : register(t3);
Texture2D gMaterialAlbedo   : register(t4);
Texture2D gMaterialFresnelRoughness : register(t5);
TextureCube gSkyDiffuse             : register(t6);
TextureCube gSkyIrradiance          : register(t7);
Texture2D gSkyBrdf                  : register(t8);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);

cbuffer cbPass : register(b0)
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

cbuffer LightConstants : register(b1)
{
    Light light;
    float3 LColor;
    int LightType; //0 - directional; 1 - point; 2 - spot
    float4x4 LWorld;
    float4x4 LViewProj[6];
    float4x4 LShadowTransform[6];
};

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(1.0 - roughness, F0) - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

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

float CalcRTShadow(uint2 TexelCoord)
{
    //blur depth map
    static const float Kernel[41] =
    {
        0.0000000000000000, 0.0000000000000001, 0.0000000000000052,
        0.0000000000001253, 0.0000000000021519, 0.0000000000278349,
        0.0000000002747260, 0.0000000021644900, 0.0000000137540400,
        0.0000000724616000, 0.0000003141800000, 0.0000011379900000,
        0.0000035235800000, 0.0000093477300000, 0.0000215682000000,
        0.0000438293000000, 0.0000791458000000, 0.0001280230000000,
        0.0001872210000000, 0.0002499590000000, 0.0003068310000000,
        0.0003479390000000, 0.0003655270000000, 0.0003561520000000,
        0.0003214440000000, 0.0002675280000000, 0.0002036640000000,
        0.0001402560000000, 0.0000861180000000, 0.0000469990000000,
        0.0000224910000000, 0.0000094450000000, 0.0000034450000000,
        0.0000010740000000, 0.0000002830000000, 0.0000000620000000,
        0.0000000110000000, 0.0000000020000000, 0.0000000000000000,
        0.0000000000000000, 0.0000000000000000
    };
    
    float blurStrength = 1.0f;
    
    float result = 0.0f;
    float kernelSum = 0.0f;
    
    [unroll]
    for (int x = -20; x <= 20; x++)
    {
        [unroll]
        for (int y = -20; y <= 20; y++)
        {
            float2 offset = float2(x, y) / gRenderTargetSize * blurStrength;
            uint2 sampleCoord = TexelCoord + uint2(offset * gRenderTargetSize);
            
            if (sampleCoord.x < gRenderTargetSize.x && sampleCoord.y < gRenderTargetSize.y)
            {
                float kernelValue = Kernel[x + 20] * Kernel[y + 20];
                float sampleValue = gShadowMap.Load(int4(sampleCoord, 0, 0)).x;
                result += sampleValue * kernelValue;
                kernelSum += kernelValue;
            }
        }
    }
    
    if (kernelSum > 0.0f)
    {
        result /= kernelSum;
    }
    
    return result;
}

struct VertexIn
{
    float3 PosL : POSITION;
};

struct VertexOut
{
    float4 PosH : SV_Position;
};

VertexOut VS(uint vertexID : SV_VertexID)
{
    //full-screen quad
    float2 verts[3] =
    {
        float2(-1, -1),
        float2(-1, 3),
        float2(3, -1)
    };
    
    VertexOut vo;
    vo.PosH = float4(verts[vertexID], 0, 1);
    return vo;
}

VertexOut LightsGeometryVS(VertexIn vi)
{
    VertexOut vo;
    
    vo.PosH = mul(mul(float4(vi.PosL, 1.f), LWorld), gViewProj);
    
    return vo;
}

float4 PS(VertexOut vo) : SV_Target
{
    float2 uv = vo.PosH.xy / gRenderTargetSize;
    uint2 pixelC = vo.PosH.xy;
    float4 diffuseAlbedo = gDiffuse.Load(int3(pixelC, 0));
    float4 zw = gZW.Load(int3(pixelC, 0));
    float4 normalChannel = gNormal.Load(int3(pixelC, 0));
    float3 normal = normalChannel.rgb;
    float metallic = normalChannel.a;
    float4 matAlbedo = gMaterialAlbedo.Load(int3(pixelC, 0));
    float4 matFrR = gMaterialFresnelRoughness.Load(int3(pixelC, 0));
    diffuseAlbedo = diffuseAlbedo * matAlbedo;
    float roughness = matFrR.a;
    
    // Vector from point being lit to eye. 
    float3 posW = RestoreWorldPosition(uv, zw.a);
    float3 toEyeW = gEyePosW - posW;
    float distToEye = length(toEyeW);
    toEyeW /= distToEye; // normalize
    float NdotV = max(dot(normal, toEyeW), 0.0);

    const float shininess = 1.0f - matFrR.a;
    Material mat = { diffuseAlbedo, matFrR.rgb, shininess };
    float shadowFactor = 1.0f;
    
    float4 currentLight;
    
    if (LightType == 0) // direction
        {
        
        shadowFactor = CalcRTShadow(pixelC);
        
        float3 lightDir = normalize(-light.Direction);
        float3 halfVec = normalize(toEyeW + lightDir);
        float NdotL = max(dot(normal, lightDir), 0.0);

        float3 F0 = lerp(0.04.xxx, diffuseAlbedo.rgb, metallic);
        float3 F = FresnelSchlick(max(dot(halfVec, toEyeW), 0.0), F0);

        float NDF = DistributionGGX(normal, halfVec, roughness);

        float G = GeometrySmith(normal, toEyeW, lightDir, roughness);

        // Cook-Torrance BRDF
        float3 numerator = NDF * G * F;
        float denominator = 4.0 * NdotV * NdotL + 0.001;
        float3 specular = numerator / denominator;

        float3 kS = F;
        float3 kD = 1.0 - kS;
        kD *= (1.0 - metallic);

        float3 radiance = light.Strength * LColor;

        float3 Lo = (kD * diffuseAlbedo.rgb / PI + specular) * radiance * NdotL;
        
        currentLight = float4(shadowFactor * Lo, 1.0f);
    } 
    else if (LightType == 1) // point
        {
            float3 lightToPixel = posW - light.Position;
            float distToLight = length(lightToPixel);
            lightToPixel /= distToLight;
    
            // determine shadow map cube face index
            float3 absDir = abs(lightToPixel);
            uint faceIndex = 0;
            if (absDir.x >= absDir.y && absDir.x >= absDir.z)
                faceIndex = (lightToPixel.x > 0) ? 0 : 1;
            else if (absDir.y >= absDir.z)
                faceIndex = (lightToPixel.y > 0) ? 2 : 3;
            else
                faceIndex = (lightToPixel.z > 0) ? 4 : 5;
            
            shadowFactor = CalcRTShadow(pixelC);
            currentLight = float4(ComputePointLight(light, mat, posW, normal, toEyeW) * shadowFactor, 1.0f);
    } 
    else // spot
        {
            shadowFactor = CalcRTShadow(pixelC);
            currentLight = float4(ComputeSpotLight(light, mat, posW, normal, toEyeW) * shadowFactor, 1.0f);
        }
    
    currentLight = currentLight * float4(LColor, 1.f);
    
    return currentLight;
}

float4 AmbientPS(VertexOut vo) : SV_Target
{
    float2 uv = vo.PosH.xy / gRenderTargetSize;
    uint2 pixelC = vo.PosH.xy;
    float4 diffuseAlbedo = gDiffuse.Load(int3(pixelC, 0));
    float4 zw = gZW.Load(int3(pixelC, 0));
    float4 normalChannel = gNormal.Load(int3(pixelC, 0));
    float4 matAlbedo = gMaterialAlbedo.Load(int3(pixelC, 0));
    float4 matFrR = gMaterialFresnelRoughness.Load(int3(pixelC, 0));
    float metallic = normalChannel.a;
    float roughness = matFrR.a;
    
    float3 albedo = diffuseAlbedo.rgb;
    float3 normal = normalize(normalChannel.rgb);
    
    if (length(normal) < 0.01f)
        discard;
    
    float3 worldPos = RestoreWorldPosition(uv, zw.w);
    float3 viewDir = normalize(gEyePosW - worldPos);
    float NdotV = max(dot(normal, viewDir), 0.0);
    
    // --- PBR IBL ---
    float3 F0 = lerp(0.04.xxx, albedo, metallic);
    float3 F = FresnelSchlickRoughness(NdotV, F0, roughness);
    
    float3 kS = F;
    float3 kD = 1.0 - kS;
    kD *= (1.0 - metallic);
    
    float3 irradiance = gSkyIrradiance.Sample(gsamLinearClamp, normal).rgb;
    float3 diffuse = irradiance * albedo;
    
    uint width, height, NumMips;
    gSkyDiffuse.GetDimensions(0, width, height, NumMips);
    
    float3 R = reflect(-viewDir, normal);
    float3 prefilteredColor = gSkyDiffuse.SampleLevel(gsamLinearClamp, R, roughness * NumMips).rgb;
    float2 brdf = gSkyBrdf.Sample(gsamLinearClamp, float2(NdotV, roughness)).rg;
    float3 specular = prefilteredColor * (F * brdf.x + brdf.y);
    
    float ao = 1.0f;
    float3 ambient = (kD * diffuse + specular) * ao * 0.2f.xxx;
    
    return float4(ambient, diffuseAlbedo.a);
}
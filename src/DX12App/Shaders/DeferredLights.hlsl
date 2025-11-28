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
    static const float Kernel[21] =
    {
        0.0000000000000000, 0.0000000000014940, 0.0000000002951600,
        0.0000000273766000, 0.0000014462100000, 0.0000487016000000,
        0.0010722200000000, 0.0154509000000000, 0.1479700000000000,
        0.8936300000000000, 1.0000000000000000,
        0.8936300000000000, 0.1479700000000000, 0.0154509000000000,
        0.0010722200000000, 0.0000487016000000, 0.0000014462100000,
        0.0000000273766000, 0.0000000002951600, 0.0000000000014940,
        0.0000000000000000
    };
    
    float blurStrength = 2.0f;
    
    float result = 0.0f;
    float kernelSum = 0.0f;
    
    [unroll]
    for (int x = -10; x <= 10; x++)
    {
        [unroll]
        for (int y = -10; y <= 10; y++)
        {
            float2 offset = float2(x, y) / gRenderTargetSize * blurStrength;
            uint2 sampleCoord = TexelCoord + uint2(offset * gRenderTargetSize);
            
            if (sampleCoord.x < gRenderTargetSize.x && sampleCoord.y < gRenderTargetSize.y)
            {
                float kernelValue = Kernel[x + 10] * Kernel[y + 10];
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
#include "LightingUtil.hlsl"

Texture2D gZW                           : register(t0);
Texture2D gNormal                       : register(t1);
RaytracingAccelerationStructure gTLAS   : register(t2);

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

// functions

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

bool TraceRay(float3 origin, float3 direction, float maxDistance)
{
    RayQuery < RAY_FLAG_CULL_BACK_FACING_TRIANGLES |
             RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH > rayQuery;
    
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = normalize(direction);
    ray.TMin = 0;
    ray.TMax = maxDistance;
    
    rayQuery.TraceRayInline(gTLAS, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xff, ray);
    rayQuery.Proceed();
    
    return rayQuery.CommittedStatus() != COMMITTED_NOTHING;
}

// structures

struct VertexIn
{
    float3 PosL : POSITION;
};

struct VertexOut
{
    float4 PosH : SV_Position;
};

struct psout
{
    float depth : SV_Depth;
};

// shaders

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

psout PS(VertexOut pin)
{
    psout res;
    
    float2 UV = pin.PosH.xy / gRenderTargetSize;
    float screenDepth = gZW.Load(int3(pin.PosH.xy, 0)).w;
    float3 WorldPos = RestoreWorldPosition(UV, screenDepth);
    float3 Normal = gNormal.Load(int3(pin.PosH.xy, 0)).rgb;
    
    if (screenDepth >= 1.0f)
    {
        res.depth = 1.0f;
        return res;
    }
    
    float shadowFactor = 1.0f;
    //Scaling jitter by distance from camera(cant use depth since its 0.9999 most of the time(DAMN YOU FARZ)(or depthbias idk))
    float DistanceToCamera = length(WorldPos - gEyePosW);
    float MaxJitterDistance = 50.0f;
    float MinJitterDistance = 10.0f;

    float distanceFactor = saturate((DistanceToCamera - MinJitterDistance) / (MaxJitterDistance - MinJitterDistance));
    float rayJitter = 0.005f * distanceFactor;

    
    // RayTrace based on light type
    if (LightType == 0) // Directional Light
    {
        float3 RayOrigin = WorldPos + Normal * 0.1f;
        float3 lightDir = normalize(-light.Direction + float3(
            (frac(sin(dot(UV, float2(12.9898, 78.233))) * 43758.5453) - 0.5f) * rayJitter,
            (frac(sin(dot(UV, float2(39.346, 11.135))) * 43758.5453) - 0.5f) * rayJitter,
            (frac(sin(dot(UV, float2(67.89, 45.321))) * 43758.5453) - 0.5f) * rayJitter
        ));
        
        bool hit = TraceRay(RayOrigin, lightDir, 1000.0f);
        shadowFactor = hit ? 0.0f : 1.0f;
    }
    else if (LightType == 1) // Point Light
    {
        float3 toLight = light.Position - WorldPos;
        float distanceToLight = length(toLight);
        float3 lightDir = normalize(toLight / distanceToLight + float3(
            (frac(sin(dot(UV, float2(23.456, 89.012))) * 43758.5453) - 0.5f) * rayJitter,
            (frac(sin(dot(UV, float2(56.789, 34.567))) * 43758.5453) - 0.5f) * rayJitter,
            (frac(sin(dot(UV, float2(90.123, 67.890))) * 43758.5453) - 0.5f) * rayJitter
        ));
        
        float3 rayOrigin = WorldPos + normalize(Normal) * 0.1f;
        
        bool hit = TraceRay(rayOrigin, lightDir, distanceToLight - 0.2f);
        shadowFactor = hit ? 0.0f : 1.0f;
    }
    else if (LightType == 2) // Spot Light
    {
        float3 toLight = light.Position - WorldPos;
        float distanceToLight = length(toLight);
        float3 lightDir = normalize(toLight / distanceToLight + float3(
            (frac(sin(dot(UV, float2(23.456, 89.012))) * 43758.5453) - 0.5f) * rayJitter,
            (frac(sin(dot(UV, float2(56.789, 34.567))) * 43758.5453) - 0.5f) * rayJitter,
            (frac(sin(dot(UV, float2(90.123, 67.890))) * 43758.5453) - 0.5f) * rayJitter
        ));
        
        float3 rayOrigin = WorldPos + Normal * 0.1f;
        
        bool hit = TraceRay(rayOrigin, lightDir, distanceToLight - 0.2f);
        shadowFactor = hit ? 0.0f : 1.0f;
    }
    
    //draw result into Depth stencil
    res.depth = shadowFactor;
    return res;
}

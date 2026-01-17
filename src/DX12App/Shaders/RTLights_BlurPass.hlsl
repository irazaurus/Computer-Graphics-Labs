#include "LightingUtil.hlsl"

Texture2D InputTexture : register(t0);

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

psout PS(VertexOut pin)
{
    psout res;
    
    float InputDepth = InputTexture.Load(int3(pin.PosH.xy, 0)).x;
    float BlurredDepth = 0.0;
    
    const int kernelRadius = 10;
    float weightSum = 0.0;
    
    for (int y = -kernelRadius; y <= kernelRadius; y++)
    {
        for (int x = -kernelRadius; x <= kernelRadius; x++)
        {
            int2 samplePos = pin.PosH.xy + int2(x, y);
         
            if (samplePos.x < 0 || samplePos.y < 0 ||
                samplePos.x >= gRenderTargetSize.x || samplePos.y >= gRenderTargetSize.y)
                continue;
            
            float depthSample = InputTexture.Load(int3(samplePos, 0)).x;
            
            float distance = sqrt(x * x + y * y);
            float sigma = kernelRadius / 2.5;
            float weight = exp(-(distance * distance) / (2.0 * sigma * sigma));
            
            BlurredDepth += depthSample * weight;
            weightSum += weight;
        }
    }
    
    if (weightSum > 0.0)
    {
        BlurredDepth /= weightSum;
    }
    
    res.depth = BlurredDepth;
    return res;
}

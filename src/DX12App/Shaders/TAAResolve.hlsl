Texture2D gInputImage : register(t0);
Texture2D gPrevImage   : register(t1);
Texture2D gVelocityBuf  : register(t2);

SamplerState gSampler : register(s0);

cbuffer cbPass : register(b0)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gPrevViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    int gCurrentFrame;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float2 gJitterOffset;
    float2 pad;
    float3 PrevCameraPos;
    float pad2;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
};

float4 Blur(float4 currentColor, uint2 texelCoord)
{
    float blurAmount = 3.0f;
    float4 blurredColor = float4(0, 0, 0, 0);
    float weightSum = 0.0f;
    
    const int kernelSize = 5;
    const float kernel[5][5] =
    {
        { 0.003, 0.013, 0.022, 0.013, 0.003 },
        { 0.013, 0.059, 0.097, 0.059, 0.013 },
        { 0.022, 0.097, 0.159, 0.097, 0.022 },
        { 0.013, 0.059, 0.097, 0.059, 0.013 },
        { 0.003, 0.013, 0.022, 0.013, 0.003 }
    };
   
    for (int x = -2; x <= 2; x++)
    {
        for (int y = -2; y <= 2; y++)
        {
            uint2 sampleCoord = texelCoord + uint2(x, y);
            if (all(sampleCoord >= 0 && sampleCoord < gRenderTargetSize))
            {
                float4 sampleColor = gInputImage.Load(int3(sampleCoord, 0));
                float weight = kernel[x + 2][y + 2];
                blurredColor += sampleColor * weight;
                weightSum += weight;
            }
        }
    }
        
    if (weightSum > 0)
    {
        blurredColor /= weightSum;
    }
    else
    {
        blurredColor = currentColor;
    }
    
    return blurredColor;
}


VertexOut VS(uint vid : SV_VertexID)
{    
    // Generating fullscreen triangle
    float2 verts[3] =
    {
        float2(-1, -1),
        float2(-1, 3),
        float2(3, -1)
    };
    
    VertexOut vout;
    vout.PosH = float4(verts[vid], 0, 1);
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    uint2 TexelCoord = pin.PosH.xy;

    float2 MotionVector = gVelocityBuf.Load(int3(TexelCoord, 0)).xy;
    float MotionLength = length(MotionVector);
    
    //return float4(MotionVector, 0.f, 1.f);
    
    float2 PrevTexelCoord = TexelCoord + MotionVector;
    float4 CurrFrameColor = gInputImage.Load(int3(TexelCoord, 0));
    
    if (MotionVector.x == 0 && MotionVector.y == 0)
        CurrFrameColor = Blur(CurrFrameColor, TexelCoord);
    
    if (length(gEyePosW - PrevCameraPos) > 0.001f)
    {
        return CurrFrameColor;
    }
    
    float4 PrevFrameColor = CurrFrameColor;
    
    bool IsPrevUVValid = all(PrevTexelCoord >= 0 && PrevTexelCoord < gRenderTargetSize);
    if (IsPrevUVValid)
    {
        PrevFrameColor = gPrevImage.Load(int3(PrevTexelCoord, 0));
        
        // Color clamping
        float4 minColor = CurrFrameColor;
        float4 maxColor = CurrFrameColor;
        
        for (int x = -1; x <= 1; x++)
        {
            for (int y = -1; y <= 1; y++)
            {
                uint2 neighborCoord = TexelCoord + uint2(x, y);
                if (all(neighborCoord >= 0 && neighborCoord < gRenderTargetSize))
                {
                    float4 neighborColor = gInputImage.Load(int3(neighborCoord, 0));
                    minColor = min(minColor, neighborColor);
                    maxColor = max(maxColor, neighborColor);
                }
            }
        }
        
        PrevFrameColor = clamp(PrevFrameColor, minColor, maxColor);
        
        float BlendFactor = 0.9 * saturate( 1 / MotionLength / 50.0); // more movement == less influence
        return lerp(CurrFrameColor, PrevFrameColor, BlendFactor);
    }
    
    return CurrFrameColor;
}
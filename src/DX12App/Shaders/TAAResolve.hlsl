Texture2D gInputImage : register(t0);
Texture2D gPrevImage   : register(t1);
Texture2D gVelocityBuf  : register(t2);

SamplerState gSampler : register(s0);

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

float4 PS(VertexOut pin) : SV_Target
{
    uint2 pixelC = pin.PosH.xy;
    float4 color = gInputImage.Load(int3(pixelC, 0));
    float4 prevColor = gPrevImage.Load(int3(pixelC, 0));
    
    return color * 0.1f + prevColor * 0.9f;
}
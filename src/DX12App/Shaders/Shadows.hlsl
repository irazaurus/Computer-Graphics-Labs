#include "Common.hlsl"

cbuffer LightConstants : register(b3)
{
    float3  LStrength;
    float   LFalloffStart; // point/spot light only
    float3  LDirection; // directional/spot light only
    float   LFalloffEnd; // point/spot light only
    float3  LPosition; // point/spot light only
    float   LSpotPower; // spot light only
    float3  LColor;
    int     LightType; //0 - directional; 1 - point; 2 - spot
    float4x4 LWorld;
    float4x4 LViewProj[6];
    float4x4 LShadowTransform[6];
};

struct VertexIn
{
	float3 PosL    : POSITION;
	float2 TexC    : TEXCOORD;
};

struct VertexOut
{
	float4 PosW    : POSITION;
	float2 TexC    : TEXCOORD;
};

struct GSOut
{
    float4 PosCS : SV_Position;
    uint ArrInd : SV_RenderTargetArrayIndex;
    float2 TexC : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut)0.0f;
	
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW;
	
    return vout;
}

[instance(6)]
[maxvertexcount(3)]
void GS(triangle VertexOut p[3], in uint id : SV_GSInstanceID, inout TriangleStream<GSOut> stream)
{
   // draw 4 cascades for directional lights
    switch (LightType)
    {
        case 0:
            if (id > 3) return;
            break;
        case 2:
            if (id > 0) return;
            break;
    }
    
    for (int i = 0; i < 3; i++)
    {
        GSOut Out;
        Out.PosCS = mul(float4(p[i].PosW.xyz, 1.f), LViewProj[id]);
        Out.TexC = p[i].TexC;
        Out.ArrInd = id;
        stream.Append(Out);
    }
}

// This is only used for alpha cut out geometry, so that shadows 
// show up correctly.  Geometry that does not need to sample a
// texture can use a NULL pixel shader for depth pass.
void PS(GSOut pin) 
{
    // lol
}

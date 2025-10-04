#include "Common.hlsl"

Texture2D gDiffuseMap : register(t0);
Texture2D gNormalMap : register(t1);
Texture2D gDisplacementMap : register(t2);
Texture2DArray gShadowMap : register(t3);

struct VertexIn
{
    float3 Tangent : TANGENT;
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float3 Tangent : TANGENT;
    float4 PosW : POSITION;
    float4 PosH : SV_POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};
 
struct PatchTess
{
    float EdgeTess[3] : SV_TessFactor;
    float InsideTess : SV_InsideTessFactor;
};

struct GBufferData
{
    float4 diffuse : SV_TARGET0;
    float4 zwzanashih_RGBA32F : SV_TARGET1;
    float4 normal : SV_TARGET2;
    float4 materialAlbedo : SV_TARGET3;
    float4 MaterialFresnelRoughness : SV_TARGET4;
};

bool isVertexOnEdge(float2 TexC1, float2 TexC2)
{
    float scale = 1e-3;
    
    if (abs(TexC1.x) < scale && abs(TexC2.x) < scale)
        return true;
    if (abs(TexC1.y) < scale && abs(TexC2.y) < scale)
        return true;
    
    
    if (abs(1.f - TexC1.x) < scale && abs(1.f - TexC2.x) < scale)
        return true;
    if (abs(1.f - TexC1.y) < scale && abs(1.f - TexC2.y) < scale)
        return true;
    
    return false;
}

VertexIn tessVS(VertexIn vin)
{
    vin.TexC = mul(float4(vin.TexC, 0.f, 1.f), gTexTransform).xy;
    return vin;
}

VertexOut VS(VertexIn vin)
{
    VertexOut vo;
    
    vo.Tangent = vin.Tangent;
    vo.PosW = mul(float4(vin.PosL, 1.0f), gWorld);
    vo.PosH = mul(vo.PosW, gViewProj);
    vo.NormalL = vin.NormalL;
    vo.TexC = mul(float4(vin.TexC, 0.f, 1.f), gTexTransform).xy;
    
    return vo;
}

VertexOut displaceVS(VertexIn vin)
{
    VertexOut vo;
    
    vo.Tangent = vin.Tangent;
    vo.PosW = mul(float4(vin.PosL, 1.0f), gWorld);
    vo.NormalL = vin.NormalL;
    vo.TexC = mul(float4(vin.TexC, 0.f, 1.f), gTexTransform).xy;
    
    // Displacement mapping
    uint width, height;
    float disp = gDisplacementMap.SampleLevel(gsamAnisotropicClamp, vo.TexC, 0).r;
    vo.PosW.y += disp * 250.0f;
    
    vo.PosH = mul(vo.PosW, gViewProj);
    
    return vo;
}


PatchTess ConstantHS(InputPatch<VertexIn, 3> patch, uint patchID : SV_PrimitiveID)
{
    PatchTess pt;
    float tess = 1.f;

	// Uniformly tessellate the patch.
    
    pt.EdgeTess[0] = tess;
    pt.EdgeTess[1] = tess;
    pt.EdgeTess[2] = tess;
	
    pt.InsideTess = tess;
	
    return pt;
}

[domain("tri")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("ConstantHS")]
[maxtessfactor(64.0f)]
VertexIn HS(InputPatch<VertexIn, 3> p,
           uint i : SV_OutputControlPointID,
           uint patchId : SV_PrimitiveID)
{
    VertexIn hout;
	
    hout.PosL = p[i].PosL;
    hout.NormalL = p[i].NormalL;
    hout.TexC = p[i].TexC;
    hout.Tangent = p[i].Tangent;
	
    return hout;
}

[domain("tri")]
VertexOut DS(PatchTess patchTess,
             float3 bary : SV_DomainLocation,
             const OutputPatch<VertexIn, 3> tri)
{
    VertexOut dout;
    
    float3 p = bary.x * tri[0].PosL +
               bary.y * tri[1].PosL +
               bary.z * tri[2].PosL;
    
    if (bary.x + bary.y + bary.z != 1.0f)
    {
        p = (0.0f, 20.0f, 0.0f);

    }
    
    float2 t = bary.x * tri[0].TexC +
               bary.y * tri[1].TexC +
               bary.z * tri[2].TexC;
    t = float2(abs(t.x) - (uint) t.x, abs(t.y) - (uint) t.y);
    
    float3 norm = bary.x * tri[0].NormalL +
               bary.y * tri[1].NormalL +
               bary.z * tri[2].NormalL;

    // Displacement mapping
    uint width, height;
    gDisplacementMap.GetDimensions(width, height);
    float disp = gDisplacementMap.Load(int3(t.x * width, t.y * height, 0)).r;
    if (abs(disp) < 1e-5f)
        disp = 1.0f;
    p.y += disp * 2.0f;
    
    dout.PosW = mul(float4(p, 1.0f), gWorld);
    dout.PosH = mul(dout.PosW, gViewProj);
    dout.NormalL = norm;
    dout.Tangent = tri[0].Tangent;
    dout.TexC = t;

    return dout;
}

[maxvertexcount(21)]
void curtainsGS(triangle VertexOut p[3], inout TriangleStream<VertexOut> stream)
{
    stream.Append(p[0]);
    stream.Append(p[1]);
    stream.Append(p[2]);
    
    float downOffset = 10.f;
    int sides[3][2] = { { 0, 1 }, { 1, 2 }, { 2, 0 } };
    
    VertexOut p1, p2;
    
    // check if triangle's side is on the edge
    
    for (int i = 0; i < 3; i++)
    {
        p1 = p[sides[i][0]];
        p2 = p[sides[i][1]];
        
        if (isVertexOnEdge(p1.TexC, p2.TexC))
        {
            // if yes -- generate from it two triangles down
            VertexOut p3 = p1;
            p3.PosW.y -= downOffset;
            p3.PosH = mul(p3.PosW, gViewProj);
            
            VertexOut p4 = p2;
            p4.PosW.y -= downOffset;
            p4.PosH = mul(p4.PosW, gViewProj);
            
            stream.Append(p1);
            stream.Append(p2);
            stream.Append(p4);
            
            stream.Append(p1);
            stream.Append(p3);
            stream.Append(p4);
        }

    }
    
}

GBufferData OriginalNormalPS(VertexOut pin)
{
    GBufferData pout;
    
    float3 normalMap = gNormalMap.Sample(gsamAnisotropicWrap, pin.TexC).rgb;
    
    float4 diffuseAlbedo = gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC);

    pout.diffuse = diffuseAlbedo;
    pout.zwzanashih_RGBA32F = float4(0.f, 0.f, 0.f, pin.PosH.z);
    pout.normal = float4(normalMap, Metallic);
    pout.materialAlbedo = gDiffuseAlbedo;
    pout.MaterialFresnelRoughness = float4(gFresnelR0, gRoughness);

    return pout;
}

GBufferData DeferredPS(VertexOut pin)
{
    GBufferData pout;
    
    float3 normalMap = gNormalMap.Sample(gsamAnisotropicWrap, pin.TexC).rgb;
    if (length(normalMap) != 0.f)
    {    
	// TBN
        float3 bitangent = (cross(pin.NormalL, pin.Tangent));
        bitangent = normalize(mul(bitangent, (float3x3) gWorld));
        float3 tangent = normalize(mul(pin.Tangent, (float3x3) gWorld));
        float3 normal = normalize(mul(pin.NormalL, (float3x3) gWorld));
        float3x3 TBN = float3x3(pin.Tangent, bitangent, pin.NormalL);
    
	// normal from texture
        normalMap = normalMap * 2.0f - 1.0f;
        normalMap = normalize(mul(normalMap, TBN));
    }
    else
        normalMap = normalize(pin.NormalL);
    
    float4 diffuseAlbedo = gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC);

    pout.diffuse = diffuseAlbedo;
    pout.zwzanashih_RGBA32F = float4(0.f, 0.f, 0.f, pin.PosH.z);
    pout.normal = float4(normalMap, Metallic);
    pout.materialAlbedo = gDiffuseAlbedo;
    pout.MaterialFresnelRoughness = float4(gFresnelR0, gRoughness);

    return pout;
}
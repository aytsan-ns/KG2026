cbuffer ObjectBuffer : register(b0)
{
    row_major float4x4 model;
    float4 size;
};

cbuffer SceneBuffer : register(b1)
{
    row_major float4x4 vp;
    float4 cameraPos;
};

struct VSInput
{
    float3 pos : POSITION;
    float2 uv  : TEXCOORD;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD;
};

VSOutput vs(VSInput vertex)
{
    VSOutput result;

    float4 world = mul(float4(vertex.pos, 1.0f), model);
    result.pos = mul(world, vp);
    result.uv = vertex.uv;

    return result;
}
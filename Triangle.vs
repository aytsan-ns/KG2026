cbuffer ObjectBuffer : register(b0)
{
    row_major float4x4 model;
};

cbuffer SceneBuffer : register(b1)
{
    row_major float4x4 vp;
};

struct VSInput
{
    float3 pos : POSITION;
    float4 color : COLOR;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float4 color : COLOR;
};

VSOutput vs(VSInput vertex)
{
    VSOutput result;

    float4 world = mul(float4(vertex.pos, 1.0f), model);
    result.pos   = mul(world, vp);

    result.color = vertex.color;
    return result;
}
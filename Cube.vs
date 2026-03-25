cbuffer ObjectBuffer : register(b0)
{
    row_major float4x4 model;
    row_major float4x4 normalModel;
    float4 size;
    float4 material;
    float4 color;
};

cbuffer SceneBuffer : register(b1)
{
    row_major float4x4 vp;
    float4 cameraPos;

    int4 lightCount;

    struct Light
    {
        float4 pos;
        float4 color;
    };

    Light lights[10];
    float4 ambientColor;
};

struct VSInput
{
    float3 pos  : POSITION;
    float3 tang : TANGENT;
    float3 norm : NORMAL;
    float2 uv   : TEXCOORD;
};

struct VSOutput
{
    float4 pos      : SV_Position;
    float4 worldPos : POSITION;
    float3 tang     : TANGENT;
    float3 norm     : NORMAL;
    float2 uv       : TEXCOORD;
};

VSOutput vs(VSInput vertex)
{
    VSOutput result;

    float4 world = mul(float4(vertex.pos, 1.0f), model);
    result.pos = mul(world, vp);
    result.worldPos = world;

    result.tang = mul(float4(vertex.tang, 0.0f), normalModel).xyz;
    result.norm = mul(float4(vertex.norm, 0.0f), normalModel).xyz;
    result.uv = vertex.uv;

    return result;
}
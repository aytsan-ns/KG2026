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
    float3 pos : POSITION;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float3 localPos : POSITION1;
};

VSOutput vs(VSInput vertex)
{
    VSOutput result;

    float3 pos = cameraPos.xyz + vertex.pos * size.x;
    result.pos = mul(mul(float4(pos, 1.0f), model), vp);
    result.pos.z = 0.0f;
    result.localPos = vertex.pos;

    return result;
}
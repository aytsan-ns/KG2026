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
    result.pos.z = 0.0f; // reversed depth: skybox всегда на максимальной глубине
    result.localPos = vertex.pos;

    return result;
}
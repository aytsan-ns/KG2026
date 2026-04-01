struct Light
{
    float4 pos;
    float4 color;
};

cbuffer SceneBuffer : register(b0)
{
    row_major float4x4 vp;
    float4 cameraPos;
    int4 lightCount;
    Light lights[10];
    float4 ambientColor;
};

struct GeomBuffer
{
    row_major float4x4 model;
    row_major float4x4 normalModel;
    float4 shineSpeedTexIdNM; // x - shininess, y - speed, z - texture id, w - normal map flag
    float4 posAngle;          // xyz - position, w - angle
};

cbuffer GeomBufferInst : register(b1)
{
    GeomBuffer geomBuffer[100];
};

cbuffer GeomBufferInstVis : register(b2)
{
    uint4 ids[100];
};

struct VSInput
{
    float3 pos       : POSITION;
    float3 tang      : TANGENT;
    float3 norm      : NORMAL;
    float2 uv        : TEXCOORD;
    uint   instanceId : SV_InstanceID;
};

struct VSOutput
{
    float4 pos       : SV_Position;
    float4 worldPos  : POSITION;
    float3 tang      : TANGENT;
    float3 norm      : NORMAL;
    float2 uv        : TEXCOORD;
    nointerpolation uint instanceId : INST_ID;
};

VSOutput vs(VSInput vertex)
{
    VSOutput result;

    uint idx = ids[vertex.instanceId].x;

    float4 world = mul(float4(vertex.pos, 1.0f), geomBuffer[idx].model);
    result.pos = mul(world, vp);
    result.worldPos = world;

    result.tang = mul(float4(vertex.tang, 0.0f), geomBuffer[idx].normalModel).xyz;
    result.norm = mul(float4(vertex.norm, 0.0f), geomBuffer[idx].normalModel).xyz;
    result.uv = vertex.uv;
    result.instanceId = vertex.instanceId;

    return result;
}
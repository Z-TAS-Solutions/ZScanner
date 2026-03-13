struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

VSOutput VSMain(uint vertexID : SV_VertexID)
{
    VSOutput output;

    float2 pos[4] = { float2(-1,  1), float2(1,  1), float2(-1, -1), float2(1, -1) };
    float2 uv[4] = { float2(0,0), float2(1,0), float2(0,1), float2(1,1) };

    output.position = float4(pos[vertexID], 0, 1);
    output.texcoord = uv[vertexID];

    return output;
}
Texture2D<float> tex : register(t0);
SamplerState samp : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

float4 PSMain(PSInput input) : SV_Target
{
    float gray = tex.Sample(samp, input.texcoord).r;
    return float4(gray, gray, gray, 1.0f);
}
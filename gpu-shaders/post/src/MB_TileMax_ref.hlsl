// Reconstruction of the shipped tile-max pass f11afe97 (reference only).
#include "MB_common.hlsli"
Texture2D g_t0MainMap : register(t0);
float4 main(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
    float2 sz;
    g_t0MainMap.GetDimensions(sz.x, sz.y);
    float2 texel = 1.0 / sz;
    float2 base = texel * g_cdTileParam0.xy + uv;
    float2 best = 0;
    for (int i = 0; (float)i < g_cdTileParam0.z; i++)
    {
        float2 p = base + float2(texel.x * i, 0);
        for (int j = 0; (float)j < g_cdTileParam0.z; j++)
        {
            float2 q = float2(0, texel.y) * j + p;
            float2 v = g_t0MainMap.SampleLevel(samplePoint, q, 0).xy;
            if (dot(best, best) < dot(v, v)) best = v;
        }
    }
    return float4(best, 0, 0);
}

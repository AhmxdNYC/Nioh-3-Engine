// Motion blur tile max (replaces runtime PS f11afe97). Same function as the shipped shader, bit for bit:
// each column of the tile is walked in batches of 8 rows whose 8 point samples are issued together, then compared
// in the original order with the original strict '<' (first maximum wins). A row past the end of the column
// (original loop already exited) samples the batch's first row again (L0 hit) and is never selected.
#include "MB_common.hlsli"
Texture2D g_t0MainMap : register(t0);
#define B 8
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
        for (int j = 0; (float)j < g_cdTileParam0.z; j += B)
        {
            float2 v[B];
            bool ok[B];
            [unroll] for (int k = 0; k < B; k++)
            {
                ok[k] = (float)(j + k) < g_cdTileParam0.z;
                int jj = ok[k] ? j + k : j;
                float2 q = float2(0, texel.y) * jj + p;
                v[k] = g_t0MainMap.SampleLevel(samplePoint, q, 0).xy;
            }
            [unroll] for (int k2 = 0; k2 < B; k2++)
            {
                if (ok[k2] && dot(best, best) < dot(v[k2], v[k2])) best = v[k2];
            }
        }
    }
    return float4(best, 0, 0);
}

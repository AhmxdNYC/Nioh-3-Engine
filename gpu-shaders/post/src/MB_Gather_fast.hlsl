// Motion blur gather (replaces runtime PS 4e13e236). Same function as the shipped shader, bit for bit.
// The shipped loop runs 2 iterations per step position t (+t, then -t) and waits for its 2 samples (colour +
// velocity) every iteration. Here one loop pass runs the 4 iterations of t and the next t: all 8 samples are issued
// first, then the 4 iterations' ALU runs in the original order with the original ops. When the original stops after
// the pair at t, the pass issues only that pair's 4 samples (no extra fetches).
#include "MB_common.hlsli"
Texture2D g_t0MainMap : register(t0);
Texture2D g_t3NeighborTileMap : register(t3);
Texture2D g_t4VelocityInfoMap : register(t4);

float Ign(float2 p)
{
    p = floor(p * g_cbScreenInfo.xy);
    return frac(52.982918 * frac(dot(float2(asfloat(0x3d897143), asfloat(0x3bbf4590)), p)));
}

float4 main(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
    float4 center = g_t0MainMap.Load(int3(int2(pos.xy), 0));
    float T = g_cbTimeInfo.y;
    float2 a = uv + float2(T, 0);
    a = a + float2(2, T);
    float ang = Ign(a) * asfloat(0x40c90fdb);
    float2 sc;
    sincos(ang, sc.y, sc.x);
    float2 nmuv = sc * g_cdTileInfo.zw * 0.25 + uv;
    float2 nm = g_t3NeighborTileMap.SampleLevel(samplePoint, nmuv, 0).xy;
    float len = sqrt(dot(nm, nm));
    if (len < 2.0) return center;
    float3 vi = g_t4VelocityInfoMap.SampleLevel(sampleLinear, uv, 0).xyz;
    float2 v = (vi.xy * 2 - 1) * g_cbMbParams0.y;
    float vlen = sqrt(dot(v, v));
    float2 m = max(vlen, float2(0.5, 1.0));
    float invz = 1.0 / vi.z;
    float2 dirC = (len < m.x + m.x) ? v * (len / m.x) : nm;
    float N = floor(min(len * 0.5, g_cbMbParams0.w));
    float invN = 1.0 / N;
    float jit = Ign(uv + T) - 0.5;
    float q = invN * 0.25;
    float t = -invN * 0.5 + 1;
    float maxv = m.y;
    float4 acc = 0;
    float i = 0;
    [loop] while (true)
    {
        if (q >= t) break;
        float t2 = t - invN;
        bool more = !(q >= t2);           // the original also runs the next pair (t2)
        int np = more ? 4 : 2;            // iterations taken in this pass
        float i1 = i + 1, i2 = i1 + 1, i3 = i2 + 1;
        float4 ii = float4(i, i1, i2, i3);
        float4 tt = float4(t, t, t2, t2);
        float2 suv[4];
        float4 sv;
        [unroll] for (int k = 0; k < 4; k++)
        {
            bool2 sel = 0.499 < frac(ii[k] * float2(0.25, 0.5));
            float2 d = sel.x ? dirC : nm;
            float s = sel.y ? -tt[k] : tt[k];
            s = jit * invN + s;
            sv[k] = s;
            suv[k] = (s * d) * g_cbScreenInfo.zw + uv;
        }
        float3 c[4], vs[4];
        if (more)
        {
            [unroll] for (int k1 = 0; k1 < 4; k1++)
            {
                c[k1] = g_t0MainMap.SampleLevel(sampleLinear, suv[k1], 0).xyz;
                vs[k1] = g_t4VelocityInfoMap.SampleLevel(sampleLinear, suv[k1], 0).xyz;
            }
        }
        else
        {
            [unroll] for (int k1 = 0; k1 < 2; k1++)
            {
                c[k1] = g_t0MainMap.SampleLevel(sampleLinear, suv[k1], 0).xyz;
                vs[k1] = g_t4VelocityInfoMap.SampleLevel(sampleLinear, suv[k1], 0).xyz;
            }
            c[2] = c[3] = 0;
            vs[2] = vs[3] = 0;
        }
        [unroll] for (int k2 = 0; k2 < 4; k2++)
        {
            if (k2 < np)
            {
                float2 vv = (vs[k2].xy * 2 - 1) * g_cbMbParams0.y;
                float dz = saturate(((vi.z - vs[k2].z) * invz) * 20);
                float l = sqrt(dot(vv, vv));
                l = dz * (l - maxv) + maxv;
                l = max(l, 0.0001);
                float w = saturate(-len * abs(sv[k2]) + l) / l;
                w = w * (1.2 - tt[k2]);
                acc = float4(c[k2], 1) * w + acc;
                maxv = max(maxv, l);
            }
        }
        i = more ? i3 + 1 : i2;
        t = more ? t2 - invN : t2;
        if (!more) break;
    }
    float k = 1.2 / dot(maxv.xx, N.xx);
    float4 r = float4(center.xyz, 1) * k + acc;
    return float4(r.xyz / r.w, center.w);
}

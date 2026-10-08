// Reconstruction of the shipped motion-blur gather 4e13e236 (reference only).
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
        bool2 sel = 0.499 < frac(i * float2(0.25, 0.5));
        float2 d = sel.x ? dirC : nm;
        float s = sel.y ? -t : t;
        s = jit * invN + s;
        float2 suv = (s * d) * g_cbScreenInfo.zw + uv;
        float3 c = g_t0MainMap.SampleLevel(sampleLinear, suv, 0).xyz;
        float3 vs = g_t4VelocityInfoMap.SampleLevel(sampleLinear, suv, 0).xyz;
        float2 vv = (vs.xy * 2 - 1) * g_cbMbParams0.y;
        float dz = saturate(((vi.z - vs.z) * invz) * 20);
        float l = sqrt(dot(vv, vv));
        l = dz * (l - maxv) + maxv;
        l = max(l, 0.0001);
        float w = saturate(-len * abs(s) + l) / l;
        w = w * (1.2 - t);
        acc = float4(c, 1) * w + acc;
        maxv = max(maxv, l);
        t = sel.y ? t - invN : t;
        i += 1;
    }
    float k = 1.2 / dot(maxv.xx, N.xx);
    float4 r = float4(center.xyz, 1) * k + acc;
    return float4(r.xyz / r.w, center.w);
}

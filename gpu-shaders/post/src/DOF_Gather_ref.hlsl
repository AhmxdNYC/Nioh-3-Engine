// Reconstruction of the shipped DOF gather/composite cf792d56 (reference only).
#include "DOF_common.hlsli"
float4 main(float4 pos : SV_Position, float2 uv0 : TEXCOORD0, float2 uv : TEXCOORD1) : SV_Target
{
    int3 p = int3(int2(pos.xy), 0);
    float4 fore = min(g_tSpritesDofForeAccum.Load(p).wxyz, 65024.0);
    int R = (int)g_vDofInfo1.y;
    int n = (int)g_vSampleInfo.x;
    int c = (int)((uint)(n - 1) >> 1);
    int lo = max(c - R, 0);
    int hi = min(n, c + R + 1);
    float twoS = g_vSampleInfo.y + g_vSampleInfo.y;
    float cocC = g_tDofCocMap.Load(p).x;
    float cc = min(max(cocC, -g_vDofInfo1.x), g_vDofInfo1.x);
    float4 A = 0, Bc = 0;
    for (int y = lo; y < hi; y++)
    {
        for (int x = lo; x < hi; x++)
        {
            float3 off = g_tSampleInfo.Load(int3(x, y, 0)).xyz;
            float2 o2 = off.xy * g_vDofInfo1.z;
            float2 suv = float2(o2.x * g_vDofInfo1.w, o2.y) + uv;
            float3 col = g_t4MainMap.SampleLevel(sampleLinear, suv, 0).xyz;
            float coc = g_tDofCocMap.SampleLevel(sampleLinear, suv, 0).x;
            coc = min(max(coc, -g_vDofInfo1.x), g_vDofInfo1.x);
            float a = max(min(cc, coc), 0);
            a = -off.z * g_vDofInfo1.z + a;
            a = g_vSampleInfo.y * 2 + a;
            float b = -off.z * g_vDofInfo1.z + -coc;
            b = g_vSampleInfo.y * 2 + b;
            a = saturate(a / twoS);
            b = saturate(b / twoS);
            b = (-coc >= g_vSampleInfo.y ? 1.0 : 0.0) * b;
            if (0 < a || 0 < b)
            {
                A = float4(col, 1) * a + A;
                Bc = float4(col, 1) * b + Bc;
            }
        }
    }
    float3 Arg = A.xyz / (0 < A.w ? A.w : 1.0);
    float3 Brg = Bc.xyz / (0 < Bc.w ? Bc.w : 1.0);
    float f = saturate((cc - g_vSampleInfo.y) * (1.0 / g_vSampleInfo.y));
    f = f * f * (f * -2 + 3);
    float cnt = (float)(hi - lo);
    float cov = saturate(asfloat(0x40490fdb) / (cnt * cnt) * Bc.w);
    if (g_vDofInfo2.y < cocC)
    {
        float4 rear = min(g_tSpritesDofRearAccum.Load(p).wxyz, 65024.0);
        float3 rrgb = 0 < rear.x ? rear.yzw / rear.x : 0;
        Arg = saturate(rear.x) * (rrgb - Arg) + Arg;
    }
    float f2 = min(f, 1.0);
    float3 r = f2 * Arg;
    r = cov * (-Arg * f2 + Brg) + r;
    float wv = (1 - f2) * (1 - cov);
    float fa = fore.x;
    float3 o;
    if (0 < fa)
    {
        float3 frgb = fore.yzw / fa;
        fa = saturate(fa);
        o = fa * (frgb - r) + r;
    }
    else
        o = r;
    return float4(o, min(1 - fa, wv));
}

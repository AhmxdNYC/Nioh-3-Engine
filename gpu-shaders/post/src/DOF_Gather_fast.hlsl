// DOF gather + composite (replaces runtime PS cf792d56). Same function as the shipped shader, bit for bit.
// The shipped kernel walks the (hi-lo)^2 taps row by row and per tap waits twice: for the offset-table load, then
// for the colour + CoC samples that use it. Here the taps are taken 4 at a time in the same row-major order:
// 4 table loads, then 8 samples, then the 4 taps' ALU in the original order and ops. The last 1-3 taps run one
// per pass exactly as shipped (no extra fetches).
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
    int x = lo, y = lo;
    [loop] while (y < hi)
    {
        int2 tp[4];
        bool ok3 = true;
        int x1 = x, y1 = y;   // position after one tap
        [unroll] for (int k = 0; k < 4; k++)
        {
            ok3 = y < hi;
            tp[k] = int2(x, y);
            x = x + 1;
            bool wrap = x >= hi;
            x = wrap ? lo : x;
            y = wrap ? y + 1 : y;
            if (k == 0)
            {
                x1 = x;
                y1 = y;
            }
        }
        float3 off[4], col[4];
        float cs[4];
        if (ok3)   // 4 taps left: 4 table loads, then 8 samples, then the 4 taps in order
        {
            [unroll] for (int k1 = 0; k1 < 4; k1++)
                off[k1] = g_tSampleInfo.Load(int3(tp[k1], 0)).xyz;
            [unroll] for (int k2 = 0; k2 < 4; k2++)
            {
                float2 o2 = off[k2].xy * g_vDofInfo1.z;
                float2 suv = float2(o2.x * g_vDofInfo1.w, o2.y) + uv;
                col[k2] = g_t4MainMap.SampleLevel(sampleLinear, suv, 0).xyz;
                cs[k2] = g_tDofCocMap.SampleLevel(sampleLinear, suv, 0).x;
            }
            [unroll] for (int K = 0; K < 4; K++)
            {
                float coc = min(max(cs[K], -g_vDofInfo1.x), g_vDofInfo1.x);
                float a = max(min(cc, coc), 0);
                a = -off[K].z * g_vDofInfo1.z + a;
                a = g_vSampleInfo.y * 2 + a;
                float b = -off[K].z * g_vDofInfo1.z + -coc;
                b = g_vSampleInfo.y * 2 + b;
                a = saturate(a / twoS);
                b = saturate(b / twoS);
                b = (-coc >= g_vSampleInfo.y ? 1.0 : 0.0) * b;
                if (0 < a || 0 < b)
                {
                    A = float4(col[K], 1) * a + A;
                    Bc = float4(col[K], 1) * b + Bc;
                }
            }
        }
        else       // last 1-3 taps: one per pass, as shipped
        {
            const int K = 0;
            off[0] = g_tSampleInfo.Load(int3(tp[0], 0)).xyz;
            float2 o2 = off[0].xy * g_vDofInfo1.z;
            float2 suv = float2(o2.x * g_vDofInfo1.w, o2.y) + uv;
            col[0] = g_t4MainMap.SampleLevel(sampleLinear, suv, 0).xyz;
            cs[0] = g_tDofCocMap.SampleLevel(sampleLinear, suv, 0).x;
            float coc = min(max(cs[K], -g_vDofInfo1.x), g_vDofInfo1.x);
            float a = max(min(cc, coc), 0);
            a = -off[K].z * g_vDofInfo1.z + a;
            a = g_vSampleInfo.y * 2 + a;
            float b = -off[K].z * g_vDofInfo1.z + -coc;
            b = g_vSampleInfo.y * 2 + b;
            a = saturate(a / twoS);
            b = saturate(b / twoS);
            b = (-coc >= g_vSampleInfo.y ? 1.0 : 0.0) * b;
            if (0 < a || 0 < b)
            {
                A = float4(col[K], 1) * a + A;
                Bc = float4(col[K], 1) * b + Bc;
            }
            x = x1;
            y = y1;
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

// Composite (replaces runtime PS e6efc8ff): mechanical translation of the shipped DXBC (dxbc2hlsl.py)
// with only the chromatic-aberration loop rewritten ([EDITED], built by mkcompose.py).

cbuffer cbComposite : register(b2)
{
    float4 g_vSceneTexSize;
    float4 g_vCompositeInfo;
    float4 g_vSun2dInfo;
    float4 g_vEtcEffect;
    float4 g_vBloomInfo;
    float4 g_vLimbDarkenningInfo;
    float4 g_vFxaaParams;
    float4 g_vGammaCorrection;
    float4 g_vRadialBlurCenter;
    float4 g_vRadialBlurInfo;
    float4 g_vFxaaQualityParams;
    float4 g_vCompositeLastViewport;
    float4 g_vMaxUV;
    float4 g_vMinUV;
    float4 g_vP2V;
    float4 g_mV2W[4];
    float4 g_vDramaticHdrLutInfo0[2];
    float4 g_vDramaticHdrLutInfo1[2];
    float4 g_vDrawFixParams;
    float4 g_vDistortionParams;
    float4 g_vVerticalLimbDarkenningTopInfo;
    float4 g_vVerticalLimbDarkenningBottomInfo;
};
SamplerState sampleLinear : register(s7);
Texture2D<float4> g_tSceneMap : register(t0);
Texture2D<float4> g_tLensFlareMap : register(t1);
Texture2D<float4> g_tExposureScaleInfo : register(t2);
Texture3D<float4> g_tHdrLut : register(t3);
Texture3D<float4> g_tLdrLut : register(t4);
static const uint4 icb[12] = {
    {0x00000000, 0x00000000, 0x3f800000, 0x00000000},
    {0x00000000, 0x3f800000, 0x00000000, 0x00000000},
    {0x3f800000, 0x00000000, 0x00000000, 0x00000000},
    {0x3f800000, 0x00000000, 0x00000000, 0x00000000},
    {0x00000000, 0x3f800000, 0x00000000, 0x00000000},
    {0x00000000, 0x00000000, 0x3f800000, 0x00000000},
    {0x00000000, 0x3f800000, 0x00000000, 0x00000000},
    {0x3f800000, 0x00000000, 0x00000000, 0x00000000},
    {0x3f800000, 0x00000000, 0x3f800000, 0x00000000},
    {0x3f800000, 0x00000000, 0x3f800000, 0x00000000},
    {0x3f800000, 0x00000000, 0x00000000, 0x00000000},
    {0x00000000, 0x3f800000, 0x00000000, 0x00000000}
};
uint2 movc2_(uint2 c, uint2 a, uint2 b) { return uint2(c.x ? a.x : b.x, c.y ? a.y : b.y); }
uint3 movc3(uint3 c, uint3 a, uint3 b) { return uint3(c.x ? a.x : b.x, c.y ? a.y : b.y, c.z ? a.z : b.z); }
uint4 movc4(uint4 c, uint4 a, uint4 b) { return uint4(c.x ? a.x : b.x, c.y ? a.y : b.y, c.z ? a.z : b.z, c.w ? a.w : b.w); }
uint2 select4(bool2 c, int n) { return c ? 0xffffffffu : 0u; }
uint3 select4(bool3 c, int n) { return c ? 0xffffffffu : 0u; }
uint4 select4(bool4 c, int n) { return c ? 0xffffffffu : 0u; }
float4 main(float4 in_sv_position0 : SV_Position, float2 in_texcoord0 : TEXCOORD0) : SV_Target
{
    uint4 v0 = asuint(float4(in_sv_position0.x, in_sv_position0.y, in_sv_position0.z, in_sv_position0.w));
    uint4 v1 = asuint(float4(in_texcoord0.x, in_texcoord0.y, 0, 0));
    uint4 r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0, r6 = 0, r7 = 0, r8 = 0, r9 = 0, r10 = 0, r11 = 0, r12 = 0;
    uint4 o0 = 0;
    r0.x = (asfloat(0x00000000) < asfloat(asuint(g_vSun2dInfo).z)) ? 0xffffffffu : 0u;
    r0.y = (asfloat(0x00000000) < asfloat(asuint(g_vLimbDarkenningInfo).w)) ? 0xffffffffu : 0u;
    r0.zw = select4(float2(asfloat(0x00000000), asfloat(0x00000000)) < asfloat(asuint(g_vDrawFixParams).xy), 2);
    r1.x = (asfloat(0x00000000) < asfloat(asuint(g_vVerticalLimbDarkenningTopInfo).x)) ? 0xffffffffu : 0u;
    r1.y = (asfloat(0x00000000) < asfloat(asuint(g_vVerticalLimbDarkenningBottomInfo).x)) ? 0xffffffffu : 0u;
    r1.z = (asfloat(asuint(g_vCompositeInfo).z) < asfloat(0x00000000)) ? 0xffffffffu : 0u;
    r1.w = asuint((g_tExposureScaleInfo.Load(int3(int3(0, 0, 0)))).x);
    r2.xy = select4(float2(asfloat(0x00000000), asfloat(0x00000000)) < asfloat(asuint(g_vCompositeInfo).zy), 2);
    r2.x = (r2.x != 0u) ? asuint(g_vCompositeInfo).z : 0x3f800000u;
    r1.z = (r1.z != 0u) ? r1.w : r2.x;
    r2.xz = asuint(mad(asfloat(v1.xy), asfloat(asuint(g_vCompositeLastViewport).zw), asfloat(asuint(g_vCompositeLastViewport).xy)));
    r3.xy = select4(asfloat(asuint(g_vDistortionParams).xy) != float2(asfloat(0x00000000), asfloat(0x00000000)), 2);
    r1.w = (r3.y | r3.x);
    r3.yz = asuint((asfloat(r2.xz) + float2(asfloat(0xbf000000), asfloat(0xbf000000))));
    r3.x = asuint((asfloat(r3.y) * asfloat(asuint(g_vCompositeInfo).x)));
    r2.w = asuint((dot(asfloat(r3.xz), asfloat(r3.xz))));
    r3.y = asuint((asfloat(r2.w) * asfloat(r2.w)));
    r2.w = asuint(mad(asfloat(asuint(g_vDistortionParams).x), asfloat(r2.w), asfloat(0x3f800000)));
    r2.w = asuint(mad(asfloat(asuint(g_vDistortionParams).y), asfloat(r3.y), asfloat(r2.w)));
    r3.yz = asuint((asfloat(r3.xz) * asfloat(r2.ww)));
    r3.x = asuint((asfloat(r3.y) / asfloat(asuint(g_vCompositeInfo).x)));
    r3.xy = asuint(mad(asfloat(r3.xz), asfloat(asuint(g_vDistortionParams).zz), float2(asfloat(0x3f000000), asfloat(0x3f000000))));
    r2.xz = movc2_(r1.ww, r3.xy, r2.xz);
    r3.xyz = asuint((g_tSceneMap.SampleLevel(sampleLinear, asfloat(r2.xz), asfloat(0x00000000))).xyz);
    r3.xyz = asuint(min(asfloat(r3.xyz), float3(asfloat(0x477e0000), asfloat(0x477e0000), asfloat(0x477e0000))));
    r1.w = (asfloat(0x00000000) < asfloat(asuint(g_vEtcEffect).x)) ? 0xffffffffu : 0u;
    if (r1.w != 0u)
    {
        r1.w = (uint)asfloat(asuint(g_vEtcEffect).y);
        r4.xy = asuint(mad(asfloat(r2.xz), float2(asfloat(0x40000000), asfloat(0x40000000)), float2(asfloat(0xbf800000), asfloat(0xbf800000))));
        r2.w = asuint((dot(asfloat(r4.xy), asfloat(r4.xy))));
        r4.xy = asuint((asfloat(r2.ww) * asfloat(r4.xy)));
        r4.xy = asuint((asfloat(r4.xy) * asfloat(asuint(g_vEtcEffect).xx)));
        r4.zw = asuint(((-asfloat(r4.xy)) * asfloat(asuint(g_vSceneTexSize).xy)));
        r4.zw = asuint((asfloat(r4.zw) * float2(asfloat(0x3f000000), asfloat(0x3f000000))));
        r2.w = asuint((dot(asfloat(r4.zw), asfloat(r4.zw))));
        r2.w = asuint(sqrt(asfloat(r2.w)));
        r2.w = asuint((int)asfloat(r2.w));
        r2.w = asuint(max(asint(r2.w), 3));
        r2.w = asuint(min(asint(r2.w), 16));
        r3.w = asuint((float)asint(r2.w));
        r4.xy = asuint(((-asfloat(r4.xy)) / asfloat(r3.ww)));
        r5.xyz = asuint((asfloat(r3.xyz) * asfloat(icb[asint(r1.w) + 0].xyz)));
        r4.zw = asuint((asint(r1.ww) + int2(1, 2)));
        r6.xyz = asuint(((-asfloat(icb[asint(r1.w) + 0].xyz)) + asfloat(icb[asint(r4.z) + 0].xyz)));
        r7.xyz = asuint(((-asfloat(icb[asint(r4.z) + 0].xyz)) + asfloat(icb[asint(r4.w) + 0].xyz)));
        r8.xyz = r5.xyz;
        r9.xyz = icb[asint(r1.w) + 0].xyz;
        r10.xy = r2.xz;
        r4.w = 0x00000001u;
        [loop] while (true)
        {
            // [EDITED] chromatic-aberration loop: 4 taps per pass while 4 remain (see mkcompose.py)
            int i0 = asint(r4.w);
            if (i0 >= asint(r2.w)) break;
            int nt = (i0 + 3 < asint(r2.w)) ? 4 : 1;
            float2 pp[4];
            float2 pos = asfloat(r10.xy);
            [unroll] for (int k = 0; k < 4; k++)
            {
                pos = asfloat(r4.xy) + pos;
                pp[k] = pos;
            }
            float3 cs[4];
            if (nt == 4)
            {
                [unroll] for (int k1 = 0; k1 < 4; k1++)
                    cs[k1] = g_tSceneMap.SampleLevel(sampleLinear, pp[k1], asfloat(0x00000000)).xyz;
            }
            else
            {
                cs[0] = g_tSceneMap.SampleLevel(sampleLinear, pp[0], asfloat(0x00000000)).xyz;
                cs[1] = cs[2] = cs[3] = 0;
            }
            float3 acc = asfloat(r8.xyz), wsum = asfloat(r9.xyz);
            [unroll] for (int k2 = 0; k2 < 4; k2++)
            {
                if (k2 < nt)
                {
                    float3 c = min(cs[k2], float3(asfloat(0x477e0000), asfloat(0x477e0000), asfloat(0x477e0000)));
                    float f = (float)(i0 + k2);
                    f = f / asfloat(r3.w);
                    float3 w;
                    if (f < asfloat(0x3f000000))
                        w = mad((f + f).xxx, asfloat(r6.xyz), asfloat(icb[asint(r1.w) + 0].xyz));
                    else
                        w = mad(mad(f, asfloat(0x40000000), asfloat(0xbf800000)).xxx, asfloat(r7.xyz), asfloat(icb[asint(r4.z) + 0].xyz));
                    acc = mad(c, w, acc);
                    wsum = wsum + w;
                }
            }
            r8.xyz = asuint(acc);
            r9.xyz = asuint(wsum);
            r10.xy = asuint(nt == 4 ? pp[3] : pp[0]);
            r4.w = asuint(i0 + nt);
        }
        r3.xyz = asuint((asfloat(r8.xyz) / asfloat(r9.xyz)));
    }
    r3.xyz = asuint((asfloat(r1.zzz) * asfloat(r3.xyz)));
    if (r0.z != 0u)
    {
        r0.z = asuint((dot(asfloat(v0.xy), float2(asfloat(0x432b0000), asfloat(0x43670000)))));
        r4.xyz = asuint((asfloat(r0.zzz) * float3(asfloat(0x3c1f1166), asfloat(0x3c66c2b4), asfloat(0x3c28e83f))));
        r4.xyz = asuint(frac(asfloat(r4.xyz)));
        r4.xyz = asuint(mad(asfloat(r4.xyz), float3(asfloat(0x3b808081), asfloat(0x3b808081), asfloat(0x3b808081)), asfloat(r3.xyz)));
        r4.xyz = asuint((asfloat(r4.xyz) + float3(asfloat(0xbb008081), asfloat(0xbb008081), asfloat(0xbb008081))));
        r3.xyz = asuint(max(asfloat(r4.xyz), float3(asfloat(0x00000000), asfloat(0x00000000), asfloat(0x00000000))));
    }
    if (r0.x != 0u)
    {
        r4.xyz = asuint((g_tSceneMap.SampleLevel(sampleLinear, asfloat(asuint(g_vSun2dInfo).xy), asfloat(0x00000000))).xyz);
        r4.xyz = asuint(min(asfloat(r4.xyz), float3(asfloat(0x477e0000), asfloat(0x477e0000), asfloat(0x477e0000))));
        r4.xyz = asuint((asfloat(r1.zzz) * asfloat(r4.xyz)));
        r5.xyz = asuint((g_tLensFlareMap.SampleLevel(sampleLinear, asfloat(r2.xz), asfloat(0x00000000))).xyz);
        r5.xyz = asuint(min(asfloat(r5.xyz), float3(asfloat(0x477e0000), asfloat(0x477e0000), asfloat(0x477e0000))));
        r0.x = asuint((dot(asfloat(r4.xyz), float3(asfloat(0x3e6357e6), asfloat(0x3f34e758), asfloat(0x3d921577)))));
        r0.x = (asfloat(asuint(g_vEtcEffect).w) < asfloat(r0.x)) ? 0xffffffffu : 0u;
        r0.x = (r0.x & asuint(g_vEtcEffect).z);
        r4.xyz = asuint((asfloat(r4.xyz) * asfloat(r5.xyz)));
        r3.xyz = asuint(mad(asfloat(r4.xyz), asfloat(r0.xxx), asfloat(r3.xyz)));
    }
    if (r0.y != 0u)
    {
        r0.xz = asuint((asfloat(v1.xy) + float2(asfloat(0xbf000000), asfloat(0xbf000000))));
        r0.y = asuint((asfloat(r0.x) * asfloat(asuint(g_vCompositeInfo).x)));
        r0.x = asuint((dot(asfloat(r0.yz), asfloat(r0.yz))));
        r0.y = asuint(sqrt(asfloat(r0.x)));
        r0.y = asuint((asfloat(r0.y) + (-asfloat(asuint(g_vLimbDarkenningInfo).y))));
        r0.z = (asfloat(0x00000000) < asfloat(r0.y)) ? 0xffffffffu : 0u;
        r0.y = asuint(saturate(mad((-asfloat(r0.y)), asfloat(asuint(g_vLimbDarkenningInfo).z), asfloat(0x3f800000))));
        r0.y = (r0.z != 0u) ? r0.y : 0x3f800000u;
        r0.z = (asfloat(0x00000000) < asfloat(r0.y)) ? 0xffffffffu : 0u;
        r0.x = asuint((asfloat(r0.x) + asfloat(asuint(g_vLimbDarkenningInfo).x)));
        r0.x = asuint((asfloat(asuint(g_vLimbDarkenningInfo).x) / asfloat(r0.x)));
        r0.x = asuint((asfloat(r0.x) * asfloat(r0.x)));
        r0.x = (r0.z != 0u) ? r0.x : 0x3f800000u;
        r0.x = asuint((asfloat(r0.y) * asfloat(r0.x)));
        r0.y = asuint(((-asfloat(asuint(g_vLimbDarkenningInfo).w)) + asfloat(0x3f800000)));
        r0.x = asuint(mad(asfloat(r0.x), asfloat(asuint(g_vLimbDarkenningInfo).w), asfloat(r0.y)));
        r3.xyz = asuint((asfloat(r0.xxx) * asfloat(r3.xyz)));
    }
    if (r1.x != 0u)
    {
        r0.x = asuint(((-asfloat(r2.z)) + asfloat(0x3f800000)));
        r0.x = asuint((asfloat(r0.x) + (-asfloat(asuint(g_vVerticalLimbDarkenningTopInfo).y))));
        r0.x = asuint((asfloat(r0.x) * asfloat(asuint(g_vVerticalLimbDarkenningTopInfo).z)));
        r0.y = (asfloat(r0.x) >= asfloat(0x3f800000)) ? 0xffffffffu : 0u;
        if (r0.y != 0u)
        {
            r0.y = asuint(((-asfloat(asuint(g_vVerticalLimbDarkenningTopInfo).x)) + asfloat(0x3f800000)));
        }
        else
        {
            r0.z = (asfloat(0x00000000) < asfloat(r0.x)) ? 0xffffffffu : 0u;
            r0.x = asuint(max(asfloat(r0.x), asfloat(0x00000000)));
            r0.x = asuint(log2(asfloat(r0.x)));
            r0.x = asuint((asfloat(r0.x) * asfloat(asuint(g_vVerticalLimbDarkenningTopInfo).w)));
            r0.x = asuint(exp2(asfloat(r0.x)));
            r0.x = asuint(((-asfloat(r0.x)) + asfloat(0x3f800000)));
            r0.x = asuint((asfloat(r0.x) * asfloat(r0.x)));
            r1.x = asuint(((-asfloat(asuint(g_vVerticalLimbDarkenningTopInfo).x)) + asfloat(0x3f800000)));
            r0.x = asuint(mad(asfloat(r0.x), asfloat(asuint(g_vVerticalLimbDarkenningTopInfo).x), asfloat(r1.x)));
            r0.y = (r0.z != 0u) ? r0.x : 0x3f800000u;
        }
        r3.xyz = asuint((asfloat(r0.yyy) * asfloat(r3.xyz)));
    }
    if (r1.y != 0u)
    {
        r0.x = asuint((asfloat(r2.z) + (-asfloat(asuint(g_vVerticalLimbDarkenningBottomInfo).y))));
        r0.x = asuint((asfloat(r0.x) * asfloat(asuint(g_vVerticalLimbDarkenningBottomInfo).z)));
        r0.y = (asfloat(r0.x) >= asfloat(0x3f800000)) ? 0xffffffffu : 0u;
        if (r0.y != 0u)
        {
            r0.y = asuint(((-asfloat(asuint(g_vVerticalLimbDarkenningBottomInfo).x)) + asfloat(0x3f800000)));
        }
        else
        {
            r0.z = (asfloat(0x00000000) < asfloat(r0.x)) ? 0xffffffffu : 0u;
            r0.x = asuint(max(asfloat(r0.x), asfloat(0x00000000)));
            r0.x = asuint(log2(asfloat(r0.x)));
            r0.x = asuint((asfloat(r0.x) * asfloat(asuint(g_vVerticalLimbDarkenningBottomInfo).w)));
            r0.x = asuint(exp2(asfloat(r0.x)));
            r0.x = asuint(((-asfloat(r0.x)) + asfloat(0x3f800000)));
            r0.x = asuint((asfloat(r0.x) * asfloat(r0.x)));
            r1.x = asuint(((-asfloat(asuint(g_vVerticalLimbDarkenningBottomInfo).x)) + asfloat(0x3f800000)));
            r0.x = asuint(mad(asfloat(r0.x), asfloat(asuint(g_vVerticalLimbDarkenningBottomInfo).x), asfloat(r1.x)));
            r0.y = (r0.z != 0u) ? r0.x : 0x3f800000u;
        }
        r3.xyz = asuint((asfloat(r0.yyy) * asfloat(r3.xyz)));
    }
    r0.xyz = asuint(mad(asfloat(r3.xyz), float3(asfloat(0x3f80022e), asfloat(0x3f80022e), asfloat(0x3f80022e)), float3(asfloat(0xbb8055ae), asfloat(0xbb8055ae), asfloat(0xbb8055ae))));
    r0.xyz = movc3(r0.www, r0.xyz, r3.xyz);
    r0.xyz = asuint(mad(asfloat(r0.xyz), float3(asfloat(0x40b1c71d), asfloat(0x40b1c71d), asfloat(0x40b1c71d)), float3(asfloat(0x3d449774), asfloat(0x3d449774), asfloat(0x3d449774))));
    r0.xyz = asuint(log2(asfloat(r0.xyz)));
    r0.xyz = asuint(saturate(mad(asfloat(r0.xyz), float3(asfloat(0x3d96870e), asfloat(0x3d96870e), asfloat(0x3d96870e)), float3(asfloat(0x3ec5a683), asfloat(0x3ec5a683), asfloat(0x3ec5a683)))));
    r0.xyz = asuint((g_tHdrLut.SampleLevel(sampleLinear, asfloat(r0.xyz), asfloat(0x00000000))).xyz);
    if (r2.y != 0u)
    {
        r1.xyz = asuint(saturate(asfloat(r0.xyz)));
        r1.xyz = asuint(log2(asfloat(r1.xyz)));
        r1.xyz = asuint((asfloat(r1.xyz) * float3(asfloat(0x3ee8ba2f), asfloat(0x3ee8ba2f), asfloat(0x3ee8ba2f))));
        r1.xyz = asuint(exp2(asfloat(r1.xyz)));
        r1.xyz = asuint((g_tLdrLut.SampleLevel(sampleLinear, asfloat(r1.xyz), asfloat(0x00000000))).xyz);
        r1.xyz = asuint(((-asfloat(r0.xyz)) + asfloat(r1.xyz)));
        r0.xyz = asuint(mad(asfloat(asuint(g_vCompositeInfo).yyy), asfloat(r1.xyz), asfloat(r0.xyz)));
    }
    r0.w = (asfloat(asuint(g_vGammaCorrection).x) != asfloat(0x3f800000)) ? 0xffffffffu : 0u;
    r1.xyz = asuint(log2(abs(asfloat(r0.xyz))));
    r1.xyz = asuint((asfloat(r1.xyz) * asfloat(asuint(g_vGammaCorrection).xxx)));
    r1.xyz = asuint(exp2(asfloat(r1.xyz)));
    r0.xyz = movc3(r0.www, r1.xyz, r0.xyz);
    o0.xyz = asuint((asfloat(r0.xyz) * asfloat(asuint(g_vRadialBlurCenter).zzz)));
    o0.w = 0x3f800000u;
    return asfloat(o0);
}

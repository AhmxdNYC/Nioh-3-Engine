// ESM_fast.hlsl - drop-in replacement for the ESM separable Gaussian blur pixel shaders in
// shader_dx12\0x1ca8bb39.file (group "ESM"): records #2..#6 = vertical, radius R = 1..5;
// #7..#11 = horizontal, R = 1..5. In the RGP trace (SHADOW HIGH): #9 = H 7-tap (PSO 8501ad7a, 57 us) and
// #4 = V 7-tap (PSO c570890a, 239 us), both on the 2048^2 directional ESM. That map has a fixed size, so
// on the ROG Ally this pass costs the same in absolute GPU work as on the PC regardless of render scale.
//
// Interface identical to the originals (verified by build_esm.py): ps_6_5, t0 / s0 (space 0), no cbuffer,
//   in  SV_Position (declared, unused) + TEXCOORD0.xy (linear), out SV_Target0 float4,
//   no wave ops (SFI0 = 0), reflection stripped like the shipped records.
//
// Original:  d = 1/dim  (dim = width for H, height for V, GetDimensions mip 0)
//            s(k) = tSrc.Sample(s0, tc +/- k*d), k = -R..R (implicit LOD)
//            out  = ((s(+1)+s(-1))*w1 + s(0)*w0) + (s(+2)+s(-2))*w2 + ... + (s(+R)+s(-R))*wR  (per channel)
//
// Fast path: with the normal same-size ping-pong the 2R+1 taps sit on texel centres, so each Sample returns
// one texel. A Gather placed on the corner to the right/below of our texel returns a 2x2 block whose
// column (V) / row (H) u0/v0 is ours; one gather therefore delivers 2 taps -> R+1 gathers instead of
// 2R+1 samples (7 -> 4 for R=3), and the driver drops the Green/Blue/Alpha gathers for a 1-channel target
// exactly as it drops the unused channels of the original samples. Same weights (bit patterns) and the
// same summation order. The result equals the original up to the texture unit's sub-texel weight rounding
// (the original's bilinear tap at a centre that interpolation put 1e-4 texel off can mix in <=1/256 of
// the neighbour; gather never does).
// Pixels whose texel mapping is not 1:1 within 1/1024 texel (different src/dst size, offset, mirrored)
// take the original 2R+1-tap path (SampleGrad with the same derivatives = the same LOD as Sample).
//
// Build: dxc -T ps_6_5 -E main -O3 -validator-version 1.5 -Qstrip_reflect -Qstrip_debug -D R=<1..5> -D VERT=<0|1>

#ifndef R
#define R 3
#endif
#ifndef VERT
#define VERT 1
#endif

Texture2D<float4> tSrc : register(t0);
SamplerState      sSrc : register(s0);

// Gaussian weights exactly as in the original DXIL (float bit patterns), W[0] = centre.
#if   R == 1
static const float W[2] = { asfloat(0x3eb78898u), asfloat(0x3ea43bb4u) };
#elif R == 2
static const float W[3] = { asfloat(0x3e6cea6cu), asfloat(0x3e5b5fe5u), asfloat(0x3e2e2ac3u) };
#elif R == 3
static const float W[4] = { asfloat(0x3e356bcau), asfloat(0x3e2b0ea2u), asfloat(0x3e0f6274u), asfloat(0x3dd5b24fu) };
#elif R == 4
static const float W[5] = { asfloat(0x3e16aa08u), asfloat(0x3e0fa872u), asfloat(0x3df91149u), asfloat(0x3dc44bf5u), asfloat(0x3d8ca70du) };
#elif R == 5
static const float W[6] = { asfloat(0x3e03165du), asfloat(0x3dfbe554u), asfloat(0x3ddf6901u), asfloat(0x3db6e9bcu), asfloat(0x3d8a3e7du), asfloat(0x3d40e5a3u) };
#endif

#define NG (R + 1)                  // gathers per pixel (2 taps each, last one has a spare)

// Original arithmetic order: acc = (s(+1)+s(-1))*w1 + s(0)*w0; then acc += (s(+k)+s(-k))*wk, k = 2..R
float4 Combine(float4 s[2 * R + 2])     // s[R + t] = tap t (s[2R+1] unused)
{
    float4 acc = (s[R + 1] + s[R - 1]) * W[1] + s[R] * W[0];
    [unroll] for (int k = 2; k <= R; ++k)
        acc = acc + (s[R + k] + s[R - k]) * W[k];
    return acc;
}

float4 main(float4 pos : SV_Position, float2 tc : TEXCOORD0) : SV_Target0
{
    uint w, h;
    tSrc.GetDimensions(w, h);
    float2 size = float2(w, h);
    // derivatives for the fallback, taken in uniform control flow (the values implicit-LOD Sample uses)
    float2 dx = ddx(tc), dy = ddy(tc);

    float2 p  = tc * size - 0.5f;          // texel-space position (texel centres at integers)
    float2 ip = floor(p + 0.5f);
    float2 f  = p - ip;

    [branch] if (abs(f.x) <= (1.0f / 1024.0f) && abs(f.y) <= (1.0f / 1024.0f)) {
        float2 rcpSize = 1.0f / size;
        float4 t[2 * R + 2];
        [unroll] for (int j = 0; j < NG; ++j) {
            // block: our column/row (u0/v0) plus the next one; along the axis texels ip-R+2j, ip-R+2j+1.
            // UV = the block's shared corner, 0.5 texel away from any floor() boundary.
#if VERT
            float2 uv = float2(ip.x + 1.0f, ip.y + (float)(2 * j - R + 1)) * rcpSize;
#else
            float2 uv = float2(ip.x + (float)(2 * j - R + 1), ip.y + 1.0f) * rcpSize;
#endif
            float4 r = tSrc.GatherRed  (sSrc, uv);
            float4 g = tSrc.GatherGreen(sSrc, uv);
            float4 b = tSrc.GatherBlue (sSrc, uv);
            float4 a = tSrc.GatherAlpha(sSrc, uv);
            // Gather component order: x=(u0,v1) y=(u1,v1) z=(u1,v0) w=(u0,v0)
#if VERT
            t[2 * j]     = float4(r.w, g.w, b.w, a.w);   // (u0, v0): row ip.y-R+2j
            t[2 * j + 1] = float4(r.x, g.x, b.x, a.x);   // (u0, v1): row ip.y-R+2j+1
#else
            t[2 * j]     = float4(r.w, g.w, b.w, a.w);   // (u0, v0): column ip.x-R+2j
            t[2 * j + 1] = float4(r.z, g.z, b.z, a.z);   // (u1, v0): column ip.x-R+2j+1
#endif
        }
        return Combine(t);
    }

    // ---- fallback: the original 2R+1 taps -----------------------------------------------------------
#if VERT
    float d = 1.0f / (float)(int)h;
#else
    float d = 1.0f / (float)(int)w;
#endif
    float4 s[2 * R + 2];
    s[2 * R + 1] = 0;
    [unroll] for (int k = -R; k <= R; ++k) {
#if VERT
        float2 uv = float2(tc.x, (k < 0) ? tc.y - d * (float)(-k) : ((k == 0) ? tc.y : d * (float)k + tc.y));
#else
        float2 uv = float2((k < 0) ? tc.x - d * (float)(-k) : ((k == 0) ? tc.x : d * (float)k + tc.x), tc.y);
#endif
        s[k + R] = tSrc.SampleGrad(sSrc, uv, dx, dy);
    }
    return Combine(s);
}

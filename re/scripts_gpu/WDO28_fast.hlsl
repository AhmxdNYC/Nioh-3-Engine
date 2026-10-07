// WDO28_fast.hlsl - latency-hiding rewrite of shader_dx12\0x1fd07cc0.file group "WDO-PtSrbVsm" variant #28
// (RGP PSO 3e7955a89ee71e82, DXBC digest 32e8deff99183945b94bc3be679bfa95, ~367 us/frame in trace r5).
//
// Original (reconstructed from DXIL, see notes/invisible-gpu.md): 1-D vertical, depth-aware Gaussian blur of
// the ALPHA channel of slice 0 of sColor (the sun-shadow mask written by WDO-PtSrbSsmVsm#14). Per tap it does
//   SampleLevel(depth) -> wait -> compare -> SampleLevel(color) -> wait   (2 serialized round trips per tap).
// This version computes the same sum in the same order, but issues the depth and colour fetches for 4 taps at
// once (colour fetched unconditionally, its contribution masked with the same predicates), so one wait covers
// 8 fetches. Interface (registers, $Globals layout, signature) is identical to the original.
//
// NOT INSTALLED. Compile check only: dxc -T ps_6_5 -E main -O3 -validator-version 1.5 WDO28_fast.hlsl

float4 vP2T;              // uv = tc.xy * vP2T.xy + vP2T.zw
float4 vResolutionParam;  // .w = texel step in v
float4 vBlurParam;        // .x = radius R (taps), .y = gaussian k (w = exp2(-t*t*k); DXIL op 21 is base-2), .z = depth-difference threshold
float4 vDepthParam;       // z = 1/(d*x + y); .z = far threshold (pixels at/after it are not blurred / not gathered)
float4 vViewportParam;    // .zw = viewport origin (uv), .xy = viewport size (uv)

SamplerState   __smpsDepth : register(s1);
SamplerState   __smpsColor : register(s3);
Texture2D      sDepth      : register(t1);
Texture2DArray sColor      : register(t3);

// branch-free (HLSL 2021 && short-circuits, which made the compiler re-serialise the fetches)
float InVPf(float x, float y)
{
    return (float)(x >= vViewportParam.z) * (float)(x <= vViewportParam.z + vViewportParam.x) *
           (float)(y >= vViewportParam.w) * (float)(y <= vViewportParam.w + vViewportParam.y);
}
bool InVP(float x, float y) { return InVPf(x, y) != 0.0f; }

float LinZ(float d) { return 1.0f / (d * vDepthParam.x + vDepthParam.y); }

// accumulate 4 taps t0..t0+3, each enabled by en[k]
void Tap4(float u, float v0, float step, float z0, float t0, bool4 en, inout float acc, inout float wsum)
{
    float4 t = t0 + float4(0, 1, 2, 3);
    float4 y = t * step + v0;
    float4 d, c;
    [unroll] for (int k = 0; k < 4; k++) d[k] = sDepth.SampleLevel(__smpsDepth, float2(u, y[k]), 0).x;
    [unroll] for (int k = 0; k < 4; k++) c[k] = sColor.SampleLevel(__smpsColor, float3(u, y[k], 0), 0).w;
    [unroll] for (int k = 0; k < 4; k++)
    {
        float z = LinZ(d[k]);
        float m = (float)en[k] * InVPf(u, y[k]) * (float)(!(vDepthParam.z <= z));   // original: fcmp ugt far, z
        float w = exp2(-(t[k] * t[k]) * vBlurParam.y) * (float)(abs(z - z0) < vBlurParam.z);
        float cw = (m != 0.0f) ? w * c[k] : 0.0f;   // select, not a branch: a NaN colour at a rejected tap stays out
        acc  += cw;                                   // same summation order as the serial loop
        wsum += m * w;
    }
}

float4 main(noperspective float4 pos : SV_Position, float4 tc : TEXCOORD0) : SV_Target0   // same signature/interp as original
{
    float u = tc.x * vP2T.x + vP2T.z;
    float v = tc.y * vP2T.y + vP2T.w;
    float step = vResolutionParam.w;
    float c0 = sColor.SampleLevel(__smpsColor, float3(u, v, 0), 0).w;
    float z0 = LinZ(sDepth.SampleLevel(__smpsDepth, float2(u, v), 0).x);
    if (vDepthParam.z <= z0 || !InVP(u, v))
        return float4(0, 0, 0, c0);

    float R = vBlurParam.x;
    float acc = c0, wsum = 1.0f;
    if (R > 0.0f)                       // t = -R, -R+1, ... while t < 0  (ceil(R) taps)
    {
        [loop] for (float t = -R; t < 0.0f; t += 4.0f)
            Tap4(u, v, step, z0, t, (t + float4(0, 1, 2, 3)) < 0.0f, acc, wsum);
    }
    if (R >= 1.0f)                      // t = 1, 2, ... while t <= R  (floor(R) taps)
    {
        [loop] for (float t = 1.0f; t <= R; t += 4.0f)
            Tap4(u, v, step, z0, t, (t + float4(0, 1, 2, 3)) <= R, acc, wsum);
    }
    return float4(0, 0, 0, acc / wsum);
}




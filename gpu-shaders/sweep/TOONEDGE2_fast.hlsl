// TOONEDGE2_fast.hlsl - replacement for shader_dx12\0x9c481f79.file (and its twin 0x47bc5cb4.file) group "TOONEDGE2"
// record #2: the full-screen toon-outline distance pass (Dispatch 120x68 groups of 16x16 at 1080p, every frame).
// Same interface as the original: $Globals {int nResX; int nResY; float fThick;} (b0, 12 B), Texture2D<float4>
// sInputTex t8, Texture2D<float> sDepth t9, Texture2D<uint2> sStencil t10, RWTexture2D<float> uOutputTex u0, cs_6_0,
// numthreads(16,16,1). Built by build_sweep.py, proven by verify_sweep.py.
//
// Original (DXIL digest 318fa766...), per thread (x, y):
//   c = sInputTex[x,y] (unclamped); if any(c != 0) -> FLT_MAX
//   n[k] = sInputTex[clamp((x,y) + o[k])] for the 8 neighbours; if (sStencil[x,y].y & 1) -> FLT_MAX
//   dC = sDepth[clamp(x,y)]; acc = fThick + 0.5; for k: if sDepth[clamp(p_k)] >= dC (ordered) && any(n[k] != 0) acc -= (fThick+0.5)/4
//   then a classification of the quantised (x255) R/G of the 4-neighbours / diagonals -> +-0.5 +- max(acc,0),
//   +-0.7071070 (0x3F3504F7) +- max(acc,0) or 3.4028231e38 (0x7F7FFFFD); both are the original's constants ("FLT_MAX" below).  -> 19 image loads per pixel on the common path (center == 0).
//
// Here: the 16x16 group stages an 18x18 tile of sInputTex (and, only if any pixel of the group needs it, of sDepth)
// in LDS with exactly the original's clamped coordinates, so the common path costs ~1.3 + 1.3 + 1 (stencil) image
// loads per pixel instead of 19; every neighbour value is the same texel the original loads, and the arithmetic is the
// original's op for op. Threads outside [0,nResX) x [0,nResY) (the 8 padding rows at 1080p) take the original code
// path with direct loads, so out-of-bounds behaviour is unchanged.

int nResX;
int nResY;
float fThick;
Texture2D<float4> sInputTex : register(t8);
Texture2D<float> sDepth : register(t9);
Texture2D<uint2> sStencil : register(t10);
RWTexture2D<float> uOutputTex : register(u0);

#define TW 18
groupshared float4 sIn[TW * TW];
groupshared float sDp[TW * TW];
groupshared uint sNeed[4];

static const int2 kOff[8] = { int2(0, -1), int2(-1, 0), int2(1, 0), int2(0, 1),
                              int2(-1, -1), int2(1, -1), int2(-1, 1), int2(1, 1) };

int2 clampXY(int2 p)
{
    return int2(min(max(p.x, 0), nResX - 1), min(max(p.y, 0), nResY - 1));
}

bool nz(float4 v)
{
    return v.x != 0.0f || v.y != 0.0f || v.z != 0.0f || v.w != 0.0f;
}

int q255(float v)
{
    return (int)(v * 255.0f + 0.5f);
}

// the original's classification, given the center (all zero here), the 8 neighbours, and the accumulated thickness
float classify(float4 c, float4 n[8], float acc)
{
    const float t = max(acc, 0.0f);
    const int c0 = q255(c.x), c1 = q255(c.y);
    bool e = false;
    [unroll] for (uint k = 0; k < 4; k++)
        e = e || (q255(n[k].x) != c0) || (q255(n[k].y) != c1);
    if (e)
        return (n[0].x > 0.0f || n[1].x > 0.0f || n[2].x > 0.0f || n[3].x > 0.0f || c.x > 0.0f) ? (-0.5f - t) : (t + 0.5f);
    const int a0 = q255(n[0].x), a1 = q255(n[0].y);
    bool g = false;
    [unroll] for (uint k2 = 4; k2 < 8; k2++)
        g = g || (q255(n[k2].x) != a0) || (q255(n[k2].y) != a1);
    if (g)
        return (n[4].x > 0.0f || n[5].x > 0.0f || n[6].x > 0.0f || n[7].x > 0.0f || c.x > 0.0f) ? (-asfloat(0x3F3504F7u) - t) : (t + asfloat(0x3F3504F7u));
    return asfloat(0x7F7FFFFDu);
}

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID)
{
    const int x = (int)DTid.x, y = (int)DTid.y;
    const uint flat = GTid.y * 16u + GTid.x;
    const int2 org = int2(Gid.xy) * 16 - 1;
    const bool inRange = x < nResX && y < nResY;          // x, y >= 0 always; then clamp(x,y) == (x,y)
    const uint ci = (GTid.y + 1u) * TW + GTid.x + 1u;

    // ---- phase 1: input tile (18x18, original clamped coordinates)
    if (flat == 0u)
        sNeed[0] = 0u;
    [unroll] for (uint r = 0; r < 2; r++)
    {
        const uint t = flat + r * 256u;
        if (t < TW * TW)
            sIn[t] = sInputTex.Load(int3(clampXY(org + int2(t % TW, t / TW)), 0));
    }
    GroupMemoryBarrierWithGroupSync();

    // ---- phase 2: center, stencil; does any pixel of the group reach the depth test?
    float4 c = 0.0f;
    uint st = 0u;
    if (inRange)
    {
        c = sIn[ci];
        if (!nz(c))
        {
            st = sStencil.Load(int3(x, y, 0)).y;
            if ((st & 1u) == 0u)
                InterlockedOr(sNeed[0], 1u);
        }
    }
    GroupMemoryBarrierWithGroupSync();

    // ---- phase 3: depth tile, only when needed
    if (sNeed[0] != 0u)
    {
        [unroll] for (uint r2 = 0; r2 < 2; r2++)
        {
            const uint t = flat + r2 * 256u;
            if (t < TW * TW)
                sDp[t] = sDepth.Load(int3(clampXY(org + int2(t % TW, t / TW)), 0));
        }
    }
    GroupMemoryBarrierWithGroupSync();

    // ---- phase 4: the original computation
    float res = asfloat(0x7F7FFFFDu);
    const float t0 = fThick + 0.5f;
    const float stp = t0 * 0.25f;
    if (inRange)
    {
        if (!nz(c))
        {
            float4 n[8];
            [unroll] for (uint k = 0; k < 8; k++)
                n[k] = sIn[(uint)((int)ci + kOff[k].y * TW + kOff[k].x)];
            if ((st & 1u) == 0u)
            {
                const float dC = sDp[ci];
                float acc = t0;
                [unroll] for (uint k3 = 0; k3 < 8; k3++)
                {
                    const float d = sDp[(uint)((int)ci + kOff[k3].y * TW + kOff[k3].x)];
                    if (d >= dC && nz(n[k3]))
                        acc = acc - stp;
                }
                res = classify(c, n, acc);
            }
        }
    }
    else
    {
        // original code path, direct loads (threads outside the nResX x nResY rectangle)
        const float4 cc = sInputTex.Load(int3(x, y, 0));
        if (!nz(cc))
        {
            float4 n[8];
            [unroll] for (uint k = 0; k < 8; k++)
                n[k] = sInputTex.Load(int3(clampXY(int2(x, y) + kOff[k]), 0));
            const uint s2 = sStencil.Load(int3(x, y, 0)).y;
            if ((s2 & 1u) == 0u)
            {
                const float dC = sDepth.Load(int3(clampXY(int2(x, y)), 0));
                float acc = t0;
                [unroll] for (uint k3 = 0; k3 < 8; k3++)
                {
                    const float d = sDepth.Load(int3(clampXY(int2(x, y) + kOff[k3]), 0));
                    if (d >= dC && nz(n[k3]))
                        acc = acc - stp;
                }
                res = classify(cc, n, acc);
            }
        }
    }
    uOutputTex[uint2(x, y)] = res;
}

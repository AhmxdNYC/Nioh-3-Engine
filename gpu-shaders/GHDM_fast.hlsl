// GHDM_fast.hlsl - drop-in replacement for Nioh 3 shader group "GHDM" (file 0x0104840c.file), variant 0
// (original DXIL hash 36608e0b5f698841356cf8f3e85bcdd7, container digest b6429c529ecf08a2cb36bd882a841c19,
//  RGP api_pso_hash e6429cd12f46fbbb). Depth min/max downsample ("hierarchical depth map").
//
// Interface is identical to the original (same register numbers, same $Globals layout, same names, cs_6_0,
// numthreads 8x8x1, same output: dst texel = MIN (reverse-Z) / MAX (normal Z) over its source footprint).
//
// Original behaviour (reconstructed from DXIL):
//   if (tid < dstSize):
//     if srcSize % dstSize == 0 (both axes): r = srcSize/dstSize; reduce tSrc.Load over the r.x*r.y block
//                                            at srcOff + tid*r   (one Load per loop iteration, fully serial)
//     else: n = ceil(srcSize/dstSize); reduce n.x*n.y SampleLevel taps on a UV grid (serial)
//     init 2.0 for MIN, -2.0 for MAX;  uDst[dstOff + tid] = acc.xxxx
// The original issues one texture fetch per iteration and waits for it (s_wait_samplecnt 0) -> latency bound
// (504 us for a 512x512 output on an RX 9070).
//
// This version: integer path uses GatherRed (4 texels / fetch) with 8 independent gathers per iteration
// (8x4 texel tile), clamped inside the block so duplicates never leave the footprint (min/max are idempotent,
// so the result is bit-identical to the original). Non-integer path keeps the exact UV formula but issues 4
// SampleLevel taps per iteration. Falls back to batched Loads when a block dimension is 1.

uint nSrcOffsetSize[4];   // srcOffX, srcOffY, srcW, srcH          (offset 0, 16-byte array stride)
uint nDstOffsetSize[4];   // dstOffX, dstOffY, dstW, dstH          (offset 64)
uint nIsReverseZ;         // != 0 -> MIN, == 0 -> MAX              (offset 116)

SamplerState        __smptSrc : register(s8);
Texture2D<float4>   tSrc      : register(t8);
RWTexture2D<float4> uDst      : register(u0);

float Red(float a, float b, bool useMin) { return useMin ? min(a, b) : max(a, b); }
float Red4(float4 v, bool useMin)
{
    return useMin ? min(min(v.x, v.y), min(v.z, v.w)) : max(max(v.x, v.y), max(v.z, v.w));
}

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    const uint srcOffX = nSrcOffsetSize[0], srcOffY = nSrcOffsetSize[1];
    const uint srcW    = nSrcOffsetSize[2], srcH    = nSrcOffsetSize[3];
    const uint dstOffX = nDstOffsetSize[0], dstOffY = nDstOffsetSize[1];
    const uint dstW    = nDstOffsetSize[2], dstH    = nDstOffsetSize[3];

    if (!(tid.x < dstW && tid.y < dstH))
        return;

    const bool useMin = (nIsReverseZ != 0);
    float acc = useMin ? 2.0f : -2.0f;

    if (((srcW % dstW) | (srcH % dstH)) == 0)
    {
        const uint rx = srcW / dstW;
        const uint ry = srcH / dstH;
        const int2 base = int2(rx * tid.x + srcOffX, ry * tid.y + srcOffY);

        if (rx >= 2 && ry >= 2)
        {
            uint texW, texH;
            tSrc.GetDimensions(texW, texH);
            const float2 inv = 1.0f / float2(texW, texH);
            const int mx = int(rx) - 2, my = int(ry) - 2;       // last legal 2x2 corner inside the block
            [loop]
            for (int y = 0; y < int(ry); y += 4)
            {
                const int y0 = min(y, my), y1 = min(y + 2, my);
                [loop]
                for (int x = 0; x < int(rx); x += 8)
                {
                    const int x0 = min(x, mx), x1 = min(x + 2, mx), x2 = min(x + 4, mx), x3 = min(x + 6, mx);
                    // texel-corner UV of the 2x2 quad whose top-left texel is (base + (xi, yi))
                    float4 g0 = tSrc.GatherRed(__smptSrc, (float2(base + int2(x0, y0)) + 1.0f) * inv);
                    float4 g1 = tSrc.GatherRed(__smptSrc, (float2(base + int2(x1, y0)) + 1.0f) * inv);
                    float4 g2 = tSrc.GatherRed(__smptSrc, (float2(base + int2(x2, y0)) + 1.0f) * inv);
                    float4 g3 = tSrc.GatherRed(__smptSrc, (float2(base + int2(x3, y0)) + 1.0f) * inv);
                    float4 g4 = tSrc.GatherRed(__smptSrc, (float2(base + int2(x0, y1)) + 1.0f) * inv);
                    float4 g5 = tSrc.GatherRed(__smptSrc, (float2(base + int2(x1, y1)) + 1.0f) * inv);
                    float4 g6 = tSrc.GatherRed(__smptSrc, (float2(base + int2(x2, y1)) + 1.0f) * inv);
                    float4 g7 = tSrc.GatherRed(__smptSrc, (float2(base + int2(x3, y1)) + 1.0f) * inv);
                    float a0 = Red(Red4(g0, useMin), Red4(g1, useMin), useMin);
                    float a1 = Red(Red4(g2, useMin), Red4(g3, useMin), useMin);
                    float a2 = Red(Red4(g4, useMin), Red4(g5, useMin), useMin);
                    float a3 = Red(Red4(g6, useMin), Red4(g7, useMin), useMin);
                    acc = Red(acc, Red(Red(a0, a1, useMin), Red(a2, a3, useMin), useMin), useMin);
                }
            }
        }
        else
        {
            // a block dimension is 1: batched Loads (4 in flight), clamped inside the block
            const int lx = int(rx) - 1, ly = int(ry) - 1;
            [loop]
            for (int y = 0; y < int(ry); ++y)
            {
                [loop]
                for (int x = 0; x < int(rx); x += 4)
                {
                    float v0 = tSrc.Load(int3(base + int2(min(x + 0, lx), y), 0)).x;
                    float v1 = tSrc.Load(int3(base + int2(min(x + 1, lx), y), 0)).x;
                    float v2 = tSrc.Load(int3(base + int2(min(x + 2, lx), y), 0)).x;
                    float v3 = tSrc.Load(int3(base + int2(min(x + 3, lx), y), 0)).x;
                    acc = Red(acc, Red(Red(v0, v1, useMin), Red(v2, v3, useMin), useMin), useMin);
                }
            }
        }
    }
    else
    {
        // non-integer ratio: identical UV math to the original, 4 taps per iteration
        uint texWi, texHi;
        tSrc.GetDimensions(texWi, texHi);
        const float texW = float(int(texWi)), texH = float(int(texHi));
        const float sw = float(srcW), sh = float(srcH);
        const float dw = float(dstW), dh = float(dstH);
        const float2 uvOff = float2(float(srcOffX) / texW, float(srcOffY) / texH);
        const float2 uvMax = float2((sw - 0.5f) / texW, (sh - 0.5f) / texH);
        const float2 uvc   = float2((float(tid.x) + 0.5f) / dw, (float(tid.y) + 0.5f) / dh);
        const float2 base  = float2(sw / texW * uvc.x, sh / texH * uvc.y) + uvOff;
        const float nxf = ceil(sw / dw), nyf = ceil(sh / dh);
        const float2 stepUV = float2(1.0f / (nxf * dw), 1.0f / (nyf * dh));
        const int nx = int(uint(nxf)), ny = int(uint(nyf));
        const int lx = nx - 1;
        [loop]
        for (int y = 0; y < ny; ++y)
        {
            const float v = min(base.y + float(uint(y)) * stepUV.y, uvMax.y);
            [loop]
            for (int x = 0; x < nx; x += 4)
            {
                const float u0 = min(base.x + float(uint(min(x + 0, lx))) * stepUV.x, uvMax.x);
                const float u1 = min(base.x + float(uint(min(x + 1, lx))) * stepUV.x, uvMax.x);
                const float u2 = min(base.x + float(uint(min(x + 2, lx))) * stepUV.x, uvMax.x);
                const float u3 = min(base.x + float(uint(min(x + 3, lx))) * stepUV.x, uvMax.x);
                float s0 = tSrc.SampleLevel(__smptSrc, float2(u0, v), 0).x;
                float s1 = tSrc.SampleLevel(__smptSrc, float2(u1, v), 0).x;
                float s2 = tSrc.SampleLevel(__smptSrc, float2(u2, v), 0).x;
                float s3 = tSrc.SampleLevel(__smptSrc, float2(u3, v), 0).x;
                acc = Red(acc, Red(Red(s0, s1, useMin), Red(s2, s3, useMin), useMin), useMin);
            }
        }
    }

    uDst[int2(dstOffX + tid.x, dstOffY + tid.y)] = acc.xxxx;
}

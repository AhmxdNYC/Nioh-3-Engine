// MPC23_fast.hlsl - replacement for shader_dx12\0x29ee0902.file group "MPC" record #23 (meshlet culling chain:
// per-draw prefix sums of the meshlet argument counts). Same interface as the original: $Globals (b0, 32 B),
// RWBuffer<uint> u2 / u9 / u11, cs_6_0, numthreads(16, 64, 1). Built by build_sweep.py, proven by verify_sweep.py.
//
// Original (DXIL, digest a969db73...): thread i = DTid.x*64 + DTid.y, i < nVSDrawPrm.y:
//     start = (DTid.y == 0) ? DTid.x*64 - 64 : DTid.x*64;  sa = sb = 0
//     for (j = start; j < i; j++) { sa += uArguments[nMshltPrm2.x + 4j]; sb += uVSDrawMshltMtrOffsets[3j + 2]; }
//     uVSDrawMshltMtrOffsets[3i] = sa;  uVSDrawArguments[9i + 9] = sb;
// It runs as Dispatch(1,1,1) four times per frame: 16 waves, each thread walking up to 64 serial load round trips
// (11-16 us per call on the RX 9070 for ~1k elements; worse on the 780M, whose memory latency is higher).
//
// Here: each element is loaded once (1 round trip), a Hillis-Steele scan over each 64-element segment runs in LDS,
// and thread i reads its exclusive prefix (segment-relative) or the previous segment's total. u32 addition is
// associative mod 2^32, so the sums are bit-identical to the serial loop in any order. Threads whose range is not
// inside this group's LDS copy (only possible for Gid.y > 0 or the first thread of a group with Gid.x > 0, i.e. never
// for the game's Dispatch(1,1,1)) run the original loop unchanged.
// Every element the scan reads is one the original reads too (j <= cnt-2); nothing else is loaded.

int4 nMshltPrm2;
uint4 nVSDrawPrm;
RWBuffer<uint> uArguments : register(u2);
RWBuffer<uint> uVSDrawArguments : register(u9);
RWBuffer<uint> uVSDrawMshltMtrOffsets : register(u11);

groupshared uint sA[1024];
groupshared uint sB[1024];

[numthreads(16, 64, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID)
{
    const uint i = (DTid.x << 6) + DTid.y;
    const uint li = (GTid.x << 6) + GTid.y;
    const uint cnt = nVSDrawPrm.y;

    uint a = 0u, b = 0u;
    if (Gid.y == 0u && i + 1u < cnt)
    {
        a = uArguments[asuint(nMshltPrm2.x) + (i << 2)];
        b = uVSDrawMshltMtrOffsets[i * 3u + 2u];
    }
    sA[li] = a;
    sB[li] = b;
    GroupMemoryBarrierWithGroupSync();
    [unroll] for (uint d = 1u; d < 64u; d <<= 1)
    {
        if (GTid.y >= d)
        {
            a += sA[li - d];
            b += sB[li - d];
        }
        GroupMemoryBarrierWithGroupSync();
        sA[li] = a;
        sB[li] = b;
        GroupMemoryBarrierWithGroupSync();
    }

    if (i < cnt)
    {
        uint ra = 0u, rb = 0u;
        if (Gid.y == 0u && li != 0u)
        {
            ra = sA[li - 1u];            // GTid.y > 0: sum [x*64, i);  GTid.y == 0: total of segment x-1
            rb = sB[li - 1u];
        }
        else
        {
            const uint start = (DTid.y == 0u) ? (DTid.x << 6) - 64u : (DTid.x << 6);
            [loop] for (uint j = start; j < i; j++)
            {
                ra += uArguments[asuint(nMshltPrm2.x) + (j << 2)];
                rb += uVSDrawMshltMtrOffsets[j * 3u + 2u];
            }
        }
        uVSDrawMshltMtrOffsets[i * 3u] = ra;
        uVSDrawArguments[i * 9u + 9u] = rb;
    }
}

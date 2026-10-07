// MPC28_chain.hlsl - replacement for 0x29ee0902.file group "MPC" record #28 (second stage of the per-draw prefix-sum
// chain, see MPC23_chain.hlsl). Same interface as the shipped record: $Globals (b0, 16 B: uint4 nVSDrawPrm),
// RWBuffer<uint> u9 uVSDrawArguments / u11 uVSDrawMshltMtrOffsets, cs_6_0, numthreads(32, 16, 1).
// cnt <= 1024: the replacement #23 already wrote this record's results (L1) and the final sums -> nothing to do.
// cnt > 1024: the shipped code, transcribed 1:1 from the DXIL (per 64-element block m = 16*DTid.x + DTid.y:
//   L1[m] = sum of the block totals before it in its 1024-element superblock, or (DTid.y == 0) the previous
//   superblock's total; the block totals sit at O[64k] after the shipped #23).
uint4 nVSDrawPrm;
RWBuffer<uint> uVSDrawArguments : register(u9);
RWBuffer<uint> uVSDrawMshltMtrOffsets : register(u11);

[numthreads(32, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    const uint cnt = nVSDrawPrm.y;
    if (cnt <= 1024u)
        return;
    const uint base = (DTid.x << 10) + (DTid.y << 6);
    if (base < cnt)
    {
        const uint first = (DTid.x << 10) + 64u;
        const uint start = (DTid.y == 0u) ? first - 1024u : first;
        uint a = 0u, b = 0u;
        if (start <= base)
        {
            uint k = start;
            [loop] do
            {
                a += uVSDrawMshltMtrOffsets[k * 3u];
                b += uVSDrawArguments[k * 9u + 9u];
                k += 64u;
            } while (k <= base);
        }
        const uint o = cnt * 3u + (((DTid.x << 4) + DTid.y) << 1);
        uVSDrawMshltMtrOffsets[o] = a;
        uVSDrawMshltMtrOffsets[o + 1u] = b;
    }
}

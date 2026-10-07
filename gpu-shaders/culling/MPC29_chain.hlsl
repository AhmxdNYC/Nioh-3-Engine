// MPC29_chain.hlsl - replacement for 0x29ee0902.file group "MPC" record #29 (third stage of the per-draw prefix-sum
// chain, see MPC23_chain.hlsl): the single-thread running sum over the 1024-element superblocks. Same interface as the
// shipped record: $Globals (b0, 16 B: uint4 nVSDrawPrm), RWBuffer<uint> u9 / u11, cs_6_0, numthreads(1, 1, 1).
// cnt <= 1024 (one superblock, a 1-iteration loop): the replacement #23 already wrote the result -> nothing to do.
// cnt > 1024: the shipped loop, transcribed 1:1 (ceil(cnt/1024) iterations; not a measurable cost below cnt ~ 32k).
uint4 nVSDrawPrm;
RWBuffer<uint> uVSDrawArguments : register(u9);
RWBuffer<uint> uVSDrawMshltMtrOffsets : register(u11);

[numthreads(1, 1, 1)]
void main()
{
    const uint cnt = nVSDrawPrm.y;
    if (cnt <= 1024u)
        return;
    uint a = 0u, b = 0u, i = 0u;
    [loop] do
    {
        const uint o = cnt * 3u + (i >> 5);
        a += uVSDrawMshltMtrOffsets[o];
        b += uVSDrawMshltMtrOffsets[o + 1u];
        uVSDrawMshltMtrOffsets[i * 3u] = a;
        uVSDrawArguments[i * 9u + 9u] = b;
        i += 1024u;
    } while (i < cnt);
}

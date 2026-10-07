// MPC30_chain.hlsl - replacement for 0x29ee0902.file group "MPC" record #30 (fourth stage of the per-draw prefix-sum
// chain, see MPC23_chain.hlsl): block starts += superblock offsets. Same interface as the shipped record: $Globals
// (b0, 16 B: uint4 nVSDrawPrm), RWBuffer<uint> u9 / u11, cs_6_0, numthreads(32, 16, 1).
// cnt <= 1024: already final (replacement #23) -> nothing to do. cnt > 1024: the shipped code, transcribed 1:1.
uint4 nVSDrawPrm;
RWBuffer<uint> uVSDrawArguments : register(u9);
RWBuffer<uint> uVSDrawMshltMtrOffsets : register(u11);

[numthreads(32, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    const uint cnt = nVSDrawPrm.y;
    if (cnt <= 1024u)
        return;
    const uint j = (DTid.x << 10) + (DTid.y << 6);
    if (j < cnt && DTid.y != 0u)
    {
        const uint o = cnt * 3u + (((DTid.x << 4) + DTid.y) << 1);
        uVSDrawMshltMtrOffsets[j * 3u] = uVSDrawMshltMtrOffsets[o] + uVSDrawMshltMtrOffsets[DTid.x * 3072u];
        uVSDrawArguments[j * 9u + 9u] = uVSDrawMshltMtrOffsets[o + 1u] + uVSDrawArguments[DTid.x * 9216u + 9u];
    }
}

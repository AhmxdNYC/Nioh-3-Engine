// MPC31_chain.hlsl - replacement for 0x29ee0902.file group "MPC" record #31 (last stage of the per-draw prefix-sum
// chain, see MPC23_chain.hlsl): elements += their block start. Same interface as the shipped record: $Globals
// (b0, 16 B: uint4 nVSDrawPrm), RWBuffer<uint> u9 / u11, cs_6_0, numthreads(16, 64, 1).
// cnt <= 1024: already final (replacement #23) -> nothing to do. cnt > 1024: the shipped code, transcribed 1:1.
uint4 nVSDrawPrm;
RWBuffer<uint> uVSDrawArguments : register(u9);
RWBuffer<uint> uVSDrawMshltMtrOffsets : register(u11);

[numthreads(16, 64, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    const uint cnt = nVSDrawPrm.y;
    if (cnt <= 1024u)
        return;
    const uint j = (DTid.x << 6) + DTid.y;
    if (j < cnt && DTid.y != 0u)
    {
        uVSDrawMshltMtrOffsets[j * 3u] = uVSDrawMshltMtrOffsets[DTid.x * 192u] + uVSDrawMshltMtrOffsets[j * 3u];
        uVSDrawArguments[j * 9u + 9u] = uVSDrawArguments[DTid.x * 576u + 9u] + uVSDrawArguments[j * 9u + 9u];
    }
}

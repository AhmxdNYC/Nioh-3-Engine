// MshLod3_fast.hlsl - replacement for shader_dx12\0x8d9282e6.file, group "MshLod", record #3 (meshlet LOD chain: the
// 1x1x1 dispatch before each level's MshLod#2 ExecuteIndirect, 9 per frame). Same interface as the shipped record:
// $Globals (b0, 16 B), RWBuffer<uint> u1 uArguments / u3 uArgumentForNextLv, RWStructuredBuffer<S_PAYLOAD_TO_MRLOD> u4,
// cs_6_0, numthreads(32, 1, 1). Built by sweep\build_sweep.py (item "mshlod"), proven by verify_culling.py mshlod.
//
// Shipped (DXIL, digest a1426af0...), every thread t = DTid.x < nArgNum:
//     uArguments[nArgOff + 4t + 0..3] = 0, 1, 1, 0
//     c = min(uArgumentForNextLv[0], nMax);  r = (c + 31) >> 5;  uArgumentForNextLv[0] = r
//     for (p = c; p != 32r; p++) if (p < nMax) uPayloadForMRLOD[nPayOff + p] = { 0, 0xFFFFFFFF }
// i.e. EVERY active thread runs the same serial pad loop (up to 31 iterations x 2 stores) over the same elements.
// All active lanes of the (single) wave issue the u3[0] load before any lane stores it, so they all read the same value.
//
// Here: the same u1 stores; u3[0] is read by every lane of an active group, a group barrier, then the same u3[0] store
// by the active threads, and the pad elements are spread over the lanes (lane j writes element c + j: one store pair per
// lane instead of a 31-iteration loop per lane). Same addresses, same values, same final memory. The pad count
// m = 32r - c (mod 2^32) is <= 31 for every c, including c >= 2^32 - 31 where c + 31 wraps (r = 0, m = 2^32 - c).
// A group is active iff its first thread is (t < nArgNum is monotone in t); inactive groups write nothing, as shipped.

struct S_PAYLOAD_TO_MRLOD
{
    uint nData[2];
};

uint nArgNum;
uint nArgOff;
int nPayOffForCurrentLevel;
int nMaxMRLODPayloadNum;
RWBuffer<uint> uArguments : register(u1);
RWBuffer<uint> uArgumentForNextLv : register(u3);
RWStructuredBuffer<S_PAYLOAD_TO_MRLOD> uPayloadForMRLOD : register(u4);

void pad(uint p)
{
    S_PAYLOAD_TO_MRLOD e;
    e.nData[0] = 0u;
    e.nData[1] = 0xFFFFFFFFu;
    uPayloadForMRLOD[asuint(nPayOffForCurrentLevel) + p] = e;
}

[numthreads(32, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
    const uint t = DTid.x;
    const bool act = t < nArgNum;
    const bool gact = (t - GTid.x) < nArgNum;                  // the group's first thread is active
    if (act)
    {
        const uint a = nArgOff + (t << 2);
        uArguments[a] = 0u;
        uArguments[a + 1u] = 1u;
        uArguments[a + 2u] = 1u;
        uArguments[a + 3u] = 0u;
    }
    uint x = 0u;
    if (gact)
        x = uArgumentForNextLv[0];
    GroupMemoryBarrierWithGroupSync();                         // all lanes have read u3[0] before any lane writes it
    if (!gact)
        return;
    const uint nmax = asuint(nMaxMRLODPayloadNum);
    const uint c = min(x, nmax);
    const uint r = (c + 31u) >> 5;
    if (act)
        uArgumentForNextLv[0] = r;
    const uint m = (r << 5) - c;                               // pad elements [c, 32r): m <= 31 for every c
    const uint p = c + GTid.x;                                 // (c >= 2^32-31 wraps to r = 0, m = 2^32 - c)
    if (GTid.x < m && p < nmax)
        pad(p);
}

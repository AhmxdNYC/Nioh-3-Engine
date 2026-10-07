// MPC23_chain.hlsl - replacement for shader_dx12\0x29ee0902.file, group "MPC", record #23 (first stage of the
// meshlet culling "per-draw prefix sum" chain #23 -> #28 -> #29 -> #30 -> #31). Built by sweep\build_sweep.py (item "mpc") together
// with MPC28/29/30/31_chain.hlsl; proven by verify_culling.py (chain-level, DXIL interpreter, original vs replacement).
// Same interface as the shipped record: $Globals (b0, 32 B: int4 nMshltPrm2, uint4 nVSDrawPrm), RWBuffer<uint>
// u2 uArguments / u9 uVSDrawArguments / u11 uVSDrawMshltMtrOffsets, cs_6_0, numthreads(16, 64, 1).
//
// Notation: cnt = nVSDrawPrm.y (draws), C1[k] = uArguments[nMshltPrm2.x + 4k], C2[k] = u11[3k + 2],
//           O[i] = u11[3i], A[i] = u9[9i + 9], L1[m] = (u11[3cnt + 2m], u11[3cnt + 2m + 1]) (scratch after O).
// What the shipped chain computes (engine records it as Dispatch #23 (ceil(cnt/1024),1,1), #28 ((ceil(cnt/64)+511)/512),
// #29 (1,1,1), #30 (as #28), #31 (ceil(cnt/1024)), all inside one `if` of MeshletPreculling 5301FC, UAV barrier after
// each): the exclusive prefix sums O[i] = sum_{k<i} C1[k], A[i] = sum_{k<i} C2[k] for all i < cnt, plus
// L1[m] = (O[64m], A[64m]) for every 64m < cnt with m < 16, and for larger cnt per-superblock values (see #28).
// The shipped #23 alone only writes block-local values (each thread walks up to 63 dependent load round trips:
// 11-16 us per call on the RX 9070, ~4 calls per frame).
//
// This record:
//   * cnt <= 1024 (every chain in the captured frames: #23 runs as Dispatch(1,1,1)): group 0 computes the FINAL
//     state of the whole chain in one pass (1 load round trip per element, LDS scan), i.e. the final O, A and L1.
//     The replacement #28/#29/#30/#31 return immediately when cnt <= 1024, so the chain's final memory is identical
//     (u32 addition is associative mod 2^32; every write of the shipped chain is reproduced with the same final value,
//     no other address is written). This also makes the four later dispatches skippable engine-side (see
//     notes\gpu-culling-rewrite.md section 3).
//   * cnt > 1024: exactly the shipped #23 (block-local sums), computed with an LDS scan instead of serial loops.
//   * Both paths: thread 0 of group 0 also writes u9[1] = max(1, (u9[0] + 2047) >> 11), the value MPC#26 writes
//     later in the same chain (nothing in between touches u9[0] or u9[1]); #26 then rewrites the same value. This
//     only matters for the engine-side skip of #26 (the file-only install is identical either way).
//   * Groups with Gid.y/Gid.z != 0 (never produced by the engine's Dispatch(n,1,1)) run the shipped per-thread loop.
// Every element read is one the shipped chain reads too (C1/C2 indices <= cnt-2, u9[0]).

int4 nMshltPrm2;
uint4 nVSDrawPrm;
RWBuffer<uint> uArguments : register(u2);
RWBuffer<uint> uVSDrawArguments : register(u9);
RWBuffer<uint> uVSDrawMshltMtrOffsets : register(u11);

groupshared uint sA[1024];
groupshared uint sB[1024];
groupshared uint rA[64];
groupshared uint rB[64];
groupshared uint pA[64];
groupshared uint pB[64];

uint ldA(uint k) { return uArguments[asuint(nMshltPrm2.x) + (k << 2)]; }
uint ldB(uint k) { return uVSDrawMshltMtrOffsets[k * 3u + 2u]; }

[numthreads(16, 64, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
    const uint cnt = nVSDrawPrm.y;

    if (Gid.y != 0u || Gid.z != 0u)
    {
        // shipped code, unchanged (not reachable with the engine's dispatch)
        const uint i = (DTid.x << 6) + DTid.y;
        if (i < cnt)
        {
            const uint start = (DTid.y == 0u) ? (DTid.x << 6) - 64u : (DTid.x << 6);
            uint a = 0u, b = 0u;
            if (start < i)
            {
                uint j = start;
                [loop] do { a += ldA(j); b += ldB(j); j++; } while (j != i);
            }
            uVSDrawMshltMtrOffsets[i * 3u] = a;
            uVSDrawArguments[i * 9u + 9u] = b;
        }
        return;
    }

    // MPC#26's value, u9[1] = max(1, ceil(u9[0] / 2048)): loaded together with the elements, stored at the end
    // (keeps the load off the critical path of the wave that owns thread 0)
    const bool t0 = (Gid.x == 0u && GI == 0u);
    uint n26 = 0u;

    if (cnt <= 1024u)
    {
        // ---- whole chain in group 0 (other groups own no element < cnt and write nothing, as in the shipped chain)
        if (Gid.x != 0u)
            return;
        const uint e = GI;                                     // element owned by this thread (any bijection works)
        uint a = 0u, b = 0u;
        if (e + 1u < cnt)
        {
            a = ldA(e);
            b = ldB(e);
        }
        n26 = uVSDrawArguments[0];                             // uniform address, issued with the element loads
        sA[e] = a;
        sB[e] = b;
        GroupMemoryBarrierWithGroupSync();
        if (e < 64u)                                           // 64 runs of 16 elements: run totals
        {
            uint ta = 0u, tb = 0u;
            [unroll] for (uint k = 0u; k < 16u; k++)
            {
                ta += sA[(e << 4) + k];
                tb += sB[(e << 4) + k];
            }
            rA[e] = ta;
            rB[e] = tb;
        }
        GroupMemoryBarrierWithGroupSync();
        if (e < 64u)                                           // exclusive prefix of the 64 run totals:
        {                                                      // independent LDS reads, no serial chain
            uint qa = 0u, qb = 0u;
            [unroll] for (uint k = 0u; k < 63u; k++)
            {
                const uint ta = rA[k], tb = rB[k];
                qa += (k < e) ? ta : 0u;
                qb += (k < e) ? tb : 0u;
            }
            pA[e] = qa;
            pB[e] = qb;
        }
        GroupMemoryBarrierWithGroupSync();
        const uint r = e >> 4;
        uint pa = pA[r], pb = pB[r];
        [unroll] for (uint k = 0u; k < 15u; k++)
        {
            if (k < (e & 15u))
            {
                pa += sA[(r << 4) + k];
                pb += sB[(r << 4) + k];
            }
        }
        if (e < cnt)
        {
            uVSDrawMshltMtrOffsets[e * 3u] = pa;               // final O[e] (shipped: #23, then #29/#30/#31)
            uVSDrawArguments[e * 9u + 9u] = pb;                // final A[e]
            if ((e & 63u) == 0u)
            {
                const uint o = cnt * 3u + ((e >> 6) << 1);     // L1[e/64] as written by the shipped #28
                uVSDrawMshltMtrOffsets[o] = pa;
                uVSDrawMshltMtrOffsets[o + 1u] = pb;
            }
        }
        if (t0)
            uVSDrawArguments[1] = max(1u, (n26 + 2047u) >> 11);
        return;
    }

    // ---- cnt > 1024: the shipped #23 (block-local sums), scan in LDS
    {
        const uint i = (DTid.x << 6) + DTid.y;                 // element of this thread (shipped mapping)
        const uint li = (GTid.x << 6) + GTid.y;                // its LDS slot: segment GTid.x, position GTid.y
        uint a = 0u, b = 0u;
        if (i + 1u < cnt)
        {
            a = ldA(i);
            b = ldB(i);
        }
        // the group's first thread needs the total of the previous group's last segment (64 elements before i)
        uint pa = 0u, pb = 0u;
        const bool prev = (GTid.x == 0u) && (Gid.x != 0u) && ((DTid.x << 6) < cnt);
        if (prev)
        {
            pa = ldA((DTid.x << 6) - 64u + GTid.y);            // < i <= cnt-1: read by the shipped thread i too
            pb = ldB((DTid.x << 6) - 64u + GTid.y);
        }
        if (t0)
            n26 = uVSDrawArguments[0];
        sA[li] = a;
        sB[li] = b;
        if (GTid.x == 0u)
        {
            rA[GTid.y] = pa;
            rB[GTid.y] = pb;
        }
        GroupMemoryBarrierWithGroupSync();
        [unroll] for (uint d = 1u; d < 64u; d <<= 1)           // inclusive scan within each 64-element segment
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
            if (li != 0u)
            {
                ra = sA[li - 1u];       // GTid.y > 0: sum [64*DTid.x, i);  GTid.y == 0: total of the previous segment
                rb = sB[li - 1u];
            }
            else if (Gid.x != 0u)
            {
                [unroll] for (uint k = 0u; k < 64u; k++)
                {
                    ra += rA[k];
                    rb += rB[k];
                }
            }
            uVSDrawMshltMtrOffsets[i * 3u] = ra;
            uVSDrawArguments[i * 9u + 9u] = rb;
        }
        if (t0)
            uVSDrawArguments[1] = max(1u, (n26 + 2047u) >> 11);
    }
}

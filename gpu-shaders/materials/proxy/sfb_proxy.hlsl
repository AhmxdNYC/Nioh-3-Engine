// Compute-shader proxy of the material SFB MinMip clamp (3 texture slots), to see what amdxc emits for gfx1103/gfx1201.
// DEDUPE=0: shipped shape (4 loads per slot, unconditional). DEDUPE=1: 3 loads behind an if ('loads' mode).
// DEDUPE=2: loads + uitofp + max tree behind the if ('tree' mode, the generator default).
Buffer<uint> MinMip : register(t3);
Texture2D T0 : register(t0); Texture2D T1 : register(t1); Texture2D T2 : register(t2);
SamplerState S : register(s0);
cbuffer SFB : register(b1) { uint4 mmm[4]; uint4 fbm[4]; uint4 ctrl[4]; float4 bias[4]; };
RWTexture2D<float4> O : register(u0);
float clampOf(uint c, uint base, float2 uv) {
    uint W = (c >> 16) & 255, H = c >> 24; float fw = W, fh = H;
    float bx = min(fw * (1.0 / 256), 0.5), by = min(fh * (1.0 / 256), 0.5);
    uint x0, y0, x1, y1;
    [branch] if ((c & 0xc000) != 0) {
        float u = frac(uv.x) * fw, v = frac(uv.y) * fh;
        uint a = (uint)(u + bx), b = (uint)(v + by), e = (uint)(fw - bx + u), f = (uint)(fh - by + v);
        x0 = min(a, a - W); y0 = min(b, b - H); x1 = min(e, e - W); y1 = min(f, f - H);
    } else {
        float u = saturate(uv.x) * fw, v = saturate(uv.y) * fh;
        x0 = min((uint)(u + bx), W - 1); y0 = min((uint)(v + by), H - 1);
        x1 = (uint)max(u - bx, 0); y1 = (uint)max(v - by, 0);
    }
    uint i00 = W * y0 + x0 + 3 + base, i01 = W * y0 + x1 + 3 + base, i10 = W * y1 + x0 + 3 + base, i11 = W * y1 + x1 + 3 + base;
    uint v00 = MinMip[i00], v01, v10, v11;
#if DEDUPE == 2
    float m = (float)v00;
    [branch] if (i01 != i00 || i10 != i00 || i11 != i00) { m = max(max(max(m, (float)MinMip[i01]), (float)MinMip[i10]), (float)MinMip[i11]); }
    return m;
#elif DEDUPE
    [branch] if (i01 != i00 || i10 != i00 || i11 != i00) { v01 = MinMip[i01]; v10 = MinMip[i10]; v11 = MinMip[i11]; }
    else { v01 = v00; v10 = v00; v11 = v00; }
#else
    v01 = MinMip[i01]; v10 = MinMip[i10]; v11 = MinMip[i11];
#endif
    return max(max(max((float)v00, (float)v01), (float)v10), (float)v11);
}
[numthreads(64, 1, 1)] void main(uint3 id : SV_DispatchThreadID) {
    float2 uv = float2(id.x, id.y) * 0.001;
    float4 a = T0.SampleLevel(S, uv, clampOf(ctrl[0].x, mmm[0].x, uv));
    float4 b = T1.SampleLevel(S, uv * 2, clampOf(ctrl[0].y, mmm[0].y, uv * 2));
    float4 c = T2.SampleLevel(S, uv * 3, clampOf(ctrl[0].z, mmm[0].z, uv * 3));
    O[id.xy] = a + b * c;
}

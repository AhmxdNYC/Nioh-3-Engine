r"""mkcompose.py -- build the composite replacements from the mechanical translation (dxbc2hlsl.py) of the shipped
04c1052d / e6efc8ff, with only the chromatic-aberration loop rewritten (4 taps per pass, samples issued together).
Usage: py -I mkcompose.py   -> src\gen\<digest>.hlsl (gitignored), src\Compose_CA_fast.hlsl, src\Compose_CA2_fast.hlsl
"""
import os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "src")

OLD = """        [loop] while (true)
        {
            r5.w = (asint(r4.w) >= asint(r2.w)) ? 0xffffffffu : 0u;
            if (r5.w != 0u) break;
            r10.xy = asuint((asfloat(r4.xy) + asfloat(r10.xy)));
            r11.xyz = asuint((g_tSceneMap.SampleLevel(sampleLinear, asfloat(r10.xy), asfloat(0x00000000))).xyz);
            r11.xyz = asuint(min(asfloat(r11.xyz), float3(asfloat(0x477e0000), asfloat(0x477e0000), asfloat(0x477e0000))));
            r5.w = asuint((float)asint(r4.w));
            r5.w = asuint((asfloat(r5.w) / asfloat(r3.w)));
            r6.w = (asfloat(r5.w) < asfloat(0x3f000000)) ? 0xffffffffu : 0u;
            if (r6.w != 0u)
            {
                r6.w = asuint((asfloat(r5.w) + asfloat(r5.w)));
                r12.xyz = asuint(mad(asfloat(r6.www), asfloat(r6.xyz), asfloat(icb[asint(r1.w) + 0].xyz)));
            }
            else
            {
                r5.w = asuint(mad(asfloat(r5.w), asfloat(0x40000000), asfloat(0xbf800000)));
                r12.xyz = asuint(mad(asfloat(r5.www), asfloat(r7.xyz), asfloat(icb[asint(r4.z) + 0].xyz)));
            }
            r8.xyz = asuint(mad(asfloat(r11.xyz), asfloat(r12.xyz), asfloat(r8.xyz)));
            r9.xyz = asuint((asfloat(r9.xyz) + asfloat(r12.xyz)));
            r4.w = asuint((asint(r4.w) + 1));
        }
"""

# Chromatic aberration: taps i = 1 .. n-1 (n = clamp(|offset| px, 3, 16)) along a fixed UV step. The shipped loop
# waits for every tap's sample. Here a pass takes 4 taps while 4 remain: the 4 positions are advanced with the
# original add (step + pos, one tap after the other), the 4 samples are issued together, then each tap's
# weight/accumulate runs with the original ops in the original order. The last 1-3 taps run one per pass as shipped
# (no extra fetches); the loop counter / position end exactly where the original's do.
NEW = """        [loop] while (true)
        {
            // [EDITED] chromatic-aberration loop: 4 taps per pass while 4 remain (see mkcompose.py)
            int i0 = asint(r4.w);
            if (i0 >= asint(r2.w)) break;
            int nt = (i0 + 3 < asint(r2.w)) ? 4 : 1;
            float2 pp[4];
            float2 pos = asfloat(r10.xy);
            [unroll] for (int k = 0; k < 4; k++)
            {
                pos = asfloat(r4.xy) + pos;
                pp[k] = pos;
            }
            float3 cs[4];
            if (nt == 4)
            {
                [unroll] for (int k1 = 0; k1 < 4; k1++)
                    cs[k1] = g_tSceneMap.SampleLevel(sampleLinear, pp[k1], asfloat(0x00000000)).xyz;
            }
            else
            {
                cs[0] = g_tSceneMap.SampleLevel(sampleLinear, pp[0], asfloat(0x00000000)).xyz;
                cs[1] = cs[2] = cs[3] = 0;
            }
            float3 acc = asfloat(r8.xyz), wsum = asfloat(r9.xyz);
            [unroll] for (int k2 = 0; k2 < 4; k2++)
            {
                if (k2 < nt)
                {
                    float3 c = min(cs[k2], float3(asfloat(0x477e0000), asfloat(0x477e0000), asfloat(0x477e0000)));
                    float f = (float)(i0 + k2);
                    f = f / asfloat(r3.w);
                    float3 w;
                    if (f < asfloat(0x3f000000))
                        w = mad((f + f).xxx, asfloat(r6.xyz), asfloat(icb[asint(r1.w) + 0].xyz));
                    else
                        w = mad(mad(f, asfloat(0x40000000), asfloat(0xbf800000)).xxx, asfloat(r7.xyz), asfloat(icb[asint(r4.z) + 0].xyz));
                    acc = mad(c, w, acc);
                    wsum = wsum + w;
                }
            }
            r8.xyz = asuint(acc);
            r9.xyz = asuint(wsum);
            r10.xy = asuint(nt == 4 ? pp[3] : pp[0]);
            r4.w = asuint(i0 + nt);
        }
"""


MV2W_OLD = """        r2.w = asuint((asfloat(r3.y) * asfloat(asuint(g_mV2W[1]).y)));
        r2.w = asuint(mad(asfloat(asuint(g_mV2W[0]).y), asfloat(r3.x), asfloat(r2.w)));
        r2.w = asuint(mad(asfloat(asuint(g_mV2W[2]).y), asfloat(r0.w), asfloat(r2.w)));
        r2.w = asuint((asfloat(r2.w) + asfloat(asuint(g_mV2W[3]).y)));
"""
MV2W_NEW = """        r2.w = asuint(mul(g_mV2W, float4(asfloat(r3.x), asfloat(r3.y), asfloat(r0.w), 1.0)).y);  // [EDITED] see mkcompose.py
"""


def main():
    for dig, out in (("04c1052d", "Compose_CA_fast.hlsl"), ("e6efc8ff", "Compose_CA2_fast.hlsl")):
        gen = os.path.join(SRC, "gen", dig + ".hlsl")
        os.makedirs(os.path.dirname(gen), exist_ok=True)
        subprocess.run([sys.executable, "-I", os.path.join(HERE, "dxbc2hlsl.py"), dig, gen], check=True)
        s = open(gen).read()
        if s.count(OLD) != 1:
            raise SystemExit("%s: CA loop not found" % dig)
        s = s.replace(OLD, NEW)
        # fxc reorders the translated mul/mad chain of the view->world row; the original came from a matrix multiply,
        # which fxc compiles in the original order.
        mv = MV2W_OLD
        if mv in s:
            s = s.replace("    float4 g_mV2W[4];", "    float4x4 g_mV2W;").replace(mv, MV2W_NEW)
        s = s.replace("// mechanical translation of the shipped DXBC (dxbc2hlsl.py); edit only where marked",
                      "// Composite (replaces runtime PS %s): mechanical translation of the shipped DXBC (dxbc2hlsl.py)\n"
                      "// with only the chromatic-aberration loop rewritten ([EDITED], built by mkcompose.py)." % dig)
        open(os.path.join(SRC, out), "w").write(s)
        print(dig, "->", out)


if __name__ == "__main__":
    main()

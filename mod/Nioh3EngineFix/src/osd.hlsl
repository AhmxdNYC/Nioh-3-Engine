// osd.hlsl - shaders of the in-game overlay (osd.cpp). Compiled offline to DXBC (SM 5.0) into osd_shaders.h:
//   tools\gen_osd_shaders.bat
// One instanced triangle strip per quad. Each instance is a glyph (8x16 cell + 1 texel outline border), a solid
// rectangle (glyph = 0xFFFF) or a sprite (glyph = 0xFFFE: the controller display's atlas t0, premultiplied alpha, mips).
// The font lives in a constant buffer (glyph ranges + normalized stroke segments).

#define FONT_W 8
#define FONT_H 16
#define FONT_GLYPHS 95
#define SOLID 0xFFFF
#define SPRITE 0xFFFE

cbuffer Font : register(b0) {
    float4 g_font[576]; // 95 glyph ranges + 481 approved stroke segments
};

Texture2D g_atlas : register(t0);
SamplerState g_linear : register(s0);

cbuffer Consts : register(b1) {
    float2 g_invScreen;  // 1 / back buffer size (pixels)
    float g_scale;       // pixels per font texel
    float g_bright;      // colour multiplier (HDR back buffers)
};

struct VSIn {
    float4 rect : RECT;    // x, y, w, h in pixels (top-left origin)
    uint glyph : GLYPH;    // 0..94 = ASCII 32..126, 0xFFFF = solid
    uint color : COLOR;    // 0xAABBGGRR
    float4 uv : UV;        // sprite: u0, v0, u1, v1 (unused otherwise)
    uint vid : SV_VertexID;
};

struct VSOut {
    float4 pos : SV_Position;
    float2 local : TEXCOORD0;  // pixel offset inside the quad
    float2 uv : TEXCOORD1;     // sprite texture coordinate
    nointerpolation uint glyph : GLYPH;
    nointerpolation float4 color : COLOR;
};

VSOut vs_main(VSIn i) {
    VSOut o;
    float2 corner = float2((float)(i.vid & 1), (float)(i.vid >> 1));
    float2 px = i.rect.xy + corner * i.rect.zw;
    o.pos = float4(px.x * g_invScreen.x * 2.0 - 1.0, 1.0 - px.y * g_invScreen.y * 2.0, 0.0, 1.0);
    o.local = corner * i.rect.zw;
    o.uv = lerp(i.uv.xy, i.uv.zw, corner);
    o.glyph = i.glyph;
    o.color = float4((float)(i.color & 255), (float)((i.color >> 8) & 255), (float)((i.color >> 16) & 255),
                     (float)(i.color >> 24)) / 255.0;
    return o;
}

float4 ps_main(VSOut i) : SV_Target {
    float2 gx = ddx(i.uv), gy = ddy(i.uv);  // outside the branches (gradients in flow control)
    if (i.glyph == SOLID) return float4(i.color.rgb * g_bright, i.color.a);
    if (i.glyph == SPRITE) {
        float4 t = g_atlas.SampleGrad(g_linear, i.uv, gx, gy);  // premultiplied
        if (t.a <= 0.002) discard;
        return float4(t.rgb / t.a * i.color.rgb * g_bright, t.a * i.color.a);
    }
    // Analytic round-capped strokes in output pixels: no enlarged 8x16 bitmap.
    float2 p = i.local - g_scale;
    uint first = (uint)g_font[i.glyph].x, count = (uint)g_font[i.glyph].y;
    float d = 1e6;
    [loop] for (uint j = 0; j < count; ++j) {
        float4 s = g_font[95 + first + j];
        float2 a = (float2(1.1,2.2) + s.xy * float2(5.8,10.0)) * g_scale;
        float2 b = (float2(1.1,2.2) + s.zw * float2(5.8,10.0)) * g_scale;
        float2 v = b-a;
        float t = saturate(dot(p-a,v)/max(dot(v,v),1e-8));
        d = min(d,length(p-a-t*v));
    }
    float radius = max(0.65,0.62*g_scale);
    float c = saturate(radius + 0.5 - d);
    float outline = saturate(radius + 1.2 - d)*0.70;
    float a = c + outline*(1.0-c);
    if(a<=0.004) discard;
    return float4(i.color.rgb*(c/a)*g_bright,a*i.color.a);
}

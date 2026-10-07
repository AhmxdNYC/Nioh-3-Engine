// osd.hlsl - shaders of the in-game overlay (osd.cpp). Compiled offline to DXBC (SM 5.0) into osd_shaders.h:
//   tools\gen_osd_shaders.bat
// One instanced triangle strip per quad. Each instance is either a glyph (8x16 cell + 1 texel outline border) or a
// solid rectangle (glyph = 0xFFFF). The font lives in a constant buffer (4-bit coverage, one uint per glyph row).

#define FONT_W 8
#define FONT_H 16
#define FONT_GLYPHS 95
#define SOLID 0xFFFF

cbuffer Font : register(b0) {
    uint4 g_font[FONT_GLYPHS * FONT_H / 4];  // one uint per row -> 4 rows per uint4
};

cbuffer Consts : register(b1) {
    float2 g_invScreen;  // 1 / back buffer size (pixels)
    float g_scale;       // pixels per font texel
    float g_bright;      // colour multiplier (HDR back buffers)
};

struct VSIn {
    float4 rect : RECT;    // x, y, w, h in pixels (top-left origin)
    uint glyph : GLYPH;    // 0..94 = ASCII 32..126, 0xFFFF = solid
    uint color : COLOR;    // 0xAABBGGRR
    uint vid : SV_VertexID;
};

struct VSOut {
    float4 pos : SV_Position;
    float2 local : TEXCOORD0;  // pixel offset inside the quad
    nointerpolation uint glyph : GLYPH;
    nointerpolation float4 color : COLOR;
};

VSOut vs_main(VSIn i) {
    VSOut o;
    float2 corner = float2((float)(i.vid & 1), (float)(i.vid >> 1));
    float2 px = i.rect.xy + corner * i.rect.zw;
    o.pos = float4(px.x * g_invScreen.x * 2.0 - 1.0, 1.0 - px.y * g_invScreen.y * 2.0, 0.0, 1.0);
    o.local = corner * i.rect.zw;
    o.glyph = i.glyph;
    o.color = float4((float)(i.color & 255), (float)((i.color >> 8) & 255), (float)((i.color >> 16) & 255),
                     (float)(i.color >> 24)) / 255.0;
    return o;
}

float cov(uint g, int x, int y) {
    if (x < 0 || y < 0 || x >= FONT_W || y >= FONT_H) return 0.0;
    uint row = g * FONT_H + (uint)y;
    uint4 v = g_font[row >> 2];
    uint w = v[row & 3];
    return (float)((w >> ((uint)x * 4)) & 15) / 15.0;
}

float4 ps_main(VSOut i) : SV_Target {
    if (i.glyph == SOLID) return float4(i.color.rgb * g_bright, i.color.a);
    // texel inside the cell; the quad has a 1-texel border on every side for the outline
    int2 t = int2(floor(i.local / g_scale)) - 1;
    float c = cov(i.glyph, t.x, t.y);
    float n = 0.0;
    [unroll] for (int dy = -1; dy <= 1; ++dy)
        [unroll] for (int dx = -1; dx <= 1; ++dx)
            n = max(n, cov(i.glyph, t.x + dx, t.y + dy));
    float outline = saturate(n * 1.5) * 0.85;
    float a = c + outline * (1.0 - c);
    if (a <= 0.004) discard;
    float3 rgb = i.color.rgb * (c / a);  // text over a black outline
    return float4(rgb * g_bright, a * i.color.a);
}

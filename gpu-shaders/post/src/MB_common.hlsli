// Interface of the game's runtime-compiled motion-blur shaders (cbMotionBlur2Info @ b3, samplers s7/s8).
cbuffer cbMotionBlur2Info : register(b3)
{
    float4 g_cbMbParams0;
    float4 g_cdTileParam0;
    float4 g_cbScreenInfo;
    float4 g_cdTileInfo;
    float4 g_cbTimeInfo;
    float4 g_cdD2Z_info;
    float4 g_cdTaa2UvParam;
    float4 g_cbMaxUV;
};
SamplerState sampleLinear : register(s7);
SamplerState samplePoint : register(s8);

// Interface of the game's runtime-compiled DOF shaders (cbDof @ b2, sampleLinear @ s7).
cbuffer cbDof : register(b2)
{
    float4 g_vDofInfo0;
    float4 g_vDofInfo1;
    float4 g_vDofInfo2;
    float4 g_vD2Z_Z2D;
    float4 g_vTexelSize;
    float4 g_vAnamorphicInfo;
    float4 g_vSampleInfo;
    float4 g_vHeatSimmerInfo;
    float4 g_vMaxUV;
};
SamplerState sampleLinear : register(s7);
Texture2D g_t4MainMap : register(t0);
Texture2D g_tDofCocMap : register(t1);
Texture2D g_tSpritesDofForeAccum : register(t2);
Texture2D g_tSpritesDofRearAccum : register(t3);
Texture2D g_tSampleInfo : register(t4);

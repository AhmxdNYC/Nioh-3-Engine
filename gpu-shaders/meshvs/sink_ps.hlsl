struct I { float4 p:SV_Position; float4 a:TEXCOORD0; float4 b:TEXCOORD1; float3 c:TEXCOORD2; float3 d:TEXCOORD3; float2 e:TEXCOORD4; nointerpolation uint f:TEXCOORD5; };
float4 main(I i):SV_Target0 { return i.a+i.b+float4(i.c+i.d,0)+float4(i.e,0,asfloat(i.f)); }

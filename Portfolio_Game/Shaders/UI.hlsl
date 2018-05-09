//***************************************************************************************
// Default.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

#include "Common.hlsl"

struct VertexIn
{
	float3 PosL    : POSITION;
	float3 NormalL : NORMAL;
	float2 TexC    : TEXCOORD;
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
	float3 PosW    : POSITION;
	float3 NormalW : NORMAL;
	float2 TexC    : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut)0.0f;

#ifdef MONSTER
	// Transform to world space.
	float4 posW = mul(float4(vin.PosL, 1.0f), gMonsterUIWorld);
	vout.PosW = posW.xyz;

	// Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
	vout.NormalW = mul(vin.NormalL, (float3x3)gMonsterUIWorld);

	// Output vertex attributes for interpolation across triangle.
	float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gMonsterUITexTransform);
#else
	float4 posW = mul(float4(vin.PosL, 1.0f), gUIWorld);
	vout.PosW = posW.xyz;
	vout.NormalW = mul(vin.NormalL, (float3x3)gUIWorld);

	float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gUITexTransform);
#endif

	vout.TexC = mul(texC, gMatTransform).xy;
	// Transform to homogeneous clip space.
	vout.PosH = mul(posW, gViewProj);
	
	return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	float4 diffuseAlbedo = gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC) * gDiffuseAlbedo;

	float4 litColor = diffuseAlbedo;

	float distanceToEye = length(gEyePosW - pin.PosW);
	float fogAmount = saturate((distanceToEye - gFogStart) / gFogRange);
	litColor = lerp(litColor, gFogColor, fogAmount);

	if (diffuseAlbedo.x == 0.0f &&
		diffuseAlbedo.y == 0.0f &&
			diffuseAlbedo.z == 0.0f)
	{
		discard;
	}

	return litColor;
}
//***************************************************************************************
// Default.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

#include "Common.hlsl"

struct VertexIn
{
	float3 PosL    : POSITION;
	float3 NormalL : NORMAL;
	float2 TexC    : TEXCOORD;
	float3 TangentL : TANGENT;
	float3 BinormalL : BINORMAL;
#ifdef SKINNED
	float3 BoneWeights : WEIGHTS;
	uint4 BoneIndices  : BONEINDICES;
#endif
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
	float3 PosW    : POSITION;
	float3 NormalW : NORMAL;
	float2 TexC    : TEXCOORD;
	float3 TangentW : TANGENT;
	float3 BinormalW : BINORMAL;
#ifdef SKINNED
	uint4 BoneIndices : BONEINDICES;
#endif
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut)0.0f;

#ifdef SKINNED
	float weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	weights[0] = vin.BoneWeights.x;
	weights[1] = vin.BoneWeights.y;
	weights[2] = vin.BoneWeights.z;
	weights[3] = 1.0f - weights[0] - weights[1] - weights[2];

	float3 posL = float3(0.0f, 0.0f, 0.0f);
	float3 normalL = float3(0.0f, 0.0f, 0.0f);
	float3 tangentL = float3(0.0f, 0.0f, 0.0f);
	float3 binormalL = float3(0.0f, 0.0f, 0.0f);
	for (int i = 0; i < 4; ++i)
	{
		// Assume no nonuniform scaling when transforming normals, so 
		// that we do not have to use the inverse-transpose.
		posL += weights[i] * mul(float4(vin.PosL, 1.0f), gBoneTransforms[vin.BoneIndices[i]]).xyz;
		normalL += weights[i] * mul(vin.NormalL, (float3x3)gBoneTransforms[vin.BoneIndices[i]]);
		tangentL += weights[i] * mul(vin.TangentL, (float3x3)gBoneTransforms[vin.BoneIndices[i]]);
		binormalL += weights[i] * mul(vin.BinormalL, (float3x3)gBoneTransforms[vin.BoneIndices[i]]);
	}

	vin.PosL = posL;
	vin.NormalL = normalL;
	vin.TangentL = tangentL;
	vin.BinormalL = binormalL;

	vout.BoneIndices = vin.BoneIndices;

	float4 posW = mul(float4(vin.PosL, 1.0f), gChaWorld);
	vout.PosW = posW.xyz;

	vout.NormalW = mul(vin.NormalL, (float3x3)gChaWorld);
	vout.TangentW = mul(vin.TangentL, (float3x3)gChaWorld);
	vout.BinormalW = mul(vin.BinormalL, (float3x3)gChaWorld);

	vout.NormalW = normalize(vout.NormalW);
	vout.TangentW = normalize(vout.TangentW);
	vout.BinormalW = normalize(vout.BinormalW);

	float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gChaTexTransform);
#elif UI
	// Transform to world space.
	float4 posW = mul(float4(vin.PosL, 1.0f), gUIWorld);
	vout.PosW = posW.xyz;

	// Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
	vout.NormalW = mul(vin.NormalL, (float3x3)gUIWorld);
	vout.TangentW = mul(vin.TangentL, (float3x3)gUIWorld);
	vout.BinormalW = mul(vin.BinormalL, (float3x3)gUIWorld);

	// Output vertex attributes for interpolation across triangle.
	float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gUITexTransform);
#else
	// Transform to world space.
	float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
	vout.PosW = posW.xyz;

	// Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
	vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);
	vout.TangentW = mul(vin.TangentL, (float3x3)gWorld);
	vout.BinormalW = mul(vin.BinormalL, (float3x3)gWorld);

	// Output vertex attributes for interpolation across triangle.
	float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
#endif

	vout.TexC = mul(texC, gMatTransform).xy;
	
	// Transform to homogeneous clip space.
	vout.PosH = mul(posW, gViewProj);
	
	return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	float4 diffuseAlbedo = gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC) * gDiffuseAlbedo;
	//float4 diffuseAlbedo = float4(231.0f / 255.0f, 221.0f / 255.0f, 255.0f / 255.0f, 1.0f);
	float4 normalMap = gNormalMap.Sample(gsamAnisotropicWrap, pin.TexC);
	// 0.0f ~ 1.0f -> -1.0f ~ 1.0f
	normalMap = (normalMap * 2.0f) - 1.0f;

	
	float3 normal =  normalMap.x * pin.TangentW + normalMap.y * pin.BinormalW +  normalMap.z * pin.NormalW;
	pin.NormalW = normalize(normal);
	//diffuseAlbedo = float4(pin.NormalW, 1.0f);

	float3 toEyeW = gEyePosW - pin.PosW;
	float distanceToEye = length(toEyeW);
	toEyeW /= distanceToEye; // Normalize

	float4 ambient = gAmbientLight * diffuseAlbedo;

	const float shininess = 1.0f - gRoughness;
	Material mat = { diffuseAlbedo, gFresnelR0, shininess };
	float3 shadowFactor = 1.0f;
	float4 directLight = ComputeLighting(gLights, mat, pin.PosW,
		pin.NormalW, toEyeW, shadowFactor);

	float4 litColor = ambient + directLight;

	float fogAmount = saturate((distanceToEye - gFogStart) / gFogRange);
	litColor = lerp(litColor, gFogColor, fogAmount);

	litColor.a = diffuseAlbedo.a;

	if (diffuseAlbedo.a == 0.0f)
	{
		discard;
	}

	return litColor;
}
#pragma once

#include <fbxsdk.h>
#include "SkinnedData.h"

struct BoneIndexAndWeight
{
	BYTE mBoneIndices;
	float mBoneWeight;

	bool operator < (const BoneIndexAndWeight& rhs)
	{
		return (mBoneWeight > rhs.mBoneWeight);
	}
};

struct CtrlPoint
{
	DirectX::XMFLOAT3 mPosition;
	std::vector<BoneIndexAndWeight> mBoneInfo;
	std::string mBoneName;

	CtrlPoint()
	{
		mBoneInfo.reserve(4);
	}

	void SortBlendingInfoByWeight()
	{
		std::sort(mBoneInfo.begin(), mBoneInfo.end());
	}
};

class FbxLoader
{
public:
	FbxLoader();
	~FbxLoader();

	HRESULT LoadFBX(
		std::vector<CharacterVertex>& outVertexVector,
		std::vector<uint32_t>& outIndexVector,
		SkinnedData& outSkinnedData,
		const std::string& ClipName,
		std::vector<Material>& outMaterial,
		std::string fileName);

	HRESULT LoadFBX(
		std::vector<Vertex>& outVertexVector, 
		std::vector<uint32_t>& outIndexVector, 
		std::vector<Material>& outMaterial,
		std::string fileName);

	HRESULT LoadFBX(
		SkinnedData& outSkinnedData, 
		const std::string& clipName,
		std::string fileName);

	bool LoadTXT(
		std::vector<CharacterVertex>& outVertexVector,
		std::vector<uint32_t>& outIndexVector,
		SkinnedData& outSkinnedData, 
		const std::string& clipName, 
		std::vector<Material>& outMaterial,
		std::string fileName);

	bool LoadTXT(
		std::vector<Vertex>& outVertexVector,
		std::vector<uint32_t>& outIndexVector,
		std::vector<Material>& outMaterial,
		std::string fileName);

	bool LoadAnimationTXT(
		AnimationClip& animation,
		const std::string& clipName, 
		std::string fileName);


	void GetSkeletonHierarchy(
		fbxsdk::FbxNode * pNode, 
		SkinnedData& outSkinnedData,
		int curIndex, int parentIndex);

	void GetControlPoints(fbxsdk::FbxNode * pFbxRootNode);

	void GetAnimation(
		fbxsdk::FbxScene * pFbxScene,
		fbxsdk::FbxNode * pFbxChildNode,
		std::string & outAnimationName, 
		const std::string& ClipName);

	void GetOnlyAnimation(
		fbxsdk::FbxScene* pFbxScene,
		fbxsdk::FbxNode * pFbxChildNode,
		AnimationClip& animation, 
		const std::string clipName);

	void GetVerticesAndIndice(
		fbxsdk::FbxMesh * pMesh,
		std::vector<CharacterVertex> & outVertexVector,
		std::vector<uint32_t> & outIndexVector, 
		SkinnedData& outSkinnedData);

	void GetVerticesAndIndice(
		fbxsdk::FbxMesh * pMesh,
		std::vector<Vertex>& outVertexVector, 
		std::vector<uint32_t>& outIndexVector);


	void GetMaterials(fbxsdk::FbxNode * pNode, std::vector<Material>& outMaterial);

	void GetMaterialAttribute(fbxsdk::FbxSurfaceMaterial* pMaterial, Material& outMaterial);
	
	void GetMaterialTexture(fbxsdk::FbxSurfaceMaterial * pMaterial, Material & Mat);

	FbxAMatrix GetGeometryTransformation(fbxsdk::FbxNode * pNode);


	void ExportFBX(
		std::vector<CharacterVertex>& outVertexVector,
		std::vector<uint32_t>& outIndexVector,
		SkinnedData& outSkinnedData,
		const std::string& clipName, 
		std::vector<Material>& outMaterial, 
		std::string fileName);

	void ExportFBX(
		std::vector<Vertex>& outVertexVector, 
		std::vector<uint32_t>& outIndexVector,
		std::vector<Material>& outMaterial,
		std::string fileName);

	void ExportAnimation(
		const AnimationClip& animation,
		std::string fileName, 
		const std::string& clipName);

	void clear();

private:
	std::unordered_map<unsigned int, CtrlPoint*> mControlPoints;
	std::vector<std::string> mBoneName;
	
	// skinnedData Output
	std::vector<int> mBoneHierarchy;
	std::vector<DirectX::XMFLOAT4X4> mBoneOffsets;
	std::unordered_map<std::string, AnimationClip> mAnimations;
};
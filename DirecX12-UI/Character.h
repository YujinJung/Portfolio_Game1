#pragma once

#include "SkinnedData.h"
#include "RenderItem.h"
#include "Materials.h"
#include "../Common/UploadBuffer.h"

struct SkinnedConstants;
struct FrameResource;
class GameTimer;

class Character
{
public:
	Character();
	~Character();

	UINT GetCharacterMeshSize() const;
	UINT GetAllRitemsSize() const;
	UINT GetBoneSize() const;
	const std::vector<RenderItem*> GetRenderItem(RenderLayer Type);

	void BuildConstantBufferViews(ID3D12Device* device, ID3D12DescriptorHeap* mCbvHeap, const std::vector<std::unique_ptr<FrameResource>> &mFrameResources, int gNumFrameResources, int mChaCbvOffset);
	void BuildGeometry(ID3D12Device * device, ID3D12GraphicsCommandList* cmdList, const std::vector<SkinnedVertex>& inVertices, const std::vector<std::uint16_t>& inIndices, const SkinnedData& inSkinInfo);
	void BuildRenderItem(Materials& mMaterials);
	
	void UpdateCharacterCBs(UploadBuffer<SkinnedConstants>* currSkinnedCB, const Light& mMainLight, const GameTimer& gt);
	void UpdateCharacterShadows(const Light& mMainLight);

private:
	SkinnedData mSkinnedInfo;
	std::unique_ptr<MeshGeometry> mGeometry;
	std::unique_ptr<SkinnedModelInstance> mSkinnedModelInst;

	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::vector<RenderItem*> mRitems[(int)RenderLayer::Count];

};


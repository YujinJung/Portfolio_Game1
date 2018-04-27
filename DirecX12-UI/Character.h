#pragma once

#include "Materials.h"
#include "../Common/UploadBuffer.h"
#include "RenderItem.h"

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
	DirectX::XMFLOAT4X4 GetWorld() const;
	const std::vector<RenderItem*> GetRenderItem(RenderLayer Type) const;

	void SetWorldTransform(DirectX::XMMATRIX inWorldTransform);

	void BuildConstantBufferViews(ID3D12Device* device, ID3D12DescriptorHeap* mCbvHeap, const std::vector<std::unique_ptr<FrameResource>> &mFrameResources, int mChaCbvOffset);
	void BuildGeometry(ID3D12Device * device, ID3D12GraphicsCommandList* cmdList, const std::vector<SkinnedVertex>& inVertices, const std::vector<std::uint16_t>& inIndices, const SkinnedData& inSkinInfo);
	void BuildRenderItem(Materials& mMaterials);
	
	virtual void UpdateCharacterCBs(FrameResource* mCurrFrameResource, const Light& mMainLight, const GameTimer & gt);
	void UpdateCharacterShadows(const Light& mMainLight);

	bool mTransformDirty = false;
	
private:
	SkinnedData mSkinnedInfo;
	std::unique_ptr<MeshGeometry> mGeometry;
	std::unique_ptr<SkinnedModelInstance> mSkinnedModelInst;
	
	DirectX::XMFLOAT4X4 mWorldTransform;

private:
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::vector<RenderItem*> mRitems[(int)RenderLayer::Count];
};


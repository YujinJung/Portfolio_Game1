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
	eClipList GetCurrentClip() const;
	float GetCurrentClipTime() const;
	bool isClipEnd(const std::string& clipName) const;
	const std::vector<RenderItem*> GetRenderItem(RenderLayer Type) const;

	void SetClipTime(float time);
	void SetClipName(const std::string& inClipName);
	void SetWorldTransform(DirectX::XMMATRIX inWorldTransform);

	virtual void BuildConstantBufferViews(ID3D12Device* device, ID3D12DescriptorHeap* mCbvHeap, const std::vector<std::unique_ptr<FrameResource>> &mFrameResources, int mChaCbvOffset) = 0;
	void BuildGeometry(ID3D12Device * device, ID3D12GraphicsCommandList* cmdList, const std::vector<SkinnedVertex>& inVertices, const std::vector<std::uint32_t>& inIndices, const SkinnedData& inSkinInfo, std::string geoName);
	void BuildRenderItem(Materials& mMaterials, std::string matrialPrefix, RenderLayer type);
	
	void UpdateCharacterCBs(std::unique_ptr<UploadBuffer<SkinnedConstants>> &currCharacter, const Light& mMainLight, RenderLayer type, const std::string& clipName, const GameTimer & gt);
	void UpdateCharacterShadows(const Light& mMainLight, RenderLayer type);

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


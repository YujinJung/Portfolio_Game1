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

	UINT GetBoneSize() const;
	UINT GetAllRitemsSize() const;
	UINT GetCharacterMeshSize() const;
	UINT GetNumOfCharacter() const;

	eClipList GetCurrentClip() const;
	float GetCurrentClipTime() const;
	WorldTransform& GetWorldTransform(int i = 0) ;
	DirectX::XMFLOAT4X4 GetWorldTransform4x4f(int i = 0) const;

	bool isClipEnd(const std::string& clipName) const;
	const std::vector<RenderItem*> GetRenderItem(RenderLayer Type) const;

	void SetClipTime(float time);
	void SetWorldTransform(const WorldTransform& inWorldTransform, const int& i);

	virtual void BuildConstantBufferViews(ID3D12Device* device, ID3D12DescriptorHeap* mCbvHeap, const std::vector<std::unique_ptr<FrameResource>> &mFrameResources, int mChaCbvOffset) = 0;
	void BuildGeometry(ID3D12Device * device, ID3D12GraphicsCommandList* cmdList, const std::vector<SkinnedVertex>& inVertices, const std::vector<std::uint32_t>& inIndices, const SkinnedData& inSkinInfo, std::string geoName);
	void BuildRenderItem(Materials& mMaterials, std::string matrialPrefix, RenderLayer type);
	
	void UpdateCharacterCBs(std::unique_ptr<UploadBuffer<SkinnedConstants>> &currCharacter, const Light& mMainLight, RenderLayer type, const std::string& clipName, const GameTimer & gt);
	void UpdateCharacterShadows(const Light& mMainLight, RenderLayer type);

public:
	bool mTransformDirty = false;
	
protected:
	UINT numOfCharacter;
	
private:
	SkinnedData mSkinnedInfo;
	std::unique_ptr<MeshGeometry> mGeometry;
	std::unique_ptr<SkinnedModelInstance> mSkinnedModelInst;

private:
	std::vector<WorldTransform> mWorldTransform;
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::vector<RenderItem*> mRitems[(int)RenderLayer::Count];
};


#pragma once
#include "Character.h"

class Monster : public Character
{
public:
	Monster();
	~Monster();

	UINT GetBoneSize() const;
	UINT GetAllRitemsSize() const;
	UINT GetNumOfCharacter() const;
	WorldTransform & GetWorldTransform(int i);
	DirectX::XMFLOAT4X4 GetWorldTransform4x4f(int i) const;
	const std::vector<RenderItem*> GetRenderItem(RenderLayer Type) const;


	virtual void BuildGeometry(ID3D12Device * device, ID3D12GraphicsCommandList * cmdList, const std::vector<SkinnedVertex>& inVertices, const std::vector<std::uint32_t>& inIndices, const SkinnedData & inSkinInfo, std::string geoName) override;
	virtual void BuildConstantBufferViews(ID3D12Device * device, ID3D12DescriptorHeap * mCbvHeap, const std::vector<std::unique_ptr<FrameResource>>& mFrameResources, int mChaCbvOffset) override;
	virtual void BuildRenderItem(Materials & mMaterials, std::string matrialPrefix) override;

	void UpdateCharacterCBs(FrameResource* mCurrFrameResource, const Light& mMainLight, const GameTimer & gt);
	void UpdateMonsterPosition(DirectX::XMFLOAT3 inPlayerPosition, const GameTimer & gt);
	void UpdateCharacterShadows(const Light & mMainLight);

private:
	SkinnedData mSkinnedInfo;
	std::unique_ptr<MeshGeometry> mGeometry;
	std::vector<std::unique_ptr<SkinnedModelInstance>> mSkinnedModelInst;

	std::vector<WorldTransform> mWorldTransform;
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::vector<RenderItem*> mRitems[(int)RenderLayer::Count];

	std::vector<std::string> mClipName;
	DirectX::XMFLOAT3 mMonsterPosition;

	UINT numOfCharacter;
};


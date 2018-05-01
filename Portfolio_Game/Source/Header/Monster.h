#pragma once

#include "DXUI.h"
#include "PlayerMovement.h"
#include "Character.h"

class Monster : public Character
{
public:
	Monster();
	~Monster();

	DXUI mUI;

public:
	UINT GetBoneSize() const;
	UINT GetAllRitemsSize() const;
	UINT GetNumOfCharacter() const;
	virtual WorldTransform & GetWorldTransform(int i);
	DirectX::XMFLOAT4X4 GetWorldTransform4x4f(int i) const;
	const std::vector<RenderItem*> GetRenderItem(RenderLayer Type) const;

public:
	bool isClipEnd(std::string clipName, int i);
	bool isClipMid(std::string clipName, int i);
	virtual int GetHealth() const override;
	virtual void Damage(int damage, DirectX::XMFLOAT3 Position, DirectX::XMFLOAT3 Look, int cIndex) override;

	void SetClipName(const std::string & inClipName, int cIndex);

public:
	virtual void BuildGeometry(ID3D12Device * device, ID3D12GraphicsCommandList * cmdList, const std::vector<SkinnedVertex>& inVertices, const std::vector<std::uint32_t>& inIndices, const SkinnedData & inSkinInfo, std::string geoName) override;
	virtual void BuildConstantBufferViews(ID3D12Device * device, ID3D12DescriptorHeap * mCbvHeap, const std::vector<std::unique_ptr<FrameResource>>& mFrameResources, int mChaCbvOffset) override;
	virtual void BuildRenderItem(Materials & mMaterials, std::string matrialPrefix) override;

	void UpdateCharacterCBs(FrameResource* mCurrFrameResource, const Light& mMainLight, const GameTimer & gt);
	void UpdateMonsterPosition(Character& Player, const GameTimer & gt);
	void UpdateCharacterShadows(const Light & mMainLight);

private:
	PlayerMovement mMonsterMovement;
	
	SkinnedData mSkinnedInfo;
	std::unique_ptr<MeshGeometry> mGeometry;
	std::vector<std::unique_ptr<SkinnedModelInstance>> mSkinnedModelInst;

	std::vector<WorldTransform> mWorldTransform;
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::vector<RenderItem*> mRitems[(int)RenderLayer::Count];

	std::vector<std::string> mClipName;
	DirectX::XMFLOAT3 mMonsterPosition;
	DirectX::XMFLOAT3 mMonsterLeft;

private:
	int mHealth;
	UINT mDamage;
	UINT numOfCharacter;
};


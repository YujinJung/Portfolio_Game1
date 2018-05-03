#pragma once

#include "MonsterUI.h"
#include "Character.h"

class MonsterUI;
class Monster : public Character
{
public:
	Monster();
	~Monster();

	MonsterUI monsterUI;
	UINT GetUISize() const;

public:
	UINT GetBoneSize() const;
	UINT GetAllRitemsSize() const;
	UINT GetNumOfCharacter() const;
	virtual WorldTransform GetWorldTransform(int i);
	DirectX::XMMATRIX GetWorldTransformMatrix(int i) const;
	const std::vector<RenderItem*> GetRenderItem(RenderLayer Type) const;
	virtual CharacterInfo& GetCharacterInfo(int cIndex = 0);

public:
	bool isClipEnd(std::string clipName, int i);
	bool isClipMid(std::string clipName, int i);
	virtual int GetHealth(int i = 0) const override;
	virtual bool Damage(int damage, DirectX::XMVECTOR Position, DirectX::XMVECTOR Look) override;

	void SetClipName(const std::string & inClipName, int cIndex);

public:
	virtual void BuildGeometry(ID3D12Device * device, ID3D12GraphicsCommandList * cmdList, const std::vector<SkinnedVertex>& inVertices, const std::vector<std::uint32_t>& inIndices, const SkinnedData & inSkinInfo, std::string geoName) override;
	virtual void BuildConstantBufferViews(ID3D12Device * device, ID3D12DescriptorHeap * mCbvHeap, const std::vector<std::unique_ptr<FrameResource>>& mFrameResources, int mChaCbvOffset) override;
	virtual void BuildRenderItem(Materials & mMaterials, std::string matrialPrefix) override;
	void BuildUIConstantBuffer(ID3D12Device * device, ID3D12DescriptorHeap * mCbvHeap, const std::vector<std::unique_ptr<FrameResource>>& mFrameResources, int mUICbvOffset);
	void BuildUIRenderItem(std::unordered_map<std::string, std::unique_ptr<MeshGeometry>>& mGeometries, Materials & mMaterials);

	void UpdateCharacterCBs(FrameResource* mCurrFrameResource, const Light& mMainLight, const GameTimer & gt);
	void UpdateMonsterPosition(Character& Player, const GameTimer & gt);
	void UpdateCharacterShadows(const Light & mMainLight);

private:
	SkinnedData mSkinnedInfo;
	std::unique_ptr<MeshGeometry> mGeometry;
	std::vector<std::unique_ptr<SkinnedModelInstance>> mSkinnedModelInst;

	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::vector<RenderItem*> mRitems[(int)RenderLayer::Count];

private:
	std::vector<CharacterInfo> mMonsterInfo;
	
	UINT mDamage;
	UINT numOfCharacter;
	int MonsterAreaSize;
};


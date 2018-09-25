#pragma once

#include "MonsterUI.h"
#include "Character.h"

class Monster : public Character
{
public:
	Monster();
	~Monster();

	MonsterUI mMonsterUI;

public:
	virtual int GetHealth(int i = 0) const override;
	virtual CharacterInfo& GetCharacterInfo(int cIndex = 0);
	virtual void Damage(int damage, DirectX::XMVECTOR Position, DirectX::XMVECTOR Look) override;

public:
	bool isClipEnd(std::string clipName, int i);
	bool isAllDie();
	DirectX::XMMATRIX GetWorldTransformMatrix(int i) const;

	UINT GetNumberOfMonster() const;
	UINT GetUISize() const;
	UINT GetAllRitemsSize() const;
	const std::vector<RenderItem*> GetRenderItem(RenderLayer Type) const;

	void SetClipName(const std::string & inClipName, int cIndex);
	void SetMaterialName(const std::string& inMaterialName);
	void SetMonsterIndex(int inMonsterIndex);

public:
	virtual void BuildGeometry(
		ID3D12Device * device,
		ID3D12GraphicsCommandList * cmdList,
		const std::vector<CharacterVertex>& inVertices,
		const std::vector<std::uint32_t>& inIndices,
		const SkinnedData & inSkinInfo, std::string geoName) override;
	virtual void BuildConstantBufferViews(
		ID3D12Device * device,
		ID3D12DescriptorHeap * mCbvHeap,
		const std::vector<std::unique_ptr<FrameResource>>& mFrameResources,
		int mChaCbvOffset) override;
	virtual void BuildRenderItem(
		Materials & mMaterials,
		std::string matrialPrefix = "") override;

	
	void UpdateCharacterCBs(
		FrameResource* mCurrFrameResource,
		const Light& mMainLight,
		const GameTimer & gt);
	virtual void UpdateCharacterShadows(const Light & mMainLight);
	void UpdateMonsterPosition(Character& Player, const GameTimer & gt);

private:
	std::vector<CharacterInfo> mMonsterInfo;

	SkinnedData mSkinnedInfo;
	std::unique_ptr<MeshGeometry> mGeometry;
	std::vector<std::unique_ptr<SkinnedModelInstance>> mSkinnedModelInst;

	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::vector<RenderItem*> mRitems[(int)RenderLayer::Count];

private:
	int mMonsterIndex;
	UINT numOfCharacter;
	UINT mAliveMonster;
	UINT mDamage;
	UINT mBossDamage;
	float mAttackTimes[2];

	std::string MaterialName;
};

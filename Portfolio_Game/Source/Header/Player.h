#pragma once

#include "Character.h"
#include "PlayerCamera.h"
#include "DXUI.h"

enum PlayerMoveList
{
	Walk,
	SideWalk,
	AddYaw,
	AddPitch,
	Death
};

class Player : public Character
{
public:
	Player();
	~Player();

	DXUI mUI;
	PlayerCamera mCamera;

public:
	virtual int GetHealth(int i = 0) const override;
	virtual CharacterInfo& GetCharacterInfo(int cIndex = 0);
	virtual void Damage(int damage, DirectX::XMVECTOR Position, DirectX::XMVECTOR Look) override;
	void Attack(Character& inMonster, std::string clipName);

public:
	bool isClipEnd();
	eClipList GetCurrentClip() const;
	DirectX::XMMATRIX GetWorldTransformMatrix() const;

	UINT GetAllRitemsSize() const;
	const std::vector<RenderItem*> GetRenderItem(RenderLayer Type) const;

	void SetClipName(const std::string & inClipName);
	void SetClipTime(float time);

public:
	virtual void BuildGeometry(
		ID3D12Device * device,
		ID3D12GraphicsCommandList * cmdList,
		const std::vector<SkinnedVertex>& inVertices,
		const std::vector<std::uint32_t>& inIndices,
		const SkinnedData & inSkinInfo, std::string geoName) override;
	virtual void BuildConstantBufferViews(
		ID3D12Device * device,
		ID3D12DescriptorHeap * mCbvHeap,
		const std::vector<std::unique_ptr<FrameResource>>& mFrameResources,
		int mPlayerCbvOffset) override;
	virtual void BuildRenderItem(
		Materials & mMaterials,
		std::string matrialPrefix) override;

	virtual void UpdateCharacterCBs(
		FrameResource* mCurrFrameResource,
		const Light& mMainLight,
		const GameTimer & gt);
	virtual void UpdateCharacterShadows(const Light & mMainLight);
	void UpdatePlayerPosition(PlayerMoveList move, float velocity);
	void UpdateTransformationMatrix();
	
private:
	CharacterInfo mPlayerInfo;
	
	SkinnedData mSkinnedInfo;
	std::unique_ptr<MeshGeometry> mGeometry;
	std::unique_ptr<SkinnedModelInstance> mSkinnedModelInst;

	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::vector<RenderItem*> mRitems[(int)RenderLayer::Count];

private:
	UINT mDamage;
	UINT mFullHealth;
	bool DeathCamFinished = false;
};


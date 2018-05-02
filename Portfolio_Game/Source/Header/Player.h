#pragma once

#include "Character.h"
#include "DXUI.h"
#include "RenderItem.h"
#include "PlayerCamera.h"
#include "PlayerMovement.h"
#include "PlayerController.h"

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
	UINT GetBoneSize() const;
	UINT GetAllRitemsSize() const;
	eClipList GetCurrentClip() const;
	DirectX::XMFLOAT4X4 GetWorldTransform4x4f() const;
	virtual WorldTransform & GetWorldTransform(int i = 0);
	const std::vector<RenderItem*> GetRenderItem(RenderLayer Type) const;

	void SetClipName(const std::string & inClipName);
	void SetClipTime(float time);

public:
	bool isClipEnd();
	virtual int GetHealth(int i = 0) const override;
	virtual void Damage(int damage, DirectX::XMFLOAT3 Position, DirectX::XMFLOAT3 Look) override;
	void Attack(Character& inMonster);

public:
	virtual void BuildGeometry(ID3D12Device * device, ID3D12GraphicsCommandList * cmdList, const std::vector<SkinnedVertex>& inVertices, const std::vector<std::uint32_t>& inIndices, const SkinnedData & inSkinInfo, std::string geoName) override;
	virtual void BuildRenderItem(Materials & mMaterials, std::string matrialPrefix) override;
	virtual void BuildConstantBufferViews(ID3D12Device * device, ID3D12DescriptorHeap * mCbvHeap, const std::vector<std::unique_ptr<FrameResource>>& mFrameResources, int mChaCbvOffset) override;

	void UpdatePlayerPosition(const Character& Monster, PlayerMoveList move, float velocity);
	void UpdateCharacterCBs(FrameResource* mCurrFrameResource, const Light& mMainLight, const GameTimer & gt);
	void UpdateTransformationMatrix();
	void UpdateCharacterShadows(const Light & mMainLight);
	
private:
	PlayerMovement mPlayerMovement;
	
	SkinnedData mSkinnedInfo;
	std::unique_ptr<MeshGeometry> mGeometry;
	std::unique_ptr<SkinnedModelInstance> mSkinnedModelInst;

	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::vector<RenderItem*> mRitems[(int)RenderLayer::Count];

private:
	int mHealth;
	UINT fullHealth;
	bool DeathCamFinished = false;

	std::string mClipName;
	WorldTransform mWorldTransform;
};

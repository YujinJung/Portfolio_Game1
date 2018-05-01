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
	AddPitch
};

class Player : public Character
{
public:
	Player();
	~Player();

	DXUI mUI;
	PlayerCamera mCamera;

public:
	UINT GetAllRitemsSize() const;
	UINT GetBoneSize() const;
	eClipList GetCurrentClip() const;
	WorldTransform & GetWorldTransform();
	DirectX::XMFLOAT4X4 GetWorldTransform4x4f() const;
	const std::vector<RenderItem*> GetRenderItem(RenderLayer Type) const;

	void SetClipName(const std::string & inClipName);
	void SetClipTime(float time);
	bool isClipEnd();

	void BuildGeometry(ID3D12Device * device, ID3D12GraphicsCommandList * cmdList, const std::vector<SkinnedVertex>& inVertices, const std::vector<std::uint32_t>& inIndices, const SkinnedData & inSkinInfo, std::string geoName);
	void BuildRenderItem(Materials & mMaterials, std::string matrialPrefix);
	void BuildConstantBufferViews(ID3D12Device * device, ID3D12DescriptorHeap * mCbvHeap, const std::vector<std::unique_ptr<FrameResource>>& mFrameResources, int mChaCbvOffset);

	void PlayerMove(PlayerMoveList move, float velocity);
	void UpdateCharacterCBs(FrameResource* mCurrFrameResource, const Light& mMainLight, const GameTimer & gt);
	void UpdateTransformationMatrix();
	void UpdateCharacterShadows(const Light & mMainLight);
	
private:
	PlayerMovement mPlayerMovement;
	PlayerController mPlayerController;
	
	SkinnedData mSkinnedInfo;
	std::unique_ptr<MeshGeometry> mGeometry;
	std::unique_ptr<SkinnedModelInstance> mSkinnedModelInst;

	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::vector<RenderItem*> mRitems[(int)RenderLayer::Count];

	WorldTransform mWorldTransform;
	std::string mClipName;
};

#pragma once
/*
class PlayerInfo
{
private:
	XMFLOAT3 mPos;
	XMFLOAT3 mTarget;
	float mYaw, mRoll;
	float mRadius;
	float mPlayerVelocity;
	UINT score;

public:
	PlayerInfo() : mPos(0.0f, 2.0f, 0.0f), mTarget(0.0f, 2.0f, 15.0f), mRadius(2.0f), mPlayerVelocity(0.0f), mYaw(0.0f), mRoll(0.0f)
	{  }

	XMFLOAT3 getPos() const { return mPos; }
	XMFLOAT3 getTarget() const { return mTarget; }
	float getYaw() const { return mYaw; }
	float getRoll() const { return mRoll; }
	float getRadius() const { return mRadius; }
	float getVelocity() const { return mPlayerVelocity; }

	void setPos(const XMFLOAT3& pos) { mPos = pos; }
	void setTarget(const XMFLOAT3& target) { mTarget = target; }
	void setYaw(const float& yaw) { mYaw = yaw; }
	void setRoll(const float& roll) { mRoll = roll; }
	void setRadius(const float& radius) { mRadius = radius; }
	void setVelocity(const float& velocity) { mPlayerVelocity = velocity; }
};
*/

#include "SkinnedData.h"
#include "RenderItem.h"
#include "Materials.h"
#include "../Common/UploadBuffer.h"

struct SkinnedConstants;
struct FrameResource;
class GameTimer;

class Character
{
public:
	Character();
	~Character();

	UINT GetAllRitemsSize() const;

	UINT GetBoneCount() const;

	const std::vector<RenderItem*> GetRenderItem(RenderLayer type);

	void BuildConstantBufferViews(ID3D12Device* device, ID3D12DescriptorHeap* mCbvHeap, const std::vector<std::unique_ptr<FrameResource>> &mFrameResources, int gNumFrameResources, int mChaCbvOffset);

	void BuildGeometry(ID3D12Device * device, ID3D12GraphicsCommandList* cmdList, const std::vector<SkinnedVertex>& inVertices, const std::vector<std::uint16_t>& inIndices, const SkinnedData& inSkinInfo);

	void BuildRenderItem(int BoneCount, int objCBIndex, Materials& mMaterials);
	
	void UpdateCharacterCBs(UploadBuffer<SkinnedConstants>* currSkinnedCB, const Light& mMainLight, const GameTimer& gt);

	void UpdateCharacterShadows(const Light& mMainLight);

private:
	SkinnedData mSkinnedInfo;
	std::unique_ptr<MeshGeometry> mGeometry;
	std::unique_ptr<SkinnedModelInstance> mSkinnedModelInst;
	std::vector<RenderItem*> mRitems[(int)RenderLayer::Count];
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
};


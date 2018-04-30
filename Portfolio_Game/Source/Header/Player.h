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

	void SetClipName(const std::string & inClipName);

	void BuildConstantBufferViews(ID3D12Device * device, ID3D12DescriptorHeap * mCbvHeap, const std::vector<std::unique_ptr<FrameResource>>& mFrameResources, int mChaCbvOffset);

	bool isClipEnd();

	DXUI mUI;
	PlayerCamera mCamera;

	void PlayerMove(PlayerMoveList move, float velocity);
	void UpdateCharacterCBs(FrameResource* mCurrFrameResource, const Light& mMainLight, RenderLayer type, const GameTimer & gt);
	void UpdateTransformationMatrix();
	
private:
	PlayerMovement mPlayerMovement;
	PlayerController mPlayerController;
	
	std::string mClipName;
};

/*
class PlayerInfo
{
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

private:
XMFLOAT3 mPos;
XMFLOAT3 mTarget;
float mYaw, mRoll;
float mRadius;
float mPlayerVelocity;
UINT score;
};
*/

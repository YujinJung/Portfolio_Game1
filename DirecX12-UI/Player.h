#pragma once

#include "Character.h"

class PlayerController;

class Player : public Character
{
public:
	Player();
	~Player();

	void AddYaw(float dx);
	void AddPitch(float dy);

	void Walk(float velocity);
	void SideWalk(float inVelocity);

	void UpdateTransformationMatrix();

private:
	PlayerController * mPlayerController;

	DirectX::XMFLOAT3 mPlayerPosition;
	DirectX::XMFLOAT3 mPlayerLook;
	DirectX::XMFLOAT3 mPlayerUp;
	DirectX::XMFLOAT3 mPlayerRight;

	DirectX::XMFLOAT4X4 mRotation = MathHelper::Identity4x4();

	bool mTransformDirty;
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

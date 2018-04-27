#include "../Common/GameTimer.h"
#include "Player.h"

using namespace DirectX;

Player::Player()
	: Character()
{

}


Player::~Player()
{

}

void Player::PlayerMove(PlayerMoveList move, float velocity)
{
	switch (move)
	{
	case PlayerMoveList::Walk :
		mPlayerMovement.Walk(velocity);
		break;

	case PlayerMoveList::SideWalk :
		mPlayerMovement.SideWalk(velocity);
		break;

	case PlayerMoveList::AddYaw :
		mPlayerMovement.AddYaw(velocity);
		break;
		
	case PlayerMoveList::AddPitch :
		mPlayerMovement.AddPitch(velocity);
		break;
	}

	mTransformDirty = true;
}

void Player::UpdateCharacterCBs(FrameResource* mCurrFrameResource, const Light& mMainLight, const GameTimer & gt)
{
	Character::UpdateCharacterCBs(mCurrFrameResource, mMainLight, gt);

	// UI
	auto currUICB = mCurrFrameResource->UICB.get();
	mUI.UpdateUICBs(currUICB, XMLoadFloat4x4(&GetWorld()), mTransformDirty);
}

void Player::UpdateTransformationMatrix()
{
	XMMATRIX world;

	if (mPlayerMovement.UpdateTransformationMatrix(world))
	{
		SetWorldTransform(world);
	}
}

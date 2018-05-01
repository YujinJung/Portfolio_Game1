
#include <string>
#include <random>
#include <chrono>
#include <DirectXMath.h>
#include "MathHelper.h"
#include "GameTimer.h"
#include "FrameResource.h"
#include "Character.h"

using namespace DirectX;
using namespace DirectX::PackedVector;

Character::Character()
{
	numOfCharacter = 1;
}

Character::~Character()
{
}

//eClipList Character::GetCurrentClip() const
//{
//	return mSkinnedModelInst->state;
//}
//float Character::GetCurrentClipTime() const
//{
//	return mSkinnedModelInst->TimePos;
//}
//WorldTransform& Character::GetWorldTransform(int i) 
//{
//	return mWorldTransform[i];
//}
//DirectX::XMFLOAT4X4 Character::GetWorldTransform4x4f(int i) const
//{
//	auto T = mWorldTransform[i];
//	XMMATRIX P = XMMatrixTranslation(T.Position.x, T.Position.y, T.Position.z);
//	XMMATRIX R = XMLoadFloat4x4(&T.Rotation);
//	XMMATRIX S = XMMatrixScaling(T.Scale.x, T.Scale.y, T.Scale.z);
//
//	XMFLOAT4X4 Ret;
//	XMStoreFloat4x4(&Ret, S * R * P);
//	return Ret;
//}
//const std::vector<RenderItem*> Character::GetRenderItem(RenderLayer Type) const
//{
//	return mRitems[(int)Type];
//}
//
//bool Character::isClipEnd(const std::string& clipName) const
//{
//	if (mSkinnedInfo.GetAnimation(clipName).GetClipEndTime() - mSkinnedModelInst->TimePos < 0.001f)
//		return true;
//	return false;
//}
//
//void Character::SetClipTime(float time)
//{
//	mSkinnedModelInst->TimePos = time;
//}

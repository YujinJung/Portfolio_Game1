#include "../Common/GameTimer.h"
#include "Player.h"

using namespace DirectX;

Player::Player() 
	: mPlayerPosition(0.0f, 0.0f, 0.0f),
	mPlayerLook(0.0f, 0.0f, 1.0f),
	mPlayerUp(0.0f, 1.0f, 0.0f),
	mPlayerRight(1.0f, 0.0f, 0.0f)
{

}


Player::~Player()
{

}

void Player::AddYaw(float dx)
{
	XMMATRIX R = XMMatrixRotationY(dx);
	XMVECTOR PlayerRight = XMVector3TransformNormal(XMLoadFloat3(&mPlayerRight), R);
	XMVECTOR PlayerUp = XMVector3TransformNormal(XMLoadFloat3(&mPlayerUp), R);
	XMVECTOR PlayerLook = XMVector3TransformNormal(XMLoadFloat3(&mPlayerLook), R);


	XMStoreFloat3(&mPlayerRight, PlayerRight);
	XMStoreFloat3(&mPlayerUp, PlayerUp);
	XMStoreFloat3(&mPlayerLook, PlayerLook);

	XMStoreFloat4x4(&mRotation, XMLoadFloat4x4(&mRotation) * R);
	//std::wstring text = L"Yaw: " + std::to_wstring(mPlayerLook.x) + std::to_wstring(mPlayerLook.z) + L"\n";
	//::OutputDebugString(text.c_str());

	mTransformDirty = true;
}

void Player::AddPitch(float dy)
{
	XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&mPlayerRight), dy);

	XMStoreFloat3(&mPlayerUp, XMVector3TransformNormal(XMLoadFloat3(&mPlayerUp), R));
	XMStoreFloat3(&mPlayerLook, XMVector3TransformNormal(XMLoadFloat3(&mPlayerLook), R));
	
	
	mTransformDirty = true;
}

void Player::Walk(float inVelocity)
{
	XMVECTOR Velocity = XMVectorSet(inVelocity, inVelocity, inVelocity, inVelocity);
	XMVECTOR Position = XMLoadFloat3(&mPlayerPosition);
	XMVECTOR Look = XMLoadFloat3(&mPlayerLook);
	XMStoreFloat3(&mPlayerPosition, XMVectorMultiplyAdd(Velocity, Look, Position));

	mTransformDirty = true;
}

void Player::SideWalk(float inVelocity)
{
	XMVECTOR Velocity = XMVectorSet(inVelocity, inVelocity, inVelocity, inVelocity);
	XMVECTOR Position = XMLoadFloat3(&mPlayerPosition);
	XMVECTOR Look = XMLoadFloat3(&mPlayerLook);
	XMStoreFloat3(&mPlayerPosition, XMVectorMultiplyAdd(Velocity, Look, Position));

	mTransformDirty = true;
}

void Player::UpdateTransformationMatrix()
{
	if (mTransformDirty)
	{
		XMVECTOR P = XMLoadFloat3(&mPlayerPosition);
		XMVECTOR L = XMLoadFloat3(&mPlayerLook);
		XMVECTOR U = XMLoadFloat3(&mPlayerUp);
		XMVECTOR R = XMLoadFloat3(&mPlayerRight);

		// Keep camera's axes orthogonal to each other and of unit length.
		L = XMVector3Normalize(L);
		U = XMVector3Normalize(XMVector3Cross(L, R));
		R = XMVector3Cross(U, L);

		XMStoreFloat3(&mPlayerRight, R);
		XMStoreFloat3(&mPlayerUp, U);
		XMStoreFloat3(&mPlayerLook, L);
		
		SetWorldTransform(XMLoadFloat4x4(&mRotation) * XMMatrixTranslation(mPlayerPosition.x, mPlayerPosition.y, mPlayerPosition.z));

		mTransformDirty = false;
	}
}
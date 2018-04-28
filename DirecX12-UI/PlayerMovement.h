#pragma once

#include "../Common/d3dUtil.h"

class PlayerMovement
{
public:
	PlayerMovement();
	PlayerMovement(DirectX::XMFLOAT3 inPlayerPosition, DirectX::XMFLOAT3 inPlayerLook, DirectX::XMFLOAT3 inPlayerUp, DirectX::XMFLOAT3 inPlayerRight);
	~PlayerMovement();

	// Get Member variable
	DirectX::XMVECTOR GetPlayerPosition() const;
	DirectX::XMVECTOR GetPlayerLook() const;
	DirectX::XMVECTOR GetPlayerUp() const;
	DirectX::XMVECTOR GetPlayerRight() const;

	void AddYaw(float dx);
	void AddPitch(float dy);

	void Walk(float velocity);
	void SideWalk(float inVelocity);

	bool UpdateTransformationMatrix(DirectX::XMMATRIX & outMatrix);

private:
	DirectX::XMFLOAT3 mPlayerPosition;
	DirectX::XMFLOAT3 mPlayerLook;
	DirectX::XMFLOAT3 mPlayerUp;
	DirectX::XMFLOAT3 mPlayerRight;

	DirectX::XMFLOAT4X4 mRotation = MathHelper::Identity4x4();

	bool mTransformDirty = false;
};


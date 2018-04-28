#include "PlayerCamera.h"

using namespace DirectX;

PlayerCamera::PlayerCamera()
{
}


PlayerCamera::~PlayerCamera()
{
}

void PlayerCamera::UpdatePosition(
	FXMVECTOR P,
	FXMVECTOR L,
	FXMVECTOR U,
	GXMVECTOR R)
{
	XMVECTOR mP = XMVectorAdd(XMVectorSubtract(P, 15.0f * L), 10.0f * U);
	XMVECTOR mL = XMVector3Normalize(XMVectorSubtract(XMVectorAdd(P, 3.0f * U), mP));
	XMVECTOR mU = XMVector3Normalize(XMVector3Cross(R, mL));

	SetEyePosition(mP);
	SetEyeLook(mL);
	SetEyeUp(mU);
	SetEyeRight(R);

	mViewDirty = true;
}

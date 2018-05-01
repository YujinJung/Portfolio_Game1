
#include "GameTimer.h"
#include "Monster.h"

using namespace DirectX;
Monster::Monster()
	: mClipName("Idle")
{
	numOfCharacter = 5;
}
Monster::~Monster()
{
}

void Monster::BuildConstantBufferViews(ID3D12Device * device, ID3D12DescriptorHeap * mCbvHeap, const std::vector<std::unique_ptr<FrameResource>>& mFrameResources, int mMonsterCbvOffset)
{
	UINT MonsterCount = GetAllRitemsSize();
	UINT MonsterCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(SkinnedConstants));
	UINT mCbvSrvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
	{
		auto MonsterCB = mFrameResources[frameIndex]->MonsterCB->Resource();

		for (UINT i = 0; i < MonsterCount; ++i)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = MonsterCB->GetGPUVirtualAddress();

			// Offset to the ith object constant buffer in the buffer.
			cbAddress += i * MonsterCBByteSize;

			// Offset to the object cbv in the descriptor heap.
			int heapIndex = mMonsterCbvOffset + frameIndex * MonsterCount + i;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, mCbvSrvDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = MonsterCBByteSize;

			device->CreateConstantBufferView(&cbvDesc, handle);
		}
	}
}
void Monster::BuildRenderItem(Materials& mMaterials, std::string matrialPrefix, RenderLayer type)
{
	Character::BuildRenderItem(mMaterials, matrialPrefix, type);
}

void Monster::UpdateCharacterCBs(FrameResource * mCurrFrameResource, const Light & mMainLight, RenderLayer type, const GameTimer & gt)
{
	Character::UpdateCharacterCBs(mCurrFrameResource->MonsterCB, mMainLight, type, mClipName, gt);
}
void Monster::UpdateMonsterPosition(XMFLOAT3 inPlayerPosition, const GameTimer & gt)
{
	// p.. - player
	// m.. - monster
	XMVECTOR pPosition = XMLoadFloat3(&inPlayerPosition);
	XMVECTOR E = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	static std::vector<float> rotationTime(numOfCharacter);

	for (UINT i = 0; i < numOfCharacter; ++i)
	{
		WorldTransform& wTransform = GetWorldTransform(i);
		XMVECTOR mPosition = XMLoadFloat3(&wTransform.Position);
		XMMATRIX mRotation = XMLoadFloat4x4(&wTransform.Rotation);

		float distance = MathHelper::getDistance(pPosition, mPosition);

		// Move Monster
		if (distance < 50.0f) 
		{
			// Monster Collision Check
			for (UINT j = 0; j < numOfCharacter; ++j)
			{
				// Me
				if (i == j) continue;

				// Other Monster nth
				XMVECTOR MnthPos = XMLoadFloat3(&GetWorldTransform(j).Position);
				if (MathHelper::getDistance(MnthPos, mPosition) < 5.0f)
				{
					XMVECTOR Md = XMVectorSubtract(MnthPos, mPosition);
					mPosition = XMVectorSubtract(mPosition, 0.01f * Md);
				}
			}

			XMVECTOR mLook = XMLoadFloat3(&wTransform.Look);
			XMVECTOR D = XMVector3Normalize(XMVectorSubtract(pPosition, mPosition));
			mPosition = XMVectorAdd(mPosition, 0.03f * mLook);
			XMStoreFloat3(&wTransform.Position, mPosition);
			mClipName = "Walking";

			float theta = XMVector3AngleBetweenNormals(mLook, D).m128_f32[0];
			if (theta < XM_PI / 12.0f) continue;

			/*std::wstring text = L"dot: " + std::to_wstring(gt.TotalTime()) + L"\n";
			::OutputDebugString(text.c_str());*/

			XMMATRIX R = XMMatrixRotationY(theta * 0.002f);

			XMStoreFloat3(&wTransform.Look, XMVector3TransformNormal(mLook, R));
			XMStoreFloat4x4(&wTransform.Rotation, mRotation * R);
		}
		else
		{
			mClipName = "Idle";
		}
	}
}

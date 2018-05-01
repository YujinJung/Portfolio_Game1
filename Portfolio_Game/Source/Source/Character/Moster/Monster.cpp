
#include <random>
#include <chrono>
#include "GameTimer.h"
#include "Monster.h"

using namespace DirectX;
Monster::Monster()
{
	numOfCharacter = 5;
	for (int i = 0; i < numOfCharacter; ++i)
		mClipName.push_back("Idle");
}
Monster::~Monster()
{
}


UINT Monster::GetAllRitemsSize() const
{
	return (UINT)mAllRitems.size();
}
UINT Monster::GetBoneSize() const
{
	return (UINT)mSkinnedInfo.BoneCount();
}
UINT Monster::GetNumOfCharacter() const
{
	return numOfCharacter;
}
WorldTransform& Monster::GetWorldTransform(int i)
{
	return mWorldTransform[i];
}
DirectX::XMFLOAT4X4 Monster::GetWorldTransform4x4f(int i) const
{
	auto T = mWorldTransform[i];
	XMMATRIX P = XMMatrixTranslation(T.Position.x, T.Position.y, T.Position.z);
	XMMATRIX R = XMLoadFloat4x4(&T.Rotation);
	XMMATRIX S = XMMatrixScaling(T.Scale.x, T.Scale.y, T.Scale.z);

	XMFLOAT4X4 Ret;
	XMStoreFloat4x4(&Ret, S * R * P);
	return Ret;
}
const std::vector<RenderItem*> Monster::GetRenderItem(RenderLayer Type) const
{
	return mRitems[(int)Type];
}

void Monster::BuildGeometry(
	ID3D12Device * device,
	ID3D12GraphicsCommandList* cmdList,
	const std::vector<SkinnedVertex>& inVertices,
	const std::vector<std::uint32_t>& inIndices,
	const SkinnedData& inSkinInfo,
	std::string geoName)
{
	for (int i = 0; i < numOfCharacter; ++i)
	{
		mSkinnedInfo = inSkinInfo;

		auto skinnedModelInst = std::make_unique<SkinnedModelInstance>();
		skinnedModelInst->SkinnedInfo = &mSkinnedInfo;
		skinnedModelInst->FinalTransforms.resize(mSkinnedInfo.BoneCount());
		skinnedModelInst->TimePos = 0.0f;

		mSkinnedModelInst.push_back(std::move(skinnedModelInst));
	}

	if (inVertices.size() == 0)
	{
		MessageBox(0, L"Fbx not found", 0, 0);
		return;
	}

	UINT vCount = 0, iCount = 0;
	vCount = inVertices.size();
	iCount = inIndices.size();

	const UINT vbByteSize = (UINT)inVertices.size() * sizeof(SkinnedVertex);
	const UINT ibByteSize = (UINT)inIndices.size() * sizeof(std::uint32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = geoName;

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), inVertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), inIndices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(device, cmdList, inVertices.data(), vbByteSize, geo->VertexBufferUploader);
	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(device, cmdList, inIndices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(SkinnedVertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	auto vSubmeshOffset = mSkinnedInfo.GetSubmeshOffset();
	auto boneName = mSkinnedInfo.GetBoneName();

	UINT SubmeshOffsetIndex = 0;
	for (int i = 0; i < vSubmeshOffset.size(); ++i)
	{
		UINT CurrSubmeshOffsetIndex = vSubmeshOffset[i];

		SubmeshGeometry FbxSubmesh;
		FbxSubmesh.IndexCount = CurrSubmeshOffsetIndex;
		FbxSubmesh.StartIndexLocation = SubmeshOffsetIndex;
		FbxSubmesh.BaseVertexLocation = 0;

		std::string SubmeshName = boneName[i];
		geo->DrawArgs[SubmeshName] = FbxSubmesh;

		SubmeshOffsetIndex += CurrSubmeshOffsetIndex;
	}

	mGeometry = std::move(geo);
}

void Monster::BuildRenderItem(Materials& mMaterials, std::string matrialPrefix)
{
	int chaIndex = 0;
	int BoneCount = GetBoneSize();
	auto boneName = mSkinnedInfo.GetBoneName();

	for (int characterIndex = 0; characterIndex < numOfCharacter; ++characterIndex)
	{
		WorldTransform wTransform;
		wTransform.Position = { 0.0f, 0.0f, 0.0f };
		wTransform.Scale = { 1.0f, 1.0f, 1.0f };
		wTransform.Look = { 0.0f, 0.0f, 1.0f };
		XMStoreFloat4x4(&wTransform.Rotation, XMMatrixRotationRollPitchYaw(0.0f, 0.0f, 0.0f));

		// Monster - Random Position
		auto seed = std::chrono::system_clock::now().time_since_epoch().count();
		std::mt19937 engine{ (unsigned int)seed };
		std::uniform_int_distribution <> dis{ 40, 80 };

		//Generate a random integer
		int x{ dis(engine) };
		int z{ dis(engine) };

		wTransform.Position = { static_cast<float>(x), 0.0f, static_cast<float>(z) };

		// Character Mesh
		for (int submeshIndex = 0; submeshIndex < BoneCount - 1; ++submeshIndex)
		{
			std::string SubmeshName = boneName[submeshIndex];
			std::string MaterialName = matrialPrefix; // TODO : Setting the Name

			auto MonsterRitem = std::make_unique<RenderItem>();
			XMStoreFloat4x4(&MonsterRitem->World, XMMatrixScaling(4.0f, 4.0f, 4.0f));
			MonsterRitem->TexTransform = MathHelper::Identity4x4();
			MonsterRitem->Mat = mMaterials.Get(MaterialName);
			MonsterRitem->Geo = mGeometry.get();
			MonsterRitem->NumFramesDirty = gNumFrameResources;
			MonsterRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			MonsterRitem->StartIndexLocation = MonsterRitem->Geo->DrawArgs[SubmeshName].StartIndexLocation;
			MonsterRitem->BaseVertexLocation = MonsterRitem->Geo->DrawArgs[SubmeshName].BaseVertexLocation;
			MonsterRitem->IndexCount = MonsterRitem->Geo->DrawArgs[SubmeshName].IndexCount;
			MonsterRitem->SkinnedModelInst = mSkinnedModelInst[characterIndex].get();
			MonsterRitem->MonsterCBIndex = chaIndex++;

			auto shadowedObjectRitem = std::make_unique<RenderItem>();
			*shadowedObjectRitem = *MonsterRitem;
			shadowedObjectRitem->Mat = mMaterials.Get("shadow0");
			shadowedObjectRitem->NumFramesDirty = gNumFrameResources;
			shadowedObjectRitem->SkinnedModelInst = mSkinnedModelInst[characterIndex].get();
			shadowedObjectRitem->MonsterCBIndex = chaIndex++;

			mRitems[(int)RenderLayer::Monster].push_back(MonsterRitem.get());
			mAllRitems.push_back(std::move(MonsterRitem));
			mRitems[(int)RenderLayer::Shadow].push_back(shadowedObjectRitem.get());
			mAllRitems.push_back(std::move(shadowedObjectRitem));
		}
		mWorldTransform.push_back(wTransform);
	}
}
void Monster::BuildConstantBufferViews(ID3D12Device * device, ID3D12DescriptorHeap * mCbvHeap, const std::vector<std::unique_ptr<FrameResource>>& mFrameResources, int mMonsterCbvOffset)
{
	UINT MonsterCount = GetAllRitemsSize();
	UINT MonsterCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MonsterContants));
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

void Monster::UpdateCharacterCBs(FrameResource * mCurrFrameResource, const Light & mMainLight, const GameTimer & gt)
{
	for (int i = 0; i < numOfCharacter; ++i)
	{
		auto currCharacterCB = mCurrFrameResource->MonsterCB.get();
		UpdateCharacterShadows(mMainLight);

		// Character Offset : mAllsize  / numOfcharacter
		int characterOffset = mAllRitems.size() / numOfCharacter;
		int j = 0;
		static float time = 0.0f;
		if (gt.TotalTime() - time > 0.01f)
		{
			for (int k = 0; k < numOfCharacter; ++k)
			{
				mSkinnedModelInst[k]->UpdateSkinnedAnimation(mClipName[k], gt.DeltaTime());
			}
			time = gt.TotalTime();
		}

		for (auto& e : mAllRitems)
		{
			int CharacterIndex = j / characterOffset;

			if (mTransformDirty) { e->NumFramesDirty = gNumFrameResources; }
			if (e->NumFramesDirty > 0)
			{
				MonsterContants monsterConstants;

				std::copy(
					std::begin(mSkinnedModelInst[CharacterIndex]->FinalTransforms),
					std::end(mSkinnedModelInst[CharacterIndex]->FinalTransforms),
					&monsterConstants.BoneTransforms[CharacterIndex][0]);

				// TODO : player constroller
				XMMATRIX world = XMLoadFloat4x4(&e->World) * XMLoadFloat4x4(&GetWorldTransform4x4f(CharacterIndex));
				XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);
				monsterConstants.monsterIndex = CharacterIndex;

				XMStoreFloat4x4(&monsterConstants.World[CharacterIndex], XMMatrixTranspose(world));
				XMStoreFloat4x4(&monsterConstants.TexTransform[CharacterIndex], XMMatrixTranspose(texTransform));

				currCharacterCB->CopyData(e->MonsterCBIndex, monsterConstants);

				// Next FrameResource need to be updated too.
				// TODO:
				//e->NumFramesDirty--;
			}
			++j;
		}
	}
	
}
void Monster::UpdateMonsterPosition( XMFLOAT3 inPlayerPosition, const GameTimer & gt)
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
		if (distance < 50.0f && distance > 10.0f) 
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
					mPosition = XMVectorSubtract(mPosition, 0.1f * Md);
				}
			}

			XMVECTOR mLook = XMLoadFloat3(&wTransform.Look);
			XMVECTOR D = XMVector3Normalize(XMVectorSubtract(pPosition, mPosition));
			mPosition = XMVectorAdd(mPosition, 0.1f * mLook);
			XMStoreFloat3(&wTransform.Position, mPosition);
			mClipName[i] = "Walking";

			float theta = XMVector3AngleBetweenNormals(mLook, D).m128_f32[0];
			if (theta < XM_PI / 12.0f) continue;

			/*std::wstring text = L"dot: " + std::to_wstring(gt.TotalTime()) + L"\n";
			::OutputDebugString(text.c_str());*/
			XMMATRIX R = XMMatrixRotationY(theta * 0.03f);

			XMStoreFloat3(&wTransform.Look, XMVector3TransformNormal(mLook, R));
			XMStoreFloat4x4(&wTransform.Rotation, mRotation * R);
		}
		else if (distance < 10.0f)
		{
			mClipName[i] = "MAttack1";
			// if player health exist
				mSkinnedModelInst[i]->TimePos = 0.0f;
		}
		else
		{
			mClipName[i] = "Idle";
		}
	}
}

void Monster::UpdateCharacterShadows(const Light& mMainLight)
{
	int i = 0;
	for (auto& e : mRitems[(int)RenderLayer::Shadow])
	{
		// Load the object world
		auto& o = mRitems[(int)RenderLayer::Monster][i];
		XMMATRIX shadowWorld = XMLoadFloat4x4(&o->World);

		XMVECTOR shadowPlane = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		XMVECTOR toMainLight = -XMLoadFloat3(&mMainLight.Direction);
		XMMATRIX S = XMMatrixShadow(shadowPlane, toMainLight);
		XMMATRIX shadowOffsetY = XMMatrixTranslation(0.0f, 0.001f, 0.0f);
		XMStoreFloat4x4(&e->World, shadowWorld * S * shadowOffsetY);
		e->NumFramesDirty = gNumFrameResources;

		++i;
	}
}



#include <random>
#include <chrono>
#include "GameTimer.h"
#include "Monster.h"

using namespace DirectX;
Monster::Monster()
	: numOfCharacter(10),
	mDamage(0),
	mFullHealth(100),
	MonsterAreaSize(20)
{
	for (UINT i = 0; i < numOfCharacter; ++i)
	{
		CharacterInfo M;
		M.mClipName = "Idle";
		M.mHealth = 100;

		// World and View
		XMVECTOR V = XMVectorZero();
		M.mMovement.SetPlayerPosition(V);
		V.m128_f32[2] = 1.0f;
		M.mMovement.SetPlayerLook(V);
		V = XMVectorReplicate(1.0f);
		M.mMovement.SetPlayerScale(V);
		M.mMovement.SetPlayerRotation(XMMatrixIdentity());

		mMonsterInfo.push_back(M);
	}

}
Monster::~Monster()
{
}

int Monster::GetHealth(int i) const
{
	return mMonsterInfo[i].mHealth;
}
CharacterInfo & Monster::GetCharacterInfo(int cIndex)
{
	return mMonsterInfo[cIndex];
}
void Monster::Damage(int damage, XMVECTOR Position, XMVECTOR Look)
{
	XMVECTOR HitTargetv = XMVectorAdd(Position, Look * 5.0f);

	// Damage
	for (UINT cIndex = 0; cIndex < numOfCharacter; ++cIndex)
	{
		XMVECTOR MonsterPos = mMonsterInfo[cIndex].mMovement.GetPlayerPosition();

		if (MathHelper::getDistance(HitTargetv, MonsterPos) < 5.0f)
		{
			if (mMonsterInfo[cIndex].mHealth >= 0)
				mSkinnedModelInst[cIndex]->TimePos = 0.0f;

			SetClipName("HitReaction", cIndex);
			mMonsterInfo[cIndex].mHealth -= damage;
		}
		if (mMonsterInfo[cIndex].mHealth < 0)
			mMonsterInfo[cIndex].mHealth = 0;
		mMonsterUI.SetDamageScale(cIndex, static_cast<float>(mMonsterInfo[cIndex].mHealth) / static_cast<float>(mFullHealth));
	}
}

bool Monster::isClipEnd(std::string clipName, int i)
{
	if (mSkinnedInfo.GetAnimation(clipName).GetClipEndTime() - mSkinnedModelInst[i]->TimePos < 0.001f)
		return true;
	return false;
}
DirectX::XMMATRIX Monster::GetWorldTransformMatrix(int i) const
{
	auto T = mMonsterInfo[i].mMovement.GetWorldTransformInfo();
	XMMATRIX P = XMMatrixTranslation(T.Position.x, T.Position.y, T.Position.z);
	XMMATRIX R = XMLoadFloat4x4(&T.Rotation);
	XMMATRIX S = XMMatrixScaling(T.Scale.x, T.Scale.y, T.Scale.z);

	return S * R * P;
}

UINT Monster::GetNumberOfMonster() const
{
	return numOfCharacter;
}
UINT Monster::GetUISize() const
{
	UINT ret = 0;
	for (UINT i = 0; i < numOfCharacter; ++i)
	{
		ret = ret + mMonsterUI.GetSize();
	}
	return ret;
}
UINT Monster::GetAllRitemsSize() const 
{
	return (UINT)mAllRitems.size();
}
const std::vector<RenderItem*> Monster::GetRenderItem(RenderLayer Type) const
{
	return mRitems[(int)Type];
}

void Monster::SetClipName(const std::string& inClipName, int cIndex)
{
	if (mMonsterInfo[cIndex].mClipName != "Death")
	{
		mMonsterInfo[cIndex].mClipName = inClipName;
		if (inClipName == "Death")
		{
			mSkinnedModelInst[cIndex]->TimePos = 0.0f;
			mMonsterInfo[cIndex].mHealth = 0.0f;
		}
	}
}

void Monster::BuildGeometry(
	ID3D12Device * device,
	ID3D12GraphicsCommandList* cmdList,
	const std::vector<SkinnedVertex>& inVertices,
	const std::vector<std::uint32_t>& inIndices,
	const SkinnedData& inSkinInfo,
	std::string geoName)
{
	for (UINT i = 0; i < numOfCharacter; ++i)
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
	vCount = (UINT)inVertices.size();
	iCount = (UINT)inIndices.size();

	const UINT vbByteSize = vCount * sizeof(SkinnedVertex);
	const UINT ibByteSize = iCount * sizeof(std::uint32_t);

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
void Monster::BuildConstantBufferViews(
	ID3D12Device * device,
	ID3D12DescriptorHeap * mCbvHeap,
	const std::vector<std::unique_ptr<FrameResource>>& mFrameResources,
	int mMonsterCbvOffset)
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
void Monster::BuildRenderItem(
	Materials& mMaterials,
	std::string matrialPrefix)
{
	int chaIndex = 0;
	int BoneCount = (UINT)mSkinnedInfo.BoneCount();
	auto boneName = mSkinnedInfo.GetBoneName();

	for (UINT cIndex = 0; cIndex < numOfCharacter; ++cIndex)
	{
		CharacterInfo cInfo;

		// Monster - Random Position
		auto seed = std::chrono::system_clock::now().time_since_epoch().count();
		std::mt19937 engine{ (unsigned int)seed };
		std::uniform_int_distribution <> dis{ 1, MonsterAreaSize * 2 };

		//Generate a random integer
		int x{ dis(engine) };
		int z{ dis(engine) };

		XMVECTOR monsterPos = XMVectorSet(static_cast<float>(x - MonsterAreaSize), 0.0f, static_cast<float>(z - MonsterAreaSize), 0.0f);
		cInfo.mMovement.SetPlayerPosition(monsterPos);

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
			MonsterRitem->SkinnedModelInst = mSkinnedModelInst[cIndex].get();
			MonsterRitem->MonsterCBIndex = chaIndex++;

			auto shadowedObjectRitem = std::make_unique<RenderItem>();
			*shadowedObjectRitem = *MonsterRitem;
			shadowedObjectRitem->Mat = mMaterials.Get("shadow0");
			shadowedObjectRitem->NumFramesDirty = gNumFrameResources;
			shadowedObjectRitem->SkinnedModelInst = mSkinnedModelInst[cIndex].get();
			shadowedObjectRitem->MonsterCBIndex = chaIndex++;

			mRitems[(int)RenderLayer::Monster].push_back(MonsterRitem.get());
			mAllRitems.push_back(std::move(MonsterRitem));
			mRitems[(int)RenderLayer::Shadow].push_back(shadowedObjectRitem.get());
			mAllRitems.push_back(std::move(shadowedObjectRitem));
		}

		mMonsterInfo[cIndex] = cInfo;
	}
}


void Monster::UpdateCharacterCBs(
	FrameResource * mCurrFrameResource,
	const Light & mMainLight,
	const GameTimer & gt)
{
	auto curMonsterCB = mCurrFrameResource->MonsterCB.get();
	static float time = 0.0f;

	// Animation per 0.01s
	if (gt.TotalTime() - time > 0.01f)
	{
		for (UINT k = 0; k < numOfCharacter; ++k)
		{
			mSkinnedModelInst[k]->UpdateSkinnedAnimation(mMonsterInfo[k].mClipName, gt.DeltaTime());
		}
		time = gt.TotalTime();
	}

	// Character Offset : mAllsize  / numOfcharacter
	int j = 0;
	int preMonsterIndex = -1;
	int monsterOffset = mRitems[(int)RenderLayer::Monster].size() / numOfCharacter;
	std::vector<XMMATRIX> vWorld;
	std::vector<XMVECTOR> vEyeLeft;

	// Monster
	for (auto& e : mRitems[(int)RenderLayer::Monster])
	{
		int monsterIndex = j / monsterOffset;

		MonsterContants monsterConstants;

		std::copy(
			std::begin(mSkinnedModelInst[monsterIndex]->FinalTransforms),
			std::end(mSkinnedModelInst[monsterIndex]->FinalTransforms),
			&monsterConstants.BoneTransforms[monsterIndex][0]);

		// TODO : player constroller
		XMMATRIX world = XMLoadFloat4x4(&e->World) * GetWorldTransformMatrix(monsterIndex);
		XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);
		monsterConstants.monsterIndex = monsterIndex;

		XMStoreFloat4x4(&monsterConstants.World[monsterIndex], XMMatrixTranspose(world));
		XMStoreFloat4x4(&monsterConstants.TexTransform[monsterIndex], XMMatrixTranspose(texTransform));

		curMonsterCB->CopyData(e->MonsterCBIndex, monsterConstants);

		if (preMonsterIndex != monsterIndex)
		{
			preMonsterIndex = monsterIndex;

			mMonsterInfo[monsterIndex].mMovement.UpdateTransformationMatrix();
			vWorld.push_back(world);
			vEyeLeft.push_back(-mMonsterInfo[monsterIndex].mMovement.GetPlayerRight());
		}

		++j;
	}


	// Shadow
	UpdateCharacterShadows(mMainLight);
	j = 0;
	for (auto& e : mRitems[(int)RenderLayer::Shadow])
	{
		int monsterIndex = j / monsterOffset;

		MonsterContants monsterConstants;

		std::copy(
			std::begin(mSkinnedModelInst[monsterIndex]->FinalTransforms),
			std::end(mSkinnedModelInst[monsterIndex]->FinalTransforms),
			&monsterConstants.BoneTransforms[monsterIndex][0]);

		// TODO : player constroller
		XMMATRIX world = XMLoadFloat4x4(&e->World);
		XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);
		monsterConstants.monsterIndex = monsterIndex;

		XMStoreFloat4x4(&monsterConstants.World[monsterIndex], XMMatrixTranspose(world));
		XMStoreFloat4x4(&monsterConstants.TexTransform[monsterIndex], XMMatrixTranspose(texTransform));

		curMonsterCB->CopyData(e->MonsterCBIndex, monsterConstants);

		++j;
	}


	//UI
	auto curUICB = mCurrFrameResource->MonsterUICB.get();
	mMonsterUI.UpdateUICBs(curUICB, vWorld, vEyeLeft, mTransformDirty);
}

void Monster::UpdateCharacterShadows(const Light& mMainLight)
{
	int i = 0;
	int monsterOffset = mRitems[(int)RenderLayer::Shadow].size() / numOfCharacter;

	for (auto& e : mRitems[(int)RenderLayer::Shadow])
	{
		int monsterIndex = i / monsterOffset;

		// Load the object world
		auto& o = mRitems[(int)RenderLayer::Monster][i];
		XMMATRIX shadowWorld = XMLoadFloat4x4(&o->World) * GetWorldTransformMatrix(monsterIndex);

		XMVECTOR shadowPlane = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		XMVECTOR toMainLight = -XMLoadFloat3(&mMainLight.Direction);
		XMMATRIX S = XMMatrixShadow(shadowPlane, toMainLight);
		XMMATRIX shadowOffsetY = XMMatrixTranslation(0.0f, 0.001f, 0.0f);
		XMStoreFloat4x4(&e->World, shadowWorld * S * shadowOffsetY);
		e->NumFramesDirty = gNumFrameResources;

		++i;
	}
}

void Monster::UpdateMonsterPosition(Character& Player, const GameTimer & gt)
{
	// p.. - player
	// m.. - monster
	XMVECTOR E = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	XMVECTOR pPosition = Player.GetCharacterInfo().mMovement.GetPlayerPosition();
	static std::vector<std::pair<float, bool>> HitTime(numOfCharacter, std::make_pair(gt.TotalTime(), false));

	for (UINT cIndex = 0; cIndex < numOfCharacter; ++cIndex)
	{
		// Monster Die
		if (mMonsterInfo[cIndex].mHealth <= 0)
		{
			SetClipName("Death", cIndex);
		}
		if (mMonsterInfo[cIndex].mClipName == "Death")
		{
			continue;
		}

		auto& M = mMonsterInfo[cIndex];
		XMVECTOR mUp = M.mMovement.GetPlayerUp();
		XMVECTOR mLook = M.mMovement.GetPlayerLook();
		XMVECTOR mPosition = M.mMovement.GetPlayerPosition();
		XMMATRIX mRotation = M.mMovement.GetPlayerRotation();

		XMMATRIX R = XMMatrixIdentity();
		XMVECTOR D = XMVector3Normalize(XMVectorSubtract(pPosition, mPosition));
		float theta = XMVector3AngleBetweenNormals(mLook, D).m128_f32[0];

		// left right check ; Left - minus / Right - plus
		float res = XMVector3Dot(XMVector3Cross(D, mLook), mUp).m128_f32[0];

		if (theta > XM_PI / 36.0f)
		{
			if (res > 0)
				R = R * XMMatrixRotationY(0.03f * -theta);
			else
				R = R * XMMatrixRotationY(0.03f * theta);
		}

		float curDeltaTime = gt.TotalTime() - HitTime[cIndex].first;
		float curClipTime = mSkinnedModelInst[cIndex]->TimePos;
		if (curDeltaTime < 5.0f) // Attack
		{
			// After half the full time of the clip
			if (mSkinnedInfo.GetClipEndTime("MAttack1") / 2.0f < curClipTime && HitTime[cIndex].second)
			{
				Player.Damage(mDamage, mPosition, mLook);

				HitTime[cIndex].second = false;
			}
		}

		float distance = MathHelper::getDistance(pPosition, mPosition);


		if (distance < 3.0f)
		{
			// Move Back
			mPosition = XMVectorSubtract(mPosition, mLook);
			M.mMovement.SetPlayerPosition(mPosition);
		}
		else if (distance < 10.0f)
		{
			float pHealth = static_cast<float>(Player.GetCharacterInfo().mHealth);

			if (curDeltaTime > 10.0f && pHealth > 0) // Hit per 5 seconds
			{
				HitTime[cIndex].first = gt.TotalTime();
				SetClipName("MAttack1", cIndex);
				mSkinnedModelInst[cIndex]->TimePos = 0.0f;

				HitTime[cIndex].second = true;
			}
			else if (pHealth <= 0)
			{
				SetClipName("Idle", cIndex);
			}
		}
		else if (distance < 50.0f) // Move Monster
		{
			// Monster Collision Check
			for (UINT j = 0; j < numOfCharacter; ++j)
			{
				// Me
				if (cIndex == j) continue;

				// Other Monster nth
				XMVECTOR MnthPos = mMonsterInfo[j].mMovement.GetPlayerPosition();
				XMVECTOR MnthDirection = XMVectorSubtract(MnthPos, mPosition);

				// Monster nth`s Position is NOT Front
				if (XMVector3Dot(mLook, MnthDirection).m128_f32[0] < 0)
					continue;

				if (MathHelper::getDistance(MnthPos, mPosition) < 5.0f)
				{
					XMVECTOR Md = XMVectorSubtract(MnthPos, mPosition);
					mPosition = XMVectorSubtract(mPosition, 0.01f * Md);

					// Rotate opposite direction
					if (res > 0)
						R = R * XMMatrixRotationY(0.1f * theta);
					else
						R = R * XMMatrixRotationY(0.1f * -theta);
				}
			}

			// Move to player
			mPosition = XMVectorAdd(mPosition, 0.15f * mLook);

			SetClipName("Walking", cIndex);
			mTransformDirty = true;
		}
		else
		{
			SetClipName("Idle", cIndex);
		}

		M.mMovement.SetPlayerLook(XMVector3TransformNormal(mLook, R));
		M.mMovement.SetPlayerRotation(mRotation * R);
		M.mMovement.SetPlayerPosition(mPosition);
	}
}

/*std::wstring text = L"dot: " + std::to_wstring(gt.TotalTime()) + L"\n";
::OutputDebugString(text.c_str());*/

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
	XMStoreFloat4x4(&mWorldTransform, XMMatrixIdentity());
	numOfCharacter = 1;
}

Character::~Character()
{
}

UINT Character::GetCharacterMeshSize() const
{
	return (UINT)mRitems[(int)RenderLayer::Character].size();
}
UINT Character::GetAllRitemsSize() const
{
	return (UINT)mAllRitems.size();
}
UINT Character::GetBoneSize() const
{
	return (UINT)mSkinnedInfo.BoneCount();
}
DirectX::XMFLOAT4X4 Character::GetWorld() const
{
	return mWorldTransform;
}
eClipList Character::GetCurrentClip() const
{
	return mSkinnedModelInst->state;
}
float Character::GetCurrentClipTime() const
{
	return mSkinnedModelInst->TimePos;
}
bool Character::isClipEnd(const std::string& clipName) const
{
	if (mSkinnedInfo.GetAnimation(clipName).GetClipEndTime() - mSkinnedModelInst->TimePos < 0.001f)
		return true;
	return false;
}
const std::vector<RenderItem*> Character::GetRenderItem(RenderLayer Type) const
{
	return mRitems[(int)Type];
}

void Character::SetClipTime(float time)
{
	mSkinnedModelInst->TimePos = time;
}
void Character::SetWorldTransform(DirectX::XMMATRIX inWorldTransform)
{
	XMStoreFloat4x4(&mWorldTransform, inWorldTransform);
	mTransformDirty = true;
}

void Character::BuildGeometry(ID3D12Device * device, ID3D12GraphicsCommandList* cmdList, const std::vector<SkinnedVertex>& inVertices, const std::vector<std::uint32_t>& inIndices, const SkinnedData& inSkinInfo, std::string geoName)
{
	mSkinnedInfo = inSkinInfo;

	mSkinnedModelInst = std::make_unique<SkinnedModelInstance>();
	mSkinnedModelInst->SkinnedInfo = &mSkinnedInfo;
	mSkinnedModelInst->FinalTransforms.resize(mSkinnedInfo.BoneCount());
	mSkinnedModelInst->TimePos = 0.0f;

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

void Character::BuildRenderItem(Materials& mMaterials, std::string matrialPrefix, RenderLayer type)
{
	int chaIndex = 0;
	int BoneCount = GetBoneSize();
	auto boneName = mSkinnedInfo.GetBoneName();

	int worldX = 0, worldZ = 0;
	for (int characterIndex = 0; characterIndex < numOfCharacter; ++characterIndex)
	{
		// Monster - Random Position
		if (type == RenderLayer::Monster)
		{
			auto seed = std::chrono::system_clock::now().time_since_epoch().count();
			std::mt19937 engine{ (unsigned int)seed };
			std::uniform_int_distribution <> dis{ 40, 80 };

			//Generate a random integer
			int x{ dis(engine) };
			int z{ dis(engine) };

			worldX = x;
			worldZ = z;
		}
		
		// Character Mesh
		for (int submeshIndex = 0; submeshIndex < BoneCount - 1; ++submeshIndex)
		{
			std::string SubmeshName = boneName[submeshIndex];
			std::string MaterialName = matrialPrefix;
			// TODO : Setting the Name

			auto FbxRitem = std::make_unique<RenderItem>();
			XMStoreFloat4x4(&FbxRitem->World, XMMatrixScaling(4.0f, 4.0f, 4.0f) * XMMatrixTranslation(worldX, 0.0f, worldZ));
			FbxRitem->TexTransform = MathHelper::Identity4x4();
			FbxRitem->Mat = mMaterials.Get(MaterialName);
			FbxRitem->Geo = mGeometry.get();
			FbxRitem->NumFramesDirty = gNumFrameResources;
			FbxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			FbxRitem->StartIndexLocation = FbxRitem->Geo->DrawArgs[SubmeshName].StartIndexLocation;
			FbxRitem->BaseVertexLocation = FbxRitem->Geo->DrawArgs[SubmeshName].BaseVertexLocation;
			FbxRitem->IndexCount = FbxRitem->Geo->DrawArgs[SubmeshName].IndexCount;

			if (type == RenderLayer::Character)
			{
				FbxRitem->PlayerCBIndex = chaIndex++;
			}
			else if (type == RenderLayer::Monster)
			{
				FbxRitem->MonsterCBIndex = chaIndex++;
			}
			FbxRitem->SkinnedModelInst = mSkinnedModelInst.get();
			mRitems[(int)type].push_back(FbxRitem.get());
			mAllRitems.push_back(std::move(FbxRitem));
		}
	}

	// Character Shadow
	for (auto& e : mRitems[(int)type])
	{
		auto shadowedObjectRitem = std::make_unique<RenderItem>();
		*shadowedObjectRitem = *e;
		shadowedObjectRitem->Mat = mMaterials.Get("shadow0");
		shadowedObjectRitem->NumFramesDirty = gNumFrameResources;


		if (type == RenderLayer::Character)
		{
			shadowedObjectRitem->PlayerCBIndex= chaIndex++;
		}
		else if (type == RenderLayer::Monster)
		{
			shadowedObjectRitem->MonsterCBIndex = chaIndex++;
		}
		shadowedObjectRitem->SkinnedModelInst = mSkinnedModelInst.get();

		mRitems[(int)RenderLayer::Shadow].push_back(shadowedObjectRitem.get());
		mAllRitems.push_back(std::move(shadowedObjectRitem));
	}
}

void Character::UpdateCharacterCBs(std::unique_ptr<UploadBuffer<SkinnedConstants>> &currCharacter, const Light& mMainLight, RenderLayer type, const std::string& clipName, const GameTimer & gt)
{
	auto currCharacterCB = currCharacter.get();
	UpdateCharacterShadows(mMainLight, type);

	mSkinnedModelInst->UpdateSkinnedAnimation(clipName, gt.DeltaTime());

	for (auto& e : mAllRitems)
	{
		if (mTransformDirty) { e->NumFramesDirty = gNumFrameResources; }
		if (e->NumFramesDirty > 0)
		{
			SkinnedConstants skinnedConstants;

			std::copy(
				std::begin(mSkinnedModelInst->FinalTransforms),
				std::end(mSkinnedModelInst->FinalTransforms),
				&skinnedConstants.BoneTransforms[0]);

			// TODO : player constroller
			XMMATRIX world = XMLoadFloat4x4(&e->World) * XMLoadFloat4x4(&mWorldTransform);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			XMStoreFloat4x4(&skinnedConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&skinnedConstants.TexTransform, XMMatrixTranspose(texTransform));

			if(type == RenderLayer::Character)
				currCharacterCB->CopyData(e->PlayerCBIndex, skinnedConstants);
			else if(type == RenderLayer::Monster)
				currCharacterCB->CopyData(e->MonsterCBIndex, skinnedConstants);

			// Next FrameResource need to be updated too.

			// TODO:
			//e->NumFramesDirty--;
		}
	}
}
void Character::UpdateCharacterShadows(const Light& mMainLight, RenderLayer type)
{
	int i = 0;
	for (auto& e : mRitems[(int)RenderLayer::Shadow])
	{
		// Load the object world
		auto& o = mRitems[(int)type][i];
		XMMATRIX shadowWorld = XMLoadFloat4x4(&o->World);

		XMVECTOR shadowPlane = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		XMVECTOR toMainLight = -XMLoadFloat3(&mMainLight.Direction);
		XMMATRIX S = XMMatrixShadow(shadowPlane, toMainLight);
		XMMATRIX shadowOffsetY = XMMatrixTranslation(0.0f, 0.001f, 0.0f);
		XMStoreFloat4x4(&e->World, shadowWorld * S * shadowOffsetY);
		e->NumFramesDirty = gNumFrameResources;

		//printMatrix(L"shadow ", i, XMLoadFloat4x4(&e->World));
		++i;
	}

}


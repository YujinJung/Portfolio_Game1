#pragma once

//#include "Player.h"
//#include "Monster.h"
#include "SkinnedData.h"

enum eClipList
{
	Idle,
	StartWalking,
	Walking,
	StopWalking,
	Kick,
	FlyingKick
};

//struct CharacterList
//{
//	static Player mPlayer;
//	static Monster mMonster;
//};

struct SkinnedModelInstance
{
	SkinnedData* SkinnedInfo = nullptr;
	std::vector<DirectX::XMFLOAT4X4> FinalTransforms;
	float TimePos = 0.0f;
	bool ClipEnd;
	eClipList state;
	
	// Called every frame and increments the time position, interpolates the 
	// animations for each bone based on the current animation clip, and 
	// generates the final transforms which are ultimately set to the effect
	// for processing in the vertex shader.
	void UpdateSkinnedAnimation(std::string ClipName, float dt)
	{
		TimePos += dt;

		// Loop animation
		if (TimePos > SkinnedInfo->GetClipEndTime(ClipName))
		{
			if (ClipName == "Idle" || ClipName == "Walking")
			{
				TimePos = 0.0f;
			}
			ClipEnd = true;
		}
		//else if (ClipName == "")


		ClipEnd = false;
		if (ClipName == "Idle")
		{
			state = eClipList::Idle;
			ClipEnd = true;
		}
		else if (ClipName == "StartWalking")
			state = eClipList::StartWalking;
		else if (ClipName == "Walking" || ClipName == "WalkingBackward" || ClipName == "playerWalking")
			state = eClipList::Walking;
		else if (ClipName == "StopWalking")
			state = eClipList::StopWalking;
		else if (ClipName == "Kick")
			state = eClipList::Kick;
		else if (ClipName == "FlyingKick")
			state = eClipList::FlyingKick;

		// Compute the final transforms for this time position.
		SkinnedInfo->GetFinalTransforms(ClipName, TimePos, FinalTransforms);
	}

	// TODO SetClipName?
};

struct WorldTransform
{
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT3 Scale;
	DirectX::XMFLOAT4X4 Rotation;
	DirectX::XMFLOAT3 Look;
};

struct RenderItem
{
	RenderItem() = default;
	RenderItem(const RenderItem& rhs) = delete;

	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	int ObjCBIndex = -1;
	int PlayerCBIndex = -1;
	int MonsterCBIndex = -1;
	int UICBIndex = -1;

	int NumFramesDirty = gNumFrameResources;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;
	SkinnedModelInstance* SkinnedModelInst = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced parameters.
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};

enum class RenderLayer : int
{
	Opaque = 0,
	Mirrors,
	Reflected,
	Transparent,
	Character,
	Monster,
	Shadow,
	UI,
	Count
};

enum eUIList
{
	Rect,
	Count
};

//void printMatrix(const std::wstring& Name, const int& i, const DirectX::XMMATRIX &M)
//{
//	std::wstring text = Name + std::to_wstring(i) + L"\n";
//	::OutputDebugString(text.c_str());
//
//	for (int j = 0; j < 4; ++j)
//	{
//		for (int k = 0; k < 4; ++k)
//		{
//			std::wstring text =
//				std::to_wstring(M.r[j].m128_f32[k]) + L" ";
//
//			::OutputDebugString(text.c_str());
//		}
//		std::wstring text = L"\n";
//		::OutputDebugString(text.c_str());
//
//	}
//}

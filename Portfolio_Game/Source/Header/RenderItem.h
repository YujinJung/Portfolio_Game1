#pragma once

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
			if (ClipName == "Idle")
			{
				TimePos = 0.0f;
			}
			ClipEnd = true;
		}

		ClipEnd = false;
		if (ClipName == "Idle")
		{
			state = eClipList::Idle;
			ClipEnd = true;
		}
		else if (ClipName == "StartWalking")
			state = eClipList::StartWalking;
		else if (ClipName == "Walking" || ClipName == "WalkingBackward")
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

struct RenderItem
{
	RenderItem() = default;
	RenderItem(const RenderItem& rhs) = delete;

	// World matrix of the shape that describes the object's local space
	// relative to the world space, which defines the position, orientation,
	// and scale of the object in the world.
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	int ObjCBIndex = -1;
	int PlayerCBIndex = -1;
	int MonsterCBIndex = -1;

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

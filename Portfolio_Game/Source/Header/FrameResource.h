#pragma once

#include "../Common/d3dUtil.h"
#include "../Common/MathHelper.h"
#include "../Common/UploadBuffer.h"

struct ObjectConstants
{
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
};

struct PassConstants
{
	DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
    float cbPerObjectPad1 = 0.0f;
    DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
    float NearZ = 0.0f;
    float FarZ = 0.0f;
    float TotalTime = 0.0f;
    float DeltaTime = 0.0f;
	DirectX::XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };

	DirectX::XMFLOAT4 FogColor = { 0.0f, 0.0f, 0.0f, 1.0f };
	float FogStart = 0.0f;
	float FogRange = 0.0f;
    DirectX::XMFLOAT2 cbPerObjectPad2 = { 0.0f, 0.0f };

	Light Lights[MaxLights];
};

struct SkinnedConstants
{
	DirectX::XMFLOAT4X4 BoneTransforms[96];
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
};

struct MonsterContants
{
	DirectX::XMFLOAT4X4 BoneTransforms[10][96];
	DirectX::XMFLOAT4X4 World[10];
	DirectX::XMFLOAT4X4 TexTransform[10];
	int monsterIndex;

	MonsterContants()
	{
		for (int i = 0; i < 10; ++i)
		{
			World[i] = MathHelper::Identity4x4();
			TexTransform[i] = MathHelper::Identity4x4();
		}
	}
};

struct UIConstants
{
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
};

struct Vertex
{
    DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 TexC;

	bool operator==(const Vertex& other) const
	{
		if (Pos.x != other.Pos.x || Pos.y != other.Pos.y || Pos.z != other.Pos.z)
			return false;

		if (Normal.x != other.Normal.x || Normal.y != other.Normal.y || Normal.z != other.Normal.z)
			return false;

		if (TexC.x != other.TexC.x || TexC.y != other.TexC.y)
			return false;

		return true;
	}
};

struct SkinnedVertex
{
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 TexC;
	DirectX::XMFLOAT3 BoneWeights;
	BYTE BoneIndices[4];
	
	uint16_t MaterialIndex;
};

// Stores the resources needed for the CPU to build the command lists
// for a frame.  
struct FrameResource
{
public:
    
    FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount, UINT PlayerCount, UINT MonsterCount, UINT UICount, UINT MonsterUICount);
    FrameResource(const FrameResource& rhs) = delete;
    FrameResource& operator=(const FrameResource& rhs) = delete;
    ~FrameResource();

    // We cannot reset the allocator until the GPU is done processing the commands.
    // So each frame needs their own allocator.
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

    // We cannot update a cbuffer until the GPU is done processing the commands
    // that reference it.  So each frame needs their own cbuffers.
    std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;
	std::unique_ptr<UploadBuffer<SkinnedConstants>> PlayerCB = nullptr;
	std::unique_ptr<UploadBuffer<MonsterContants>> MonsterCB = nullptr;
	std::unique_ptr<UploadBuffer<MaterialConstants>>  MaterialCB = nullptr;
    std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
    std::unique_ptr<UploadBuffer<UIConstants>> UICB = nullptr;
    std::unique_ptr<UploadBuffer<UIConstants>> MonsterUICB = nullptr;

    // Fence value to mark commands up to this fence point.  This lets us
    // check if these frame resources are still in use by the GPU.
    UINT64 Fence = 0;
};

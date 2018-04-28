#pragma once

#include "../Common/d3dUtil.h"
#include "FrameResource.h"

class Materials
{
public:
	Materials();
	~Materials();

	UINT GetSize() const;

	Material* Get(std::string Name);

	void SetMaterial(const std::string& Name, const int& MatIndex, const int& DiffuseSrvHeapIndex, const DirectX::XMFLOAT4& DiffuseAlbedo, const DirectX::XMFLOAT3& FresnelR0, const float& Roughness);

	void Begin(ID3D12Device* device, ID3D12DescriptorHeap* cbvHeap);
	void End();

	void BuildConstantBufferViews(const std::vector<std::unique_ptr<FrameResource>> &mFrameResources, int mMatCbvOffset);

	void UpdateMaterialCB(UploadBuffer<MaterialConstants>* currMaterialCB);

private:
	ID3D12Device * mDevice;
	ID3D12DescriptorHeap* mCbvHeap;

	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;

	bool mInBeginEndPair;
};


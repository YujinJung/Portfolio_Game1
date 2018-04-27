#pragma once

#include "../Common/d3dUtil.h"
#include "FrameResource.h"

class Materials
{
public:
	Materials();
	~Materials();

	void SetMaterial(std::string Name, int MatIndex, int DiffuseSrvHeapIndex, DirectX::XMFLOAT4 DiffuseAlbedo, DirectX::XMFLOAT3 FresnelR0, float Roughness);

	Material* Get(std::string Name);

	UINT GetSize() const;

	void BuildConstantBufferViews(ID3D12Device* device, ID3D12DescriptorHeap* mCbvHeap, const std::vector<std::unique_ptr<FrameResource>> &mFrameResources, int gNumFrameResources, int mMatCbvOffset);

	void UpdateMaterialCB(UploadBuffer<MaterialConstants>* currMaterialCB);

private:
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
};


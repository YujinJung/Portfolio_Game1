#pragma once

#include "SkinnedData.h"
#include "FrameResource.h"
#include "RenderItem.h"

class Materials;
class DXUI
{
public:
	DXUI();
	~DXUI();

	virtual UINT GetSize() const;
	const std::vector<RenderItem*> GetRenderItem(eUIList Type);

	void SetPosition(DirectX::FXMVECTOR inPosition);
	void SetDamageScale(DirectX::FXMVECTOR inEyeLeft, float inScale);

	void BuildConstantBufferViews(ID3D12Device* device, ID3D12DescriptorHeap* mCbvHeap, const std::vector<std::unique_ptr<FrameResource>> &mFrameResources, int mUICbvOffset);
	void BuildRenderItem(std::unordered_map<std::string, std::unique_ptr<MeshGeometry>>& mGeometries, Materials & mMaterials);

	void UpdateUICBs(UploadBuffer<UIConstants>* currUICB, DirectX::XMMATRIX playerWorld, bool mTransformDirty);

private:
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::vector<RenderItem*> mRitems[(int)eUIList::Count];

	WorldTransform mWorldTransform;
	DirectX::XMFLOAT3 UIoffset;
};


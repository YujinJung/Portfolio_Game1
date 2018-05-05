#pragma once
#include "DXUI.h"

class MonsterUI : public DXUI
{
public:
	MonsterUI();
	~MonsterUI();

	virtual UINT GetSize() const;
	const std::vector<RenderItem*> GetRenderItem(eUIList Type)const;

	void SetDamageScale(int cIndex, float inScale);

	void BuildRenderItem(
		std::unordered_map<std::string, std::unique_ptr<MeshGeometry>>& mGeometries,
		Materials & mMaterials, 
		UINT numOfMonster);
	void BuildConstantBufferViews(
		ID3D12Device * device,
		ID3D12DescriptorHeap * mCbvHeap,
		const std::vector<std::unique_ptr<FrameResource>>& mFrameResources,
		int mMonsterUICbvOffset);

	void UpdateUICBs(
		UploadBuffer<UIConstants>* currUICB,
		std::vector<DirectX::XMMATRIX> playerWorlds,
		std::vector<DirectX::XMVECTOR> inEyeLeft,
		bool mTransformDirty);

private:
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::vector<RenderItem*> mRitems[(int)eUIList::Count];

	std::vector<WorldTransform> mWorldTransform;
};


#pragma once
#include "PlayerUI.h"

class MonsterUI : public PlayerUI
{
public:
	MonsterUI();
	~MonsterUI();

	virtual UINT GetSize() const;
	const std::vector<RenderItem*> GetRenderItem(eUIList Type)const;

	void SetDamageScale(int cIndex, float inScale);

	void DeleteMonsterUI(int cIndex);

	void BuildConstantBufferViews(
		ID3D12Device * device,
		ID3D12DescriptorHeap * mCbvHeap,
		const std::vector<std::unique_ptr<FrameResource>>& mFrameResources,
		int mMonsterUICbvOffset);

	void BuildRenderItem(
		std::unordered_map<std::string, std::unique_ptr<MeshGeometry>>& mGeometries,
		Materials & mMaterials,
		std::string monsterName,
		UINT numOfMonster);

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


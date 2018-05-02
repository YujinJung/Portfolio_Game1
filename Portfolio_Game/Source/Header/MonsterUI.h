#pragma once
#include "DXUI.h"

class MonsterUI : public DXUI
{
public:
	MonsterUI();
	~MonsterUI();

	virtual UINT GetSize() const;

	const std::vector<RenderItem*> GetRenderItem(eUIList Type)const;
	void BuildRenderItem(std::unordered_map<std::string, std::unique_ptr<MeshGeometry>>& mGeometries, Materials & mMaterials);
	void UpdateUICBs(UploadBuffer<UIConstants>* currUICB, DirectX::XMMATRIX playerWorld, bool mTransformDirty);

private:
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::vector<RenderItem*> mRitems[(int)eUIList::Count];

	WorldTransform mWorldTransform;
	DirectX::XMFLOAT3 UIoffset;
	int UIIndex = 0;
};


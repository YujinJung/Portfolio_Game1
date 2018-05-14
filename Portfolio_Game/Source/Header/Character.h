#pragma once

#include "Materials.h"
#include "RenderItem.h"
#include "../Common/UploadBuffer.h"

struct CharacterConstants;
struct FrameResource;
class GameTimer;

class Character
{
public:
	Character();
	~Character();

public:
	virtual int GetHealth(int i  = 0) const = 0;
	virtual CharacterInfo& GetCharacterInfo(int cIndex = 0) = 0;
	virtual void Damage(int damage, DirectX::XMVECTOR Position, DirectX::XMVECTOR Look) = 0;

public:
	virtual void BuildConstantBufferViews(
		ID3D12Device* device, ID3D12DescriptorHeap* mCbvHeap, 
		const std::vector<std::unique_ptr<FrameResource>> &mFrameResources, 
		int mChaCbvOffset) = 0;

	virtual void BuildGeometry(
		ID3D12Device * device, 
		ID3D12GraphicsCommandList* cmdList, 
		const std::vector<CharacterVertex>& inVertices,
		const std::vector<std::uint32_t>& inIndices, 
		const SkinnedData& inSkinInfo, std::string geoName) = 0;

	virtual void BuildRenderItem(Materials& mMaterials, std::string matrialPrefix) = 0;
	
	virtual void UpdateCharacterShadows(const Light& mMainLight) = 0;

public: 
	bool mTransformDirty = false;
	
};

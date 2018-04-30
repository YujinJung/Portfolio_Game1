#pragma once
#include "Character.h"

class Monster : public Character
{
public:
	Monster();
	~Monster();

	virtual void BuildConstantBufferViews(ID3D12Device * device, ID3D12DescriptorHeap * mCbvHeap, const std::vector<std::unique_ptr<FrameResource>>& mFrameResources, int mChaCbvOffset) override;

	void UpdateCharacterCBs(FrameResource* mCurrFrameResource, const Light& mMainLight, RenderLayer type, const GameTimer & gt);

private:
	std::string mClipName;
};


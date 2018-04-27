#pragma once

#include "../Common/d3dUtil.h"
#include "FrameResource.h"

class Textures
{
public:
	Textures();
	~Textures();

	UINT GetSize() const;

	void SetTexture(
		const std::string& Name,
		const std::wstring& szFileName);

	void Begin(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, ID3D12DescriptorHeap* cbvHeap);
	void End();

	void BuildConstantBufferViews();

private:
	ID3D12Device * mDevice;
	ID3D12GraphicsCommandList* mCommandList;
	ID3D12DescriptorHeap* mCbvHeap;

	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;

	bool mInBeginEndPair;
};


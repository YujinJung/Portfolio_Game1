#pragma once

#include "FrameResource.h"

class Textures
{
public:
	Textures();
	~Textures();

	UINT GetSize() const;
	int GetTextureIndex(std::string Name) const;

	void SetTexture(
		const std::string& Name,
		const std::wstring& szFileName);
	void SetTexture(
		const std::vector<std::string>& Name,
		const std::vector<std::wstring>& szFileName);

	void Begin(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, ID3D12DescriptorHeap* cbvHeap);
	void End();

	void BuildConstantBufferViews(int mTextureOffset);

private:
	ID3D12Device * mDevice;
	ID3D12GraphicsCommandList* mCommandList;
	ID3D12DescriptorHeap* mCbvHeap;
	 
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::vector<Texture*> mOrderTexture;

	bool mInBeginEndPair;
};


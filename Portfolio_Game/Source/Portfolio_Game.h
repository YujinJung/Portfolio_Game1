#pragma once

#include "d3dApp.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

const int gNumFrameResources = 3;

class Textures;
class Materials;
class Player;
class Monster;

class PortfolioGameApp : public D3DApp
{
public:
	PortfolioGameApp(HINSTANCE hInstance);
	PortfolioGameApp(const PortfolioGameApp& rhs) = delete;
	PortfolioGameApp& operator=(const PortfolioGameApp& rhs) = delete;
	~PortfolioGameApp();

	virtual bool Initialize()override;

private:
	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

	void OnKeyboardInput(const GameTimer& gt);

	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateMaterialCB(const GameTimer& gt);
	void UpdateCharacterCBs(const GameTimer & gt);
	void UpdateObjectShadows(const GameTimer & gt);

	void LoadTextures();
	void BuildDescriptorHeaps();
	void BuildTextureBufferViews();
	void BuildConstantBufferViews();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildShapeGeometry();
	void BuildArcheGeometry(
		const std::vector<std::vector<Vertex>>& outVertices,
		const std::vector<std::vector<std::uint32_t>>& outIndices,
		const std::vector<std::string>& geoName);
	void BuildFbxGeometry();
	void LoadFBXArchitecture(
		FbxLoader &fbx,
		std::vector<Vertex> &outVertices,
		std::vector<unsigned int> &outIndices,
		std::vector<Material> &outMaterial,
		std::string &FileName, const int& archIndex);
	void BuildMaterials();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildRenderItems();
	void BuildArchitecture(
		const FXMMATRIX& world,
		std::string matName,
		std::string architectureName,
		UINT &objCBIndex);
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

private:
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mSkinnedInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mUIInputLayout;
	
	// Pass
	PassConstants mMainPassCB;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mRitems[(int)RenderLayer::Count];
	std::vector<float> HitTime;
	std::vector<float> DelayTime;

	UINT mObjCbvOffset = 0;
	UINT mChaCbvOffset = 0;
	UINT mMonsterCbvOffset = 0;
	UINT mPassCbvOffset = 0;
	UINT mMatCbvOffset = 0;
	UINT mUICbvOffset = 0;
	UINT mMonsterUICbvOffset = 0;
	UINT mTextureOffset = 0;

	UINT mCbvSrvDescriptorSize = 0;

	bool mIsWireframe = false;
	bool mFbxWireframe = false;
	bool mCameraDetach = false; // True - Camera Move with player

	Light mMainLight;

	POINT mLastMousePos;

	Player mPlayer;
	Monster mMonster;
	std::vector<Monster> mMonstersByZone;
	Textures mTextures;
	Materials mMaterials;
};
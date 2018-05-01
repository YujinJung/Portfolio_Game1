//***************************************************************************************
// ShapesApp.cpp by Frank Luna (C) 2015 All Rights Reserved.
// 
// FBXLoaderApp.cpp by Yujin Jung
// 
// Hold down '2' key to view FBX model in wireframe mode
//
// Hold down '1' key to view scene in wireframe mode.
//***************************************************************************************

#include "Textures.h"
#include "Materials.h"
#include "Player.h"
#include "Monster.h"
#include "Portfolio_Game.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
	PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		PortfolioGameApp theApp(hInstance);
		if (!theApp.Initialize())
			return 0;

		return theApp.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}

PortfolioGameApp::PortfolioGameApp(HINSTANCE hInstance)
	: D3DApp(hInstance)
{
}

PortfolioGameApp::~PortfolioGameApp()
{
	if (md3dDevice != nullptr)
		FlushCommandQueue();
}

///
bool PortfolioGameApp::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	// TODO : DELETE
	mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	LoadTextures();
	BuildFbxGeometry();
	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildShapeGeometry();
	BuildMaterials();
	BuildRenderItems();
	BuildFrameResources();

	BuildDescriptorHeaps();
	BuildTextureBufferViews();
	BuildConstantBufferViews();

	BuildPSOs();

	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	return true;
}

void PortfolioGameApp::OnResize()
{
	D3DApp::OnResize();

	// The window resized, so update the aspect ratio and recompute the projection matrix.
	//XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	mPlayer.mCamera.SetProj(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
}

void PortfolioGameApp::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);

	// Cycle through the circular frame resource array.
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	UpdateObjectCBs(gt);
	UpdateCharacterCBs(gt);
	UpdateMainPassCB(gt);
	UpdateObjectShadows(gt);
	UpdateMaterialCB(gt);
}

void PortfolioGameApp::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(cmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	if (mIsWireframe)
	{
		ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque_wireframe"].Get()));
	}
	else
	{
		ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));
	}

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Clear the back buffer and depth buffer.
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	int passCbvIndex = mPassCbvOffset + mCurrFrameResourceIndex;
	auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
	passCbvHandle.Offset(passCbvIndex, mCbvSrvDescriptorSize);

	// Object
	mCommandList->SetGraphicsRootDescriptorTable(3, passCbvHandle);
	DrawRenderItems(mCommandList.Get(), mRitems[(int)RenderLayer::Opaque]);

	// UI
	mCommandList->SetPipelineState(mPSOs["UI"].Get());
	DrawRenderItems(mCommandList.Get(), mPlayer.mUI.GetRenderItem(eUIList::Rect));

	// Character
	if (!mFbxWireframe)
	{
		mCommandList->SetPipelineState(mPSOs["Player"].Get());
	}
	else
	{
		mCommandList->SetPipelineState(mPSOs["Player_wireframe"].Get());
	}
	DrawRenderItems(mCommandList.Get(), mPlayer.GetRenderItem(RenderLayer::Character));

	// Monster
	mCommandList->SetPipelineState(mPSOs["Monster"].Get());
	DrawRenderItems(mCommandList.Get(), mMonster.GetRenderItem(RenderLayer::Monster));

	// Shadow
	mCommandList->OMSetStencilRef(0);
	mCommandList->SetPipelineState(mPSOs["Player_shadow"].Get());
	DrawRenderItems(mCommandList.Get(), mPlayer.GetRenderItem(RenderLayer::Shadow));

	mCommandList->SetPipelineState(mPSOs["Monster_shadow"].Get());
	DrawRenderItems(mCommandList.Get(), mMonster.GetRenderItem(RenderLayer::Shadow));

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Swap the back and front buffers
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// Advance the fence value to mark commands up to this fence point.
	mCurrFrameResource->Fence = ++mCurrentFence;

	// Add an instruction to the command queue to set a new fence point. 
	// Because we are on the GPU timeline, the new fence point won't be 
	// set until the GPU finishes processing all the commands prior to this Signal().
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}


void PortfolioGameApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void PortfolioGameApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void PortfolioGameApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	float dx = XMConvertToRadians(0.25f*static_cast<float>(x - mLastMousePos.x));
	float dy = XMConvertToRadians(0.25f*static_cast<float>(y - mLastMousePos.y));

	if ((btnState & MK_LBUTTON) != 0)
	{
		mPlayer.mCamera.AddPitch(dy);

		// Rotate Camera only
		mPlayer.mCamera.AddYaw(dx);
		//mPlayer.PlayerMove(PlayerMoveList::AddYaw, dx);
	}
	else if ((btnState & MK_RBUTTON) != 0)
	{
		mPlayer.mCamera.AddPitch(dy);

		// Rotate Camera with Player
		mPlayer.mCamera.AddYaw(dx);
		mPlayer.PlayerMove(PlayerMoveList::AddYaw, dx);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;

	mPlayer.UpdateTransformationMatrix();
}

void PortfolioGameApp::OnKeyboardInput(const GameTimer& gt)
{
	const float dt = gt.DeltaTime();

	if (GetAsyncKeyState('7') & 0x8000)
		mIsWireframe = true;
	else if (GetAsyncKeyState('8') & 0x8000)
		mFbxWireframe = true;
	else if (GetAsyncKeyState('9') & 0x8000)
		mCameraDetach = true;
	else if (GetAsyncKeyState('0') & 0x8000)
		mCameraDetach = false;
	else
	{
		mIsWireframe = false;
		mFbxWireframe = false;
	}

	if (GetAsyncKeyState('W') & 0x8000)
	{
		if (!mCameraDetach)
		{
			mPlayer.PlayerMove(PlayerMoveList::Walk, 5.0f * dt);
			mPlayer.SetClipName("playerWalking");
			if (mPlayer.GetCurrentClip() == eClipList::Walking)
			{
				if (mPlayer.isClipEnd())
					mPlayer.SetClipTime(0.0f);
			}
		}
		else
		{
			mPlayer.mCamera.Walk(10.0f * dt);
		}
	}
	else if (GetAsyncKeyState('S') & 0x8000) 
	{
		if (!mCameraDetach)
		{
			mPlayer.PlayerMove(PlayerMoveList::Walk, -5.0f * dt);
			mPlayer.SetClipName("WalkingBackward");
			if (mPlayer.GetCurrentClip() == eClipList::Walking)
			{
				if (mPlayer.isClipEnd())
					mPlayer.SetClipTime(0.0f);
			}
		}
		else
		{
			mPlayer.mCamera.Walk(-10.0f * dt);
		}
	}
	else if(GetAsyncKeyState('1') & 0x8000)
	{
		mPlayer.SetClipTime(0.0f);
		mPlayer.SetClipName("FlyingKick");
	}
	else
	{
		if (mPlayer.isClipEnd())
			mPlayer.SetClipName("Idle");
		/*if (mPlayer.GetCurrentClip() == eClipList::StopWalking && mPlayer.isClipEnd())
			mPlayer.SetClipName("Idle");
		else if (mPlayer.GetCurrentClip() == eClipList::Idle)
			mPlayer.SetClipName("Idle");
		else if(mPlayer.GetCurrentClip() != eClipList::StopWalking)
		{
			float walkRatio = mPlayer.GetCurrentClipTime() / 1.625f;
			walkRatio += 0.1f;
			if (walkRatio >= 1.0f) walkRatio -= 1.0f;
			mPlayer.SetClipName("StopWalking");
			mPlayer.SetClipTime(walkRatio);
		}*/
	}

	if (GetAsyncKeyState('A') & 0x8000)
	{
		if (!mCameraDetach)
			mPlayer.PlayerMove(PlayerMoveList::AddYaw, -1.0f * dt);
		else
			mPlayer.mCamera.WalkSideway(-10.0f * dt);
	}
	else if (GetAsyncKeyState('D') & 0x8000)
	{
		if (!mCameraDetach)
			mPlayer.PlayerMove(PlayerMoveList::AddYaw, 1.0f * dt);
		else
			mPlayer.mCamera.WalkSideway(10.0f * dt);
	}

	mPlayer.UpdateTransformationMatrix();
}

void PortfolioGameApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();

	for (auto& e : mAllRitems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if (e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}

void PortfolioGameApp::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = mPlayer.mCamera.GetView();
	XMMATRIX proj = mPlayer.mCamera.GetProj();

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePosW = mPlayer.mCamera.GetEyePosition3f();
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();
	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	mMainPassCB.Lights[0].Direction = mMainLight.Direction;
	mMainPassCB.Lights[0].Strength = mMainLight.Strength;
	mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void PortfolioGameApp::UpdateMaterialCB(const GameTimer & gt)
{
	auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
	mMaterials.UpdateMaterialCB(currMaterialCB);
}

void PortfolioGameApp::UpdateCharacterCBs(const GameTimer & gt)
{
	mPlayer.UpdateCharacterCBs(mCurrFrameResource, mMainLight, gt);
	mMonster.UpdateMonsterPosition(mPlayer.GetWorldTransform().Position, gt);
	mMonster.UpdateCharacterCBs(mCurrFrameResource, mMainLight, gt);
}

void PortfolioGameApp::UpdateObjectShadows(const GameTimer& gt)
{
	//auto currSkinnedCB = mCurrFrameResource->PlayerCB.get();
	//mCharacter.UpdateCharacterShadows(mMainLight);
	
}


///
void PortfolioGameApp::BuildDescriptorHeaps()
{
	mObjCbvOffset = mTextures.GetSize();
	UINT objCount = (UINT)mAllRitems.size();
	UINT chaCount = mPlayer.GetAllRitemsSize();
	UINT monsterCount = mMonster.GetAllRitemsSize();
	UINT matCount = mMaterials.GetSize();
	UINT uiCount = mPlayer.mUI.GetSize();

	// Need a CBV descriptor for each object for each frame resource,
	// +1 for the perPass CBV for each frame resource.
	// +matCount for the Materials for each frame resources.
	UINT numDescriptors = mObjCbvOffset + (objCount + chaCount + monsterCount + matCount + uiCount + 1) * gNumFrameResources;

	// Save an offset to the start of the pass CBVs.  These are the last 3 descriptors.
	mChaCbvOffset = objCount * gNumFrameResources + mObjCbvOffset;
	mMonsterCbvOffset = chaCount * gNumFrameResources + mChaCbvOffset;
	mMatCbvOffset = monsterCount * gNumFrameResources + mMonsterCbvOffset;
	mPassCbvOffset = matCount * gNumFrameResources + mMatCbvOffset;
	mUICbvOffset = 1 * gNumFrameResources + mPassCbvOffset;

	// mPassCbvOffset + (passSize)
	// passSize = 1 * gNumFrameResources
	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NumDescriptors = numDescriptors;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&cbvHeapDesc,
		IID_PPV_ARGS(&mCbvHeap)));
}

void PortfolioGameApp::BuildTextureBufferViews()
{
	mTextures.Begin(md3dDevice.Get(), mCommandList.Get(), mCbvHeap.Get());
	mTextures.BuildConstantBufferViews();
	mTextures.End();
}

void PortfolioGameApp::BuildConstantBufferViews()
{
	// Object 
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT objCount = (UINT)mAllRitems.size();

	// Need a CBV descriptor for each object for each frame resource.
	for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
	{
		auto objectCB = mFrameResources[frameIndex]->ObjectCB->Resource();
		for (UINT i = 0; i < objCount; ++i)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = objectCB->GetGPUVirtualAddress();

			// Offset to the ith object constant buffer in the buffer.
			cbAddress += i * objCBByteSize;

			// Offset to the object cbv in the descriptor heap.
			int heapIndex = mObjCbvOffset + frameIndex * objCount + i;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, mCbvSrvDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = objCBByteSize;

			md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
		}
	}

	// Material 
	mMaterials.Begin(md3dDevice.Get(), mCbvHeap.Get());
	mMaterials.BuildConstantBufferViews(
		mFrameResources,
		mMatCbvOffset);
	mMaterials.End();
	
	// Pass - 4
	// Last three descriptors are the pass CBVs for each frame resource.
	UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));
	for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
	{
		auto passCB = mFrameResources[frameIndex]->PassCB->Resource();
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCB->GetGPUVirtualAddress();

		// Offset to the pass cbv in the descriptor heap.
		int heapIndex = mPassCbvOffset + frameIndex;
		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
		handle.Offset(heapIndex, mCbvSrvDescriptorSize);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = cbAddress;
		cbvDesc.SizeInBytes = passCBByteSize;

		md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
	}

	// Character
	mPlayer.BuildConstantBufferViews(
		md3dDevice.Get(),
		mCbvHeap.Get(),
		mFrameResources,
		mChaCbvOffset);

	mMonster.BuildConstantBufferViews(
		md3dDevice.Get(),
		mCbvHeap.Get(),
		mFrameResources,
		mMonsterCbvOffset);

	// UI
	mPlayer.mUI.BuildConstantBufferViews(
		md3dDevice.Get(),
		mCbvHeap.Get(),
		mFrameResources,
		mUICbvOffset);
}

void PortfolioGameApp::BuildRootSignature()
{
	const int tableNumber = 7;
	CD3DX12_DESCRIPTOR_RANGE cbvTable[tableNumber];

	for (int i = 0; i < tableNumber - 1; ++i)
	{
		cbvTable[i].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, i);
	}

	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	// Root parameter can be a table, root descriptor or root constants.
	// Objects, Materials, Passes
	CD3DX12_ROOT_PARAMETER slotRootParameter[tableNumber];

	// Create root CBVs.
	slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
	for (int i = 1; i < tableNumber; ++i)
	{
		slotRootParameter[i].InitAsDescriptorTable(1, &cbvTable[i - 1]);
	}

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(tableNumber, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));

}

void PortfolioGameApp::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(
			md3dDevice.Get(),
			1, (UINT)mAllRitems.size(),
			mMaterials.GetSize(),
			mPlayer.GetAllRitemsSize(),mMonster.GetNumOfCharacter(),  mMonster.GetAllRitemsSize(), mPlayer.mUI.GetSize()));
	}
}

void PortfolioGameApp::BuildShadersAndInputLayout()
{
	const D3D_SHADER_MACRO skinnedDefines[] =
	{
		"SKINNED", "1",
		NULL, NULL
	};

	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["skinnedVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", skinnedDefines, "VS", "vs_5_1");
	mShaders["monsterVS"] = d3dUtil::CompileShader(L"Shaders\\Monster.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["uiVS"] = d3dUtil::CompileShader(L"Shaders\\UI.hlsl", nullptr, "VS", "vs_5_1");

	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["skinnedPS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", skinnedDefines, "PS", "ps_5_1");
	mShaders["monsterPS"] = d3dUtil::CompileShader(L"Shaders\\Monster.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["uiPS"] = d3dUtil::CompileShader(L"Shaders\\UI.hlsl", nullptr, "PS", "ps_5_1");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	mSkinnedInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "WEIGHTS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BONEINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void PortfolioGameApp::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	//
	// PSO for opaque objects.
	//
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};

	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

	// PSO for ui
	D3D12_GRAPHICS_PIPELINE_STATE_DESC UIPsoDesc = opaquePsoDesc;
	UIPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["uiVS"]->GetBufferPointer()),
		mShaders["uiVS"]->GetBufferSize()
	};
	UIPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["uiPS"]->GetBufferPointer()),
		mShaders["uiPS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&UIPsoDesc, IID_PPV_ARGS(&mPSOs["UI"])));

	// PSO for Player 
	D3D12_GRAPHICS_PIPELINE_STATE_DESC PlayerPsoDesc = opaquePsoDesc;
	PlayerPsoDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };
	PlayerPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["skinnedVS"]->GetBufferPointer()),
		mShaders["skinnedVS"]->GetBufferSize()
	};
	PlayerPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["skinnedPS"]->GetBufferPointer()),
		mShaders["skinnedPS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&PlayerPsoDesc, IID_PPV_ARGS(&mPSOs["Player"])));

	// PSO for Monster
	D3D12_GRAPHICS_PIPELINE_STATE_DESC MonsterPsoDesc = PlayerPsoDesc;
	MonsterPsoDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };
	MonsterPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["monsterVS"]->GetBufferPointer()),
		mShaders["monsterVS"]->GetBufferSize()
	};
	MonsterPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["monsterPS"]->GetBufferPointer()),
		mShaders["monsterPS"]->GetBufferSize()
	};
	
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&MonsterPsoDesc, IID_PPV_ARGS(&mPSOs["Monster"])));

	//
	// PSO for skinned wireframe objects.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC PlayerWireframePsoDesc = PlayerPsoDesc;
	PlayerWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&PlayerWireframePsoDesc, IID_PPV_ARGS(&mPSOs["Player_wireframe"])));


	//
	// PSO for opaque wireframe objects.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
	opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSOs["opaque_wireframe"])));

	//
	// PSO for Shadow
	D3D12_RENDER_TARGET_BLEND_DESC shadowBlendDesc;
	shadowBlendDesc.BlendEnable = true;
	shadowBlendDesc.LogicOpEnable = false;
	shadowBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	shadowBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	shadowBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	shadowBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	shadowBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	shadowBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	shadowBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	shadowBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	D3D12_DEPTH_STENCIL_DESC shadowDSS;
	shadowDSS.DepthEnable = true;
	shadowDSS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	shadowDSS.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	shadowDSS.StencilEnable = true;
	shadowDSS.StencilReadMask = 0xff;
	shadowDSS.StencilWriteMask = 0xff;

	shadowDSS.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDSS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDSS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_INCR;
	shadowDSS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	// We are not rendering backfacing polygons, so these settings do not matter.
	shadowDSS.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDSS.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDSS.BackFace.StencilPassOp = D3D12_STENCIL_OP_INCR;
	shadowDSS.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowPsoDesc = opaquePsoDesc;
	shadowPsoDesc.DepthStencilState = shadowDSS;
	shadowPsoDesc.BlendState.RenderTarget[0] = shadowBlendDesc;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&shadowPsoDesc, IID_PPV_ARGS(&mPSOs["shadow"])));


	// Skinned Shadow
	D3D12_GRAPHICS_PIPELINE_STATE_DESC PlayerShadowPsoDesc = shadowPsoDesc;
	PlayerShadowPsoDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };
	PlayerShadowPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["skinnedVS"]->GetBufferPointer()),
		mShaders["skinnedVS"]->GetBufferSize()
	};
	PlayerShadowPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["skinnedPS"]->GetBufferPointer()),
		mShaders["skinnedPS"]->GetBufferSize()
	};
	PlayerShadowPsoDesc.DepthStencilState = shadowDSS;
	PlayerShadowPsoDesc.BlendState.RenderTarget[0] = shadowBlendDesc;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&PlayerShadowPsoDesc, IID_PPV_ARGS(&mPSOs["Player_shadow"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC MonsterShadowPsoDesc = PlayerShadowPsoDesc;
	MonsterShadowPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["monsterVS"]->GetBufferPointer()),
		mShaders["monsterVS"]->GetBufferSize()
	};
	
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&MonsterShadowPsoDesc, IID_PPV_ARGS(&mPSOs["Monster_shadow"])));
}


///
void PortfolioGameApp::BuildShapeGeometry()
{
	mMainLight.Direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainLight.Strength = { 0.6f, 0.6f, 0.6f };

	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.5f, 0.5f, 1.5f, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
	GeometryGenerator::MeshData sphere = geoGen.CreateGeosphere(0.5f, 3);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);

	//
	// We are concatenating all the geometry into one big vertex/index buffer.  So
	// define the regions in the buffer each submesh covers.
	//

	// Cache the vertex offsets to each object in the concatenated vertex buffer.
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size();
	UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();

	// Cache the starting index for each object in the concatenated index buffer.
	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size();
	UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
	UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();

	// Define the SubmeshGeometry that cover different 
	// regions of the vertex/index buffers.
	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

	//
	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.
	//

	auto totalVertexCount =
		box.Vertices.size() +
		grid.Vertices.size() +
		sphere.Vertices.size() +
		cylinder.Vertices.size();

	std::vector<Vertex> vertices(totalVertexCount);

	UINT k = 0;
	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Normal = box.Vertices[i].Normal;
		vertices[k].TexC = box.Vertices[i].TexC;
	}

	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Normal = grid.Vertices[i].Normal;
		vertices[k].TexC = grid.Vertices[i].TexC;
	}

	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Normal = sphere.Vertices[i].Normal;
		vertices[k].TexC = sphere.Vertices[i].TexC;
	}

	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Normal = cylinder.Vertices[i].Normal;
		vertices[k].TexC = cylinder.Vertices[i].TexC;
	}

	std::vector<std::uint32_t> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
	indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "shapeGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}

void PortfolioGameApp::BuildFbxGeometry()
{
	FbxLoader fbx;

	std::vector<SkinnedVertex> outVertices;
	std::vector<std::uint32_t> outIndices;
	std::vector<Material> outMaterial;
	SkinnedData outSkinnedInfo;

	//std::string FileName = "../Resource/FBX/Capoeira.FBX";
	std::string FileName = "../Resource/FBX/Character/";
	fbx.LoadFBX(outVertices, outIndices, outSkinnedInfo, "Idle", outMaterial, FileName);

	FileName = "../Resource/FBX/Character/";
	fbx.LoadFBX(outSkinnedInfo, "playerWalking", FileName);

	FileName = "../Resource/FBX/Character/";
	fbx.LoadFBX(outSkinnedInfo, "Kick", FileName);

	FileName = "../Resource/FBX/Character/";
	fbx.LoadFBX(outSkinnedInfo, "FlyingKick", FileName);

	/*FileName = "../Resource/FBX/Character/";
	fbx.LoadFBX(outSkinnedInfo, "StartWalking", FileName);

	FileName = "../Resource/FBX/Character/";
	fbx.LoadFBX(outSkinnedInfo, "StopWalking", FileName);*/
	
	FileName = "../Resource/FBX/Character/";
	fbx.LoadFBX(outSkinnedInfo, "WalkingBackward", FileName);

	mPlayer.BuildGeometry(md3dDevice.Get(), mCommandList.Get(), outVertices, outIndices, outSkinnedInfo, "playerGeo");

	// Begin
	mTextures.Begin(md3dDevice.Get(), mCommandList.Get(), mCbvHeap.Get());
	// Load Texture and Material
	int MatIndex = mMaterials.GetSize();
	for (int i = 0; i < outMaterial.size(); ++i)
	{
		std::string TextureName;
		// Load Texture 
		if (!outMaterial[i].Name.empty())
		{
			TextureName = "texture_";
			TextureName.push_back(mTextures.GetSize() +48);
			std::wstring TextureFileName;
			TextureFileName.assign(outMaterial[i].Name.begin(), outMaterial[i].Name.end());

			mTextures.SetTexture(
				TextureName,
				TextureFileName);
		}

		// Load Material
		std::string MaterialName = "material_";
		MaterialName.push_back(MatIndex + 48);

		mMaterials.SetMaterial(
			MaterialName,
			MatIndex++,
			mTextures.GetTextureIndex(TextureName),
			outMaterial[i].DiffuseAlbedo,
			outMaterial[i].FresnelR0,
			outMaterial[i].Roughness);
	}
	mTextures.End();

	outVertices.clear();
	outIndices.clear();
	outMaterial.clear();
	outSkinnedInfo.clear();

	FileName = "../Resource/FBX/Monster/";
	fbx.LoadFBX(outVertices, outIndices, outSkinnedInfo, "Idle", outMaterial, FileName);

	FileName = "../Resource/FBX/Monster/";
	fbx.LoadFBX(outSkinnedInfo, "Walking", FileName);

	FileName = "../Resource/FBX/Monster/";
	fbx.LoadFBX(outSkinnedInfo, "MAttack1", FileName);

	mMonster.BuildGeometry(md3dDevice.Get(), mCommandList.Get(), outVertices, outIndices, outSkinnedInfo, "MonsterGeo");

	// Begin
	mTextures.Begin(md3dDevice.Get(), mCommandList.Get(), mCbvHeap.Get());
	// Load Texture and Material
	MatIndex = mMaterials.GetSize();
	for (int i = 0; i < outMaterial.size(); ++i)
	{
		// Load Texture 
		if (!outMaterial[i].Name.empty())
		{
			std::string TextureName;
			TextureName = "monsterTex_";
			TextureName.push_back(mTextures.GetSize() + 48);
			std::wstring TextureFileName;
			TextureFileName.assign(outMaterial[i].Name.begin(), outMaterial[i].Name.end());

			mTextures.SetTexture(
				TextureName,
				TextureFileName);

			// Load Material
			std::string MaterialName = "monsterMat_";
			MaterialName.push_back(MatIndex + 48);

			mMaterials.SetMaterial(
				MaterialName,
				MatIndex++,
				mTextures.GetTextureIndex(TextureName),
				outMaterial[i].DiffuseAlbedo,
				outMaterial[i].FresnelR0,
				outMaterial[i].Roughness);
		}
	}
	mTextures.End();
}

void PortfolioGameApp::LoadTextures()
{
	mTextures.Begin(md3dDevice.Get(), mCommandList.Get(), mCbvHeap.Get());

	mTextures.SetTexture(
		"bricksTex",
		L"../Resource/Textures/bricks.dds");

	mTextures.SetTexture(
		"bricks3Tex",
		L"../Resource/Textures/bricks3.dds");

	mTextures.SetTexture(
		"stoneTex",
		L"../Resource/Textures/stone.dds");

	mTextures.SetTexture(
		"tileTex",
		L"../Resource/Textures/tile.dds");

	mTextures.SetTexture(
		"grassTex",
		L"../Resource/Textures/grass.dds");

	mTextures.SetTexture(
		"iceTex",
		L"../Resource/Textures/ice.dds");

	mTextures.SetTexture(
		"sampleTex",
		L"../Resource/Textures/sample.jpg");

	mTextures.End();
}

void PortfolioGameApp::BuildMaterials()
{
	int MatIndex = mMaterials.GetSize();

	mMaterials.SetMaterial(
		"bricks0",
		MatIndex++, 
		mTextures.GetTextureIndex("bricksTex"), 
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), 
		XMFLOAT3(0.02f, 0.02f, 0.02f), 
		0.1f);

	mMaterials.SetMaterial(
		"bricks3",
		MatIndex++, 
		mTextures.GetTextureIndex("bricks3Tex"), 
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), 
		XMFLOAT3(0.02f, 0.02f, 0.02f), 
		0.1f);

	mMaterials.SetMaterial(
		"stone0", 
		MatIndex++, 
		mTextures.GetTextureIndex("stoneTex"), 
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), 
		XMFLOAT3(0.05f, 0.05f, 0.05), 
		0.3f);

	mMaterials.SetMaterial(
		"tile0", 
		MatIndex++, 
		mTextures.GetTextureIndex("tileTex"), 
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), 
		XMFLOAT3(0.02f, 0.02f, 0.02f), 
		0.2f);

	mMaterials.SetMaterial(
		"grass0",
		MatIndex++,
		mTextures.GetTextureIndex("grassTex"),
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		XMFLOAT3(0.05f, 0.02f, 0.02f),
		0.1f);

	mMaterials.SetMaterial(
		"ice0",
		MatIndex++,
		mTextures.GetTextureIndex("iceTex"),
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		XMFLOAT3(0.05f, 0.02f, 0.02f),
		0.1f);

	mMaterials.SetMaterial(
		"sample",
		MatIndex++,
		mTextures.GetTextureIndex("sampleTex"),
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		XMFLOAT3(0.05f, 0.02f, 0.02f),
		0.1f);

	mMaterials.SetMaterial(
		"shadow0", 
		MatIndex++, 
		mTextures.GetTextureIndex("grassTex"), 
		XMFLOAT4(0.0f, 0.0f, 0.0f, 0.5f), 
		XMFLOAT3(0.001f, 0.001f, 0.001f), 
		0.0f);
}

void PortfolioGameApp::BuildRenderItems()
{
	UINT objCBIndex = 0;

	auto gridRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&gridRitem->World, XMMatrixScaling(10.0f, 2.0f, 10.0f));
	XMStoreFloat4x4(&gridRitem->TexTransform, XMMatrixScaling(8.0f, 80.0f, 1.0f));
	gridRitem->ObjCBIndex = objCBIndex++;
	gridRitem->Mat = mMaterials.Get("grass0");
	gridRitem->Geo = mGeometries["shapeGeo"].get();
	gridRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
	mRitems[(int)RenderLayer::Opaque].push_back(gridRitem.get());
	mAllRitems.push_back(std::move(gridRitem));

	mPlayer.BuildRenderItem(mMaterials, "material_0");

	mPlayer.mUI.BuildRenderItem(mGeometries, mMaterials);

	mMonster.BuildRenderItem(mMaterials, "monsterMat_1");
}


///
void PortfolioGameApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	// For each render item...
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		// Offset to the CBV in the descriptor heap for this object and for this frame resource.
		// ri->ObjCBIndex = object number 0, 1, 2, --- , n
		CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		tex.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);

		UINT cbvIndex = mObjCbvOffset + mCurrFrameResourceIndex * (UINT)mAllRitems.size() + ri->ObjCBIndex;
		auto cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		cbvHandle.Offset(cbvIndex, mCbvSrvDescriptorSize);

		UINT matCbvIndex = mMatCbvOffset + mCurrFrameResourceIndex * mMaterials.GetSize() + ri->Mat->MatCBIndex;
		auto matCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		matCbvHandle.Offset(matCbvIndex, mCbvSrvDescriptorSize);

		UINT skinnedIndex = mChaCbvOffset + mCurrFrameResourceIndex * mPlayer.GetAllRitemsSize() + ri->PlayerCBIndex;
		auto skinCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		skinCbvHandle.Offset(skinnedIndex, mCbvSrvDescriptorSize);

		UINT monsterIndex = mMonsterCbvOffset + mCurrFrameResourceIndex * mMonster.GetAllRitemsSize() + ri->MonsterCBIndex;
		auto monsterCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		monsterCbvHandle.Offset(monsterIndex, mCbvSrvDescriptorSize);

		UINT uiIndex = mUICbvOffset + mCurrFrameResourceIndex * mPlayer.mUI.GetSize() + ri->ObjCBIndex;
		auto uiCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		uiCbvHandle.Offset(uiIndex, mCbvSrvDescriptorSize);

		cmdList->SetGraphicsRootDescriptorTable(0, tex);
		cmdList->SetGraphicsRootDescriptorTable(1, cbvHandle);
		cmdList->SetGraphicsRootDescriptorTable(2, matCbvHandle);
		cmdList->SetGraphicsRootDescriptorTable(6, uiCbvHandle);
		if (ri->PlayerCBIndex >= 0)
		{
			cmdList->SetGraphicsRootDescriptorTable(4, skinCbvHandle);
		}
		else if (ri->MonsterCBIndex >= 0)
		{
			cmdList->SetGraphicsRootDescriptorTable(5, monsterCbvHandle);
		}

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> PortfolioGameApp::GetStaticSamplers()
{
	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}

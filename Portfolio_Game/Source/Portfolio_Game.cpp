//***************************************************************************************
// ShapesApp.cpp by Frank Luna (C) 2015 All Rights Reserved.
// 
// FBXLoaderApp.cpp by Yujin Jung
// 
// Hold down '2' key to view FBX model in wireframe mode
//
// Hold down '1' key to view scene in wireframe mode.
//***************************************************************************************

#include <random>
#include <chrono>
#include "MathHelper.h"
#include "UploadBuffer.h"
#include "RenderItem.h"
#include "SkinnedData.h"
#include "FrameResource.h"
#include "GeometryGenerator.h"

#include "Player.h"
#include "Monster.h"
#include "Textures.h"
#include "Materials.h"
#include "FBXLoader.h"
#include "TextureLoader.h"

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
	: D3DApp(hInstance),
	HitTime((int)eUIList::Count, 0.0f),
	DelayTime((int)eUIList::Count, 0.0f)
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
	mCommandList->SetGraphicsRootDescriptorTable(0, mCbvHeap->GetGPUDescriptorHandleForHeapStart());

	int passCbvIndex = mPassCbvOffset + mCurrFrameResourceIndex;
	auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
	passCbvHandle.Offset(passCbvIndex, mCbvSrvDescriptorSize);

	// Object
	mCommandList->SetGraphicsRootDescriptorTable(3, passCbvHandle);
	CD3DX12_GPU_DESCRIPTOR_HANDLE skyTexDescriptor(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
	skyTexDescriptor.Offset(mTextureOffset, mCbvSrvDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(8, skyTexDescriptor);
	DrawRenderItems(mCommandList.Get(), mRitems[(int)RenderLayer::Opaque]);
	DrawRenderItems(mCommandList.Get(), mRitems[(int)RenderLayer::Architecture]);
	mCommandList->SetPipelineState(mPSOs["sky"].Get());
	DrawRenderItems(mCommandList.Get(), mRitems[(int)RenderLayer::Sky]);

	//// Sky
	//mCommandList->SetPipelineState(mPSOs["sky"].Get());
	//DrawRenderItems(mCommandList.Get(), mRitems[(int)RenderLayer::Sky]);

	// UI
	mCommandList->SetPipelineState(mPSOs["UI"].Get());
	DrawRenderItems(mCommandList.Get(), mPlayer.mUI.GetRenderItem(eUIList::Rect));
	DrawRenderItems(mCommandList.Get(), mPlayer.mUI.GetRenderItem(eUIList::I_Kick));
	DrawRenderItems(mCommandList.Get(), mPlayer.mUI.GetRenderItem(eUIList::I_Punch));
	mCommandList->SetPipelineState(mPSOs["MonsterUI"].Get());
	DrawRenderItems(mCommandList.Get(), mMonster.mMonsterUI.GetRenderItem(eUIList::Rect));

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
	}
	else if ((btnState & MK_RBUTTON) != 0)
	{
		mPlayer.mCamera.AddPitch(dy);

		// Rotate Camera with Player
		mPlayer.mCamera.AddYaw(dx);
		if(!mCameraDetach)
			mPlayer.UpdatePlayerPosition(PlayerMoveList::AddYaw, dx);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;

	mPlayer.UpdateTransformationMatrix();
}

void PortfolioGameApp::OnKeyboardInput(const GameTimer& gt)
{
	const float dt = gt.DeltaTime();
	bool isForward = true;
	bool isBackward = true;

	XMVECTOR Look = mPlayer.GetCharacterInfo().mMovement.GetPlayerLook();
	auto playerBoundForward = mPlayer.GetCharacterInfo().mBoundingBox;
	auto playerBoundBackward = playerBoundForward;

	playerBoundForward.Center.x += Look.m128_f32[0];
	playerBoundForward.Center.y += Look.m128_f32[1];
	playerBoundForward.Center.z += Look.m128_f32[2];

	playerBoundBackward.Center.x -= Look.m128_f32[0];
	playerBoundBackward.Center.y -= Look.m128_f32[1];
	playerBoundBackward.Center.z -= Look.m128_f32[2];

	// Architecture
	for (auto&e : mRitems[(int)RenderLayer::Architecture])
	{
		if (playerBoundForward.Contains(e->Bounds) == ContainmentType::INTERSECTS)
			isForward = false;
		if (playerBoundBackward.Contains(e->Bounds) == ContainmentType::INTERSECTS)
			isBackward = false;
	}

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
			mPlayer.SetClipName("playerWalking");

			if(isForward)
				mPlayer.UpdatePlayerPosition(PlayerMoveList::Walk, 7.0f * dt);
			isForward = true;

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
	else if (GetAsyncKeyState('F') & 0x8000)
	{
		if (!mCameraDetach)
		{
			mPlayer.SetClipName("run");

			if(isForward)
				mPlayer.UpdatePlayerPosition(PlayerMoveList::Walk, 15.0f * dt);
			isForward = true;

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
			mPlayer.SetClipName("WalkingBackward");

			if(isBackward)
				mPlayer.UpdatePlayerPosition(PlayerMoveList::Walk, -5.0f * dt);
			isBackward = true;

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
		// Kick Delay, 5 seconds
		if (gt.TotalTime() - HitTime[(int)eUIList::I_Punch] > 5.0f)
		{
			mPlayer.SetClipTime(0.0f);
			mPlayer.Attack(mMonster, "Hook");
			HitTime[(int)eUIList::I_Punch] = gt.TotalTime();
		}
	}
	else if (GetAsyncKeyState('2') & 0x8000)
	{
		// Hook Delay, 3 seconds
		if (gt.TotalTime() - HitTime[(int)eUIList::I_Kick] > 3.0f)
		{
			mPlayer.SetClipTime(0.0f);
			mPlayer.Attack(mMonster, "Kick"); 
			HitTime[(int)eUIList::I_Kick] = gt.TotalTime();
		}
	}
	else
	{
		if (mPlayer.isClipEnd())
			mPlayer.SetClipName("Idle");
	}

	if (GetAsyncKeyState('A') & 0x8000)
	{
		if (!mCameraDetach)
			mPlayer.UpdatePlayerPosition(PlayerMoveList::AddYaw, -1.0f * dt);
		else
			mPlayer.mCamera.WalkSideway(-10.0f * dt);
	}
	else if (GetAsyncKeyState('D') & 0x8000)
	{
		if (!mCameraDetach)
			mPlayer.UpdatePlayerPosition(PlayerMoveList::AddYaw, 1.0f * dt);
		else
			mPlayer.mCamera.WalkSideway(10.0f * dt);
	}

	mPlayer.UpdateTransformationMatrix();

	// Update Remaining Time
	float totalTime = gt.TotalTime();
	for (int i = (int)eUIList::I_Punch; i < (int)eUIList::Count; ++i)
	{
		// 10.0 is max delay time
		float Delay = totalTime - HitTime[i];
		if (Delay > 10.0f)
			continue;

		DelayTime[i] = totalTime - HitTime[i];
	}
}

void PortfolioGameApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	auto playerBounds = mPlayer.GetCharacterInfo().mBoundingBox;
	XMVECTOR playerPos = mPlayer.GetCharacterInfo().mMovement.GetPlayerPosition();
	static int i = 0;

	for (auto& e : mAllRitems)
	{
		//auto contain = playerBounds.Contains(e->Bounds);
		//if (contain == ContainmentType::INTERSECTS)
		//{
		//	// player - pass = pass -> player
		//	XMVECTOR objectPos = XMLoadFloat3(&e->Bounds.Center);
		//	objectPos.m128_f32[1] = 0.0f;
		//	XMVECTOR D = XMVector3Normalize(playerPos - objectPos);
		//	mPlayer.GetCharacterInfo().mMovement.SetPlayerPosition(XMVectorAdd(playerPos, D));
		//}

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
	mMainPassCB.FogColor = { 0.8f, 0.8f, 0.8f, 0.5f };
	mMainPassCB.FogRange = 200.0f;
	mMainPassCB.FogStart = 20.0f;

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
	static bool playerDeathCamFinished = false;
	XMVECTOR PlayerPos = mPlayer.GetCharacterInfo().mMovement.GetPlayerPosition();
	
	if (mPlayer.GetHealth() <= 0 && !playerDeathCamFinished)
	{
		mCameraDetach = true;

		XMVECTOR CameraPos= mPlayer.mCamera.GetEyePosition();

		if (MathHelper::getDistance(PlayerPos, CameraPos) < 50.0f)
		{
			mPlayer.mCamera.Walk(-0.5f);
			mPlayer.mCamera.UpdateViewMatrix();
		}
		else
		{
			playerDeathCamFinished = true;
		}
	}

	static float lastTime = gt.TotalTime();
	if (gt.TotalTime() - lastTime > 0.04f)
	{
		mMonster.UpdateMonsterPosition(mPlayer, gt);
		lastTime = gt.TotalTime();
	}
	mMonster.UpdateCharacterCBs(mCurrFrameResource, mMainLight, gt);
	mPlayer.UpdateCharacterCBs(mCurrFrameResource, mMainLight, DelayTime, gt);
	
	
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
	UINT monsterUICount = mMonster.GetUISize();

	// Need a CBV descriptor for each object for each frame resource,
	// +1 for the perPass CBV for each frame resource.
	// +matCount for the Materials for each frame resources.
	UINT numDescriptors = mObjCbvOffset + 1 + (objCount + chaCount + monsterCount + matCount + uiCount + monsterUICount + 1) * gNumFrameResources;

	// Save an offset to the start of the pass CBVs.  These are the last 3 descriptors.
	mChaCbvOffset = objCount * gNumFrameResources + mObjCbvOffset;
	mMonsterCbvOffset = chaCount * gNumFrameResources + mChaCbvOffset;
	mMatCbvOffset = monsterCount * gNumFrameResources + mMonsterCbvOffset;
	mPassCbvOffset = matCount * gNumFrameResources + mMatCbvOffset;
	mUICbvOffset = 1 * gNumFrameResources + mPassCbvOffset;
	mMonsterUICbvOffset = uiCount * gNumFrameResources + mUICbvOffset;
	mTextureOffset = monsterUICount * gNumFrameResources + mMonsterUICbvOffset;

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
	mTextures.BuildConstantBufferViews(mTextureOffset);
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

	mMonster.mMonsterUI.BuildConstantBufferViews(
		md3dDevice.Get(),
		mCbvHeap.Get(),
		mFrameResources,
		mMonsterUICbvOffset);
}

void PortfolioGameApp::BuildRootSignature()
{
	const int texTableNumber = 2;
	const int cbvTableNumber = 7;
	const int tableNumber = texTableNumber + cbvTableNumber;

	CD3DX12_DESCRIPTOR_RANGE cbvTable[cbvTableNumber];
	CD3DX12_DESCRIPTOR_RANGE texTable[texTableNumber];

	texTable[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	for (int i = 0; i < cbvTableNumber; ++i)
	{
		cbvTable[i].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, i);
	}
	texTable[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

	// Root parameter can be a table, root descriptor or root constants.
	// Objects, Materials, Passes
	CD3DX12_ROOT_PARAMETER slotRootParameter[tableNumber];

	// Create root CBVs.
	slotRootParameter[0].InitAsDescriptorTable(1, &texTable[0], D3D12_SHADER_VISIBILITY_PIXEL);
	for (int i = 1; i < cbvTableNumber + 1; ++i)
	{
		slotRootParameter[i].InitAsDescriptorTable(1, &cbvTable[i - 1]);
	}
	slotRootParameter[cbvTableNumber + 1].InitAsDescriptorTable(1, &texTable[1], D3D12_SHADER_VISIBILITY_PIXEL);

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
			mPlayer.GetAllRitemsSize(),
			mMonster.GetAllRitemsSize(),
			mPlayer.mUI.GetSize(),
			mMonster.GetUISize()));
	}
}

void PortfolioGameApp::BuildShadersAndInputLayout()
{
	const D3D_SHADER_MACRO skinnedDefines[] =
	{
		"SKINNED", "1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO playerUIDefines[] =
	{
		"PLAYER", "1",
		NULL, NULL
	};
	const D3D_SHADER_MACRO monsterUIDefines[] =
	{
		"MONSTER", "2",
		NULL, NULL
	};

	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["skinnedVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", skinnedDefines, "VS", "vs_5_1");
	mShaders["monsterVS"] = d3dUtil::CompileShader(L"Shaders\\Monster.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["uiVS"] = d3dUtil::CompileShader(L"Shaders\\UI.hlsl", playerUIDefines, "VS", "vs_5_1");
	mShaders["monsterUIVS"] = d3dUtil::CompileShader(L"Shaders\\UI.hlsl", monsterUIDefines, "VS", "vs_5_1");
	mShaders["skyVS"] = d3dUtil::CompileShader(L"Shaders\\Sky.hlsl", nullptr, "VS", "vs_5_1");

	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["skinnedPS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", skinnedDefines, "PS", "ps_5_1");
	mShaders["monsterPS"] = d3dUtil::CompileShader(L"Shaders\\Monster.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["uiPS"] = d3dUtil::CompileShader(L"Shaders\\UI.hlsl", playerUIDefines, "PS", "ps_5_1");
	mShaders["skyPS"] = d3dUtil::CompileShader(L"Shaders\\Sky.hlsl", nullptr, "PS", "ps_5_1");

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

	mUIInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "ROW", 0, DXGI_FORMAT_R32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
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


	// PSO for ui
	D3D12_GRAPHICS_PIPELINE_STATE_DESC UIPsoDesc = opaquePsoDesc;
	UIPsoDesc.InputLayout = { mUIInputLayout.data(), (UINT)mUIInputLayout.size() };
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

	D3D12_GRAPHICS_PIPELINE_STATE_DESC MonsterUIPsoDesc = UIPsoDesc;
	MonsterUIPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["monsterUIVS"]->GetBufferPointer()),
		mShaders["monsterUIVS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&MonsterUIPsoDesc, IID_PPV_ARGS(&mPSOs["MonsterUI"])));


	// PSO for skinned wireframe objects.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC PlayerWireframePsoDesc = PlayerPsoDesc;
	PlayerWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&PlayerWireframePsoDesc, IID_PPV_ARGS(&mPSOs["Player_wireframe"])));

	// PSO for opaque wireframe objects.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
	opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSOs["opaque_wireframe"])));


	// PSO for sky.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPsoDesc = opaquePsoDesc;

	// The camera is inside the sky sphere, so just turn off culling.
	skyPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	skyPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	skyPsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	skyPsoDesc.pRootSignature = mRootSignature.Get();
	skyPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["skyVS"]->GetBufferPointer()),
		mShaders["skyVS"]->GetBufferSize()
	};
	skyPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["skyPS"]->GetBufferPointer()),
		mShaders["skyPS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skyPsoDesc, IID_PPV_ARGS(&mPSOs["sky"])));



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
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(1000.0f, 1000.0f, 200, 200);
	GeometryGenerator::MeshData hpBar = geoGen.CreateGrid(20.0f, 20.0f, 20, 20);
	GeometryGenerator::MeshData sphere = geoGen.CreateGeosphere(0.5f, 3);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);

	//
	// We are concatenating all the geometry into one big vertex/index buffer.  So
	// define the regions in the buffer each submesh covers.
	//

	// Cache the vertex offsets to each object in the concatenated vertex buffer.
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size();
	UINT hpBarVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
	UINT sphereVertexOffset = hpBarVertexOffset + (UINT)hpBar.Vertices.size();
	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();

	// Cache the starting index for each object in the concatenated index buffer.
	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size();
	UINT hpBarIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
	UINT sphereIndexOffset = hpBarIndexOffset + (UINT)hpBar.Indices32.size();
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

	SubmeshGeometry hpBarSubmesh;
	hpBarSubmesh.IndexCount = (UINT)hpBar.Indices32.size();
	hpBarSubmesh.StartIndexLocation = hpBarIndexOffset;
	hpBarSubmesh.BaseVertexLocation = hpBarVertexOffset;

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
		hpBar.Vertices.size() +
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

	for (size_t i = 0; i < hpBar.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = hpBar.Vertices[i].Position;
		vertices[k].Normal = hpBar.Vertices[i].Normal;
		vertices[k].TexC = hpBar.Vertices[i].TexC;
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

	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
	indices.insert(indices.end(), std::begin(hpBar.GetIndices16()), std::end(hpBar.GetIndices16()));
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
	indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

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
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["hpBar"] = hpBarSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;

	mGeometries[geo->Name] = std::move(geo);


	// UI
	std::vector<UIVertex> outUIVertices;
	std::vector<uint32_t> outUIIndices;

	GeometryGenerator::CreateGrid(
		outUIVertices, outUIIndices, 
		10.0f, 10.0f, 10, 10);
	mPlayer.mUI.BuildGeometry(
		md3dDevice.Get(),
		mCommandList.Get(),
		outUIVertices,
		outUIIndices,
		"SkillUI"
	);
}

void PortfolioGameApp::BuildArcheGeometry(
	const std::vector<std::vector<Vertex>>& outVertices,
	const std::vector<std::vector<std::uint32_t>>& outIndices,
	const std::vector<std::string>& geoName)
{
	UINT vertexOffset = 0;
	UINT indexOffset = 0;
	std::vector<SubmeshGeometry> submesh(geoName.size());

	// Submesh
	for (int i = 0; i < geoName.size(); ++i)
	{
		BoundingBox box;
		BoundingBox::CreateFromPoints(
			box,
			outVertices[i].size(),
			&outVertices[i][0].Pos,
			sizeof(Vertex));
		submesh[i].Bounds = box;
		submesh[i].IndexCount = (UINT)outIndices[i].size();
		submesh[i].StartIndexLocation = indexOffset;
		submesh[i].BaseVertexLocation = vertexOffset;

		vertexOffset += outVertices[i].size();
		indexOffset += outIndices[i].size();
	}

	// vertex
	std::vector<Vertex> vertices(vertexOffset);
	UINT k = 0;
	for (int i = 0; i < geoName.size(); ++i)
	{
		for (int j = 0; j < outVertices[i].size(); ++j, ++k)
		{
			vertices[k].Pos = outVertices[i][j].Pos;
			vertices[k].Normal = outVertices[i][j].Normal;
			vertices[k].TexC = outVertices[i][j].TexC;
		}
	}

	// index
	std::vector<std::uint32_t> indices;
	for (int i = 0; i < geoName.size(); ++i)
	{
		indices.insert(indices.end(), outIndices[i].begin(), outIndices[i].end());
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "Architecture";

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

	for (int i = 0; i < geoName.size(); ++i)
	{
		geo->DrawArgs[geoName[i]] = submesh[i];
	}

	mGeometries[geo->Name] = std::move(geo);
}

void PortfolioGameApp::BuildFbxGeometry()
{
	FbxLoader fbx;
	std::vector<SkinnedVertex> outSkinnedVertices;
	std::vector<std::uint32_t> outIndices;
	std::vector<Material> outMaterial;
	SkinnedData outSkinnedInfo;

	// Player
	std::string FileName = "../Resource/FBX/Character/";
	fbx.LoadFBX(outSkinnedVertices, outIndices, outSkinnedInfo, "Idle", outMaterial, FileName);

	fbx.LoadFBX(outSkinnedInfo, "playerWalking", FileName);
	fbx.LoadFBX(outSkinnedInfo, "run", FileName);
	fbx.LoadFBX(outSkinnedInfo, "Kick", FileName);
	fbx.LoadFBX(outSkinnedInfo, "FlyingKick", FileName);
	fbx.LoadFBX(outSkinnedInfo, "Hook", FileName);
	fbx.LoadFBX(outSkinnedInfo, "HitReaction", FileName);
	fbx.LoadFBX(outSkinnedInfo, "Death", FileName);
	fbx.LoadFBX(outSkinnedInfo, "WalkingBackward", FileName);

	mPlayer.BuildGeometry(md3dDevice.Get(), mCommandList.Get(), outSkinnedVertices, outIndices, outSkinnedInfo, "playerGeo");

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
			TextureName = "playerTex";
			TextureName.push_back(i +48);
			std::wstring TextureFileName;
			TextureFileName.assign(outMaterial[i].Name.begin(), outMaterial[i].Name.end());

			mTextures.SetTexture(
				TextureName,
				TextureFileName);
		}

		// Load Material
		std::string MaterialName = "playerMat";
		MaterialName.push_back(i + 48);

		auto playerTexIndex = mTextures.GetTextureIndex(TextureName);
		mMaterials.SetMaterial(
			MaterialName,
			MatIndex++,
			mTextures.GetTextureIndex(TextureName),
			outMaterial[i].DiffuseAlbedo,
			outMaterial[i].FresnelR0,
			outMaterial[i].Roughness);
	}
	mTextures.End();

	outSkinnedVertices.clear();
	outIndices.clear();
	outMaterial.clear();
	outSkinnedInfo.clear();
	fbx.clear();

	// Monster FBX
	FileName = "../Resource/FBX/Monster/";
	fbx.LoadFBX(outSkinnedVertices, outIndices, outSkinnedInfo, "Idle", outMaterial, FileName);

	fbx.LoadFBX(outSkinnedInfo, "Walking", FileName);
	fbx.LoadFBX(outSkinnedInfo, "MAttack1", FileName);
	fbx.LoadFBX(outSkinnedInfo, "MAttack2", FileName);
	fbx.LoadFBX(outSkinnedInfo, "HitReaction", FileName);
	fbx.LoadFBX(outSkinnedInfo, "Death", FileName);

	mMonster.BuildGeometry(md3dDevice.Get(), mCommandList.Get(), outSkinnedVertices, outIndices, outSkinnedInfo, "MonsterGeo");

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
			TextureName = "monsterTex";
			TextureName.push_back(i + 48);
			std::wstring TextureFileName;
			TextureFileName.assign(outMaterial[i].Name.begin(), outMaterial[i].Name.end());

			mTextures.SetTexture(
				TextureName,
				TextureFileName);

			// Load Material
			std::string MaterialName = "monsterMat";
			MaterialName.push_back(i + 48);

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

	outSkinnedVertices.clear();
	outIndices.clear();
	outMaterial.clear();
	outSkinnedInfo.clear();
	fbx.clear();

	// Object FBX
	std::vector<std::vector<Vertex>> archVertex;
	std::vector<std::vector<uint32_t>> archIndex;
	std::vector<std::string> archName;
	std::vector<Vertex> outVertices;

	FileName = "../Resource/FBX/Architecture/houseA/house";
	LoadFBXArchitecture(fbx, outVertices, outIndices, outMaterial, FileName, archName.size());
	archVertex.push_back(outVertices);
	archIndex.push_back(outIndices);
	archName.push_back("house");

	/*outVertices.clear();
	outIndices.clear();
	outMaterial.clear();
	fbx.clear();

	FileName = "../Resource/FBX/Architecture/houseB/WoodCabinDif";
	LoadFBXArchitecture(fbx, outVertices, outIndices, outMaterial, FileName, archName.size());
	archVertex.push_back(outVertices);
	archIndex.push_back(outIndices);
	archName.push_back("WoodCabinDif");*/

	BuildArcheGeometry(archVertex, archIndex, archName);
	
	outSkinnedVertices.clear();
	outIndices.clear();
	outMaterial.clear();
	outSkinnedInfo.clear();
}

void PortfolioGameApp::LoadFBXArchitecture(
	FbxLoader &fbx,
	std::vector<Vertex> &outVertices, 
	std::vector<unsigned int> &outIndices, 
	std::vector<Material> &outMaterial,
	std::string &FileName, const int& archIndex)
{
	outVertices.clear();
	outIndices.clear();
	outMaterial.clear();
	
	fbx.LoadFBX(outVertices, outIndices, outMaterial, FileName);
	
	// Begin
	mTextures.Begin(md3dDevice.Get(), mCommandList.Get(), mCbvHeap.Get());
	// Load Texture and Material
	int MatIndex = mMaterials.GetSize();
	for (int i = 0; i < outMaterial.size(); ++i)
	{
		// Load Texture 
		if (!outMaterial[i].Name.empty())
		{
			std::string TextureName;
			TextureName = "archeTex";
			TextureName.push_back(archIndex + 48);
			std::wstring TextureFileName;
			TextureFileName.assign(outMaterial[i].Name.begin(), outMaterial[i].Name.end());

			mTextures.SetTexture(
				TextureName,
				TextureFileName);

			// Load Material
			std::string MaterialName = "archeMat";
			MaterialName.push_back(archIndex + 48);

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
		"tundraTex",
		L"../Resource/Textures/tundra.jpg");

	mTextures.SetTexture(
		"iceTex",
		L"../Resource/Textures/ice.dds");

	mTextures.SetTexture(
		"redTex",
		L"../Resource/Textures/red.jpg");

	mTextures.SetTexture(
		"iconKickTex",
		L"../Resource/Icon/iconKick.png");

	mTextures.SetTexture(
		"iconPunchTex",
		L"../Resource/Icon/iconPunch.png");

	// Cube Map,
	mTextures.SetTexture(
		"skyCubeMap",
		L"../Resource/Textures/snowcube1024.dds");

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
		"tundra0",
		MatIndex++,
		mTextures.GetTextureIndex("tundraTex"),
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
		"red",
		MatIndex++,
		mTextures.GetTextureIndex("redTex"),
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

	mMaterials.SetMaterial(
		"iconKick",
		MatIndex++,
		mTextures.GetTextureIndex("iconKickTex"),
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		XMFLOAT3(0.05f, 0.02f, 0.02f),
		0.0f);

	mMaterials.SetMaterial(
		"iconPunch",
		MatIndex++,
		mTextures.GetTextureIndex("iconPunchTex"),
		XMFLOAT4(1.0f, 1.0f, 1.0f, 0.5f),
		XMFLOAT3(0.001f, 0.001f, 0.001f),
		0.0f);

	mMaterials.SetMaterial(
		"sky",
		MatIndex++,
		mTextureOffset,
		XMFLOAT4(0.0f, 0.0f, 0.0f, 0.5f),
		XMFLOAT3(0.001f, 0.001f, 0.001f),
		0.0f);
	
}

void PortfolioGameApp::BuildRenderItems()
{
	UINT objCBIndex = 0;

	auto gridRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&gridRitem->World, XMMatrixTranslation(0.0f, -0.1f, 0.0f));
	XMStoreFloat4x4(&gridRitem->TexTransform, XMMatrixScaling(20.0f, 20.0f, 1.0f));
	gridRitem->ObjCBIndex = objCBIndex++;
	gridRitem->Mat = mMaterials.Get("tundra0");
	gridRitem->Geo = mGeometries["shapeGeo"].get();
	gridRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
	mRitems[(int)RenderLayer::Opaque].push_back(gridRitem.get());
	mAllRitems.push_back(std::move(gridRitem));

	auto skyRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&skyRitem->World, XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
	skyRitem->TexTransform = MathHelper::Identity4x4();
	skyRitem->ObjCBIndex = objCBIndex++;
	skyRitem->Mat = mMaterials.Get("sky");
	skyRitem->Geo = mGeometries["shapeGeo"].get();
	skyRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skyRitem->IndexCount = skyRitem->Geo->DrawArgs["sphere"].IndexCount;
	skyRitem->StartIndexLocation = skyRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
	skyRitem->BaseVertexLocation = skyRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
	mRitems[(int)RenderLayer::Sky].push_back(skyRitem.get());
	mAllRitems.push_back(std::move(skyRitem));
	
	// Architecture
	auto houseRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&houseRitem->World, XMMatrixScaling(0.5f, 0.5f, 0.5f) * XMMatrixRotationRollPitchYaw(-XM_PIDIV2, -XM_PIDIV2, 0.0f) * XMMatrixTranslation(-200.0f, 0.0f, -200.0f));
	houseRitem->TexTransform = MathHelper::Identity4x4();
	houseRitem->ObjCBIndex = objCBIndex++;
	houseRitem->Mat = mMaterials.Get("archeMat0");
	houseRitem->Geo = mGeometries["Architecture"].get();
	houseRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	houseRitem->IndexCount = houseRitem->Geo->DrawArgs["house"].IndexCount;
	houseRitem->StartIndexLocation = houseRitem->Geo->DrawArgs["house"].StartIndexLocation;
	houseRitem->BaseVertexLocation = houseRitem->Geo->DrawArgs["house"].BaseVertexLocation;
	houseRitem->Geo->DrawArgs["house"].Bounds.Transform(houseRitem->Bounds, XMLoadFloat4x4(&houseRitem->World));
	mRitems[(int)RenderLayer::Architecture].push_back(houseRitem.get());
	mAllRitems.push_back(std::move(houseRitem));

	auto house1Ritem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&house1Ritem->World, XMMatrixScaling(0.5f, 0.5f, 0.5f) * XMMatrixRotationRollPitchYaw(-XM_PIDIV2, -XM_PIDIV2, 0.0f) * XMMatrixTranslation(-200.0f, 0.0f, -150.0f));
	house1Ritem->TexTransform = MathHelper::Identity4x4();
	house1Ritem->ObjCBIndex = objCBIndex++;
	house1Ritem->Mat = mMaterials.Get("archeMat0");
	house1Ritem->Geo = mGeometries["Architecture"].get();
	house1Ritem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	house1Ritem->IndexCount = house1Ritem->Geo->DrawArgs["house"].IndexCount;
	house1Ritem->StartIndexLocation = house1Ritem->Geo->DrawArgs["house"].StartIndexLocation;
	house1Ritem->BaseVertexLocation = house1Ritem->Geo->DrawArgs["house"].BaseVertexLocation;
	house1Ritem->Geo->DrawArgs["house"].Bounds.Transform(house1Ritem->Bounds, XMLoadFloat4x4(&house1Ritem->World));
	mRitems[(int)RenderLayer::Architecture].push_back(house1Ritem.get());
	mAllRitems.push_back(std::move(house1Ritem));

	auto house2Ritem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&house2Ritem->World, XMMatrixScaling(0.5f, 0.5f, 0.5f) * XMMatrixRotationRollPitchYaw(-XM_PIDIV2, -XM_PIDIV2, 0.0f) * XMMatrixTranslation(-200.0f, 0.0f, -100.0f));
	house2Ritem->TexTransform = MathHelper::Identity4x4();
	house2Ritem->ObjCBIndex = objCBIndex++;
	house2Ritem->Mat = mMaterials.Get("archeMat0");
	house2Ritem->Geo = mGeometries["Architecture"].get();
	house2Ritem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	house2Ritem->IndexCount = house2Ritem->Geo->DrawArgs["house"].IndexCount;
	house2Ritem->StartIndexLocation = house2Ritem->Geo->DrawArgs["house"].StartIndexLocation;
	house2Ritem->BaseVertexLocation = house2Ritem->Geo->DrawArgs["house"].BaseVertexLocation;
	house2Ritem->Geo->DrawArgs["house"].Bounds.Transform(house2Ritem->Bounds, XMLoadFloat4x4(&house2Ritem->World));
	mRitems[(int)RenderLayer::Architecture].push_back(house2Ritem.get());
	mAllRitems.push_back(std::move(house2Ritem));

	
	/*auto roadRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&roadRitem->World, XMMatrixScaling(1.0f, 1.0f, 30.0f) * XMMatrixTranslation(-150.0f, 0.0f, -100.0f));
	XMStoreFloat4x4(&roadRitem->TexTransform, XMMatrixScaling(10.0f, 100.0f, 1.0f));
	roadRitem->ObjCBIndex = objCBIndex++;
	roadRitem->Mat = mMaterials.Get("bricks0");
	roadRitem->Geo = mGeometries["shapeGeo"].get();
	roadRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	roadRitem->IndexCount = roadRitem->Geo->DrawArgs["hpBar"].IndexCount;
	roadRitem->StartIndexLocation = roadRitem->Geo->DrawArgs["hpBar"].StartIndexLocation;
	roadRitem->BaseVertexLocation = roadRitem->Geo->DrawArgs["hpBar"].BaseVertexLocation;
	mRitems[(int)RenderLayer::Opaque].push_back(roadRitem.get());
	mAllRitems.push_back(std::move(roadRitem));*/

	// Player
	mPlayer.BuildRenderItem(mMaterials, "playerMat0");
	mPlayer.mUI.BuildRenderItem(mGeometries, mMaterials);

	// Monster
	mMonster.BuildRenderItem(mMaterials, "monsterMat0");
	mMonster.mMonsterUI.BuildRenderItem(mGeometries, mMaterials, mMonster.GetNumberOfMonster());
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

		const int texOffset = 1;
		CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		tex.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);

		cmdList->SetGraphicsRootDescriptorTable(0, tex);

		if (ri->ObjCBIndex >= 0)
		{
		UINT cbvIndex = mObjCbvOffset + mCurrFrameResourceIndex * (UINT)mAllRitems.size() + ri->ObjCBIndex;
		auto cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		cbvHandle.Offset(cbvIndex, mCbvSrvDescriptorSize);
			cmdList->SetGraphicsRootDescriptorTable(texOffset, cbvHandle);
		}

		UINT matCbvIndex = mMatCbvOffset + mCurrFrameResourceIndex * mMaterials.GetSize() + ri->Mat->MatCBIndex;
		auto matCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		matCbvHandle.Offset(mMatCbvOffset + mCurrFrameResourceIndex * mMaterials.GetSize() + ri->Mat->MatCBIndex, mCbvSrvDescriptorSize);

		UINT skinnedIndex = mChaCbvOffset + mCurrFrameResourceIndex * mPlayer.GetAllRitemsSize() + ri->PlayerCBIndex;
		auto skinCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		skinCbvHandle.Offset(skinnedIndex, mCbvSrvDescriptorSize);

		UINT monsterIndex = mMonsterCbvOffset + mCurrFrameResourceIndex * mMonster.GetAllRitemsSize() + ri->MonsterCBIndex;
		auto monsterCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		monsterCbvHandle.Offset(monsterIndex, mCbvSrvDescriptorSize);

		UINT uiIndex = mUICbvOffset + mCurrFrameResourceIndex * mPlayer.mUI.GetSize() + ri->ObjCBIndex;
		auto uiCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		uiCbvHandle.Offset(uiIndex, mCbvSrvDescriptorSize);

		UINT monsterUIIndex = mMonsterUICbvOffset + mCurrFrameResourceIndex * mMonster.mMonsterUI.GetSize() + ri->ObjCBIndex;
		auto monsterUICbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		monsterUICbvHandle.Offset(monsterUIIndex, mCbvSrvDescriptorSize);

		
		cmdList->SetGraphicsRootDescriptorTable(texOffset + 1, matCbvHandle);
		cmdList->SetGraphicsRootDescriptorTable(texOffset + 5, uiCbvHandle);
		cmdList->SetGraphicsRootDescriptorTable(texOffset + 6, monsterUICbvHandle);

		if (ri->PlayerCBIndex >= 0)
		{
			cmdList->SetGraphicsRootDescriptorTable(texOffset + 3, skinCbvHandle);
		}
		else if (ri->MonsterCBIndex >= 0)
		{
			cmdList->SetGraphicsRootDescriptorTable(texOffset + 4, monsterCbvHandle);
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

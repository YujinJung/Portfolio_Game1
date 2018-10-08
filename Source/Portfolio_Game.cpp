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
#include "FBXGenerator.h"

#include "Player.h"
#include "Monster.h"
#include "Textures.h"
#include "Materials.h"
#include "TextureLoader.h"
#include "Utility.h"

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
	for (int i = 0; i < (int)eUIList::Count; ++i)
	{
		HitTime[i] = 0.0f;
		DelayTime[i] = 0.0f;
	}
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
	BuildShapeGeometry();
	BuildMaterials();
	BuildFbxGeometry();
	BuildRootSignature();
	BuildShadersAndInputLayout();
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
	DrawRenderItems(mCommandList.Get(), mRitems[(int)RenderLayer::Wall]);
	DrawRenderItems(mCommandList.Get(), mRitems[(int)RenderLayer::Architecture]);
	mCommandList->SetPipelineState(mPSOs["sky"].Get());
	DrawRenderItems(mCommandList.Get(), mRitems[(int)RenderLayer::Sky]);

	//// Sky
	//mCommandList->SetPipelineState(mPSOs["sky"].Get());
	//DrawRenderItems(mCommandList.Get(), mRitems[(int)RenderLayer::Sky]);

	// UI
	mCommandList->SetPipelineState(mPSOs["UI"].Get());
	DrawRenderItems(mCommandList.Get(), mPlayer.mUI.GetRenderItem(eUIList::Rect));
	DrawRenderItems(mCommandList.Get(), mPlayer.mUI.GetRenderItem(eUIList::I_Punch));
	DrawRenderItems(mCommandList.Get(), mPlayer.mUI.GetRenderItem(eUIList::I_Kick));
	DrawRenderItems(mCommandList.Get(), mPlayer.mUI.GetRenderItem(eUIList::I_Kick2));
	mCommandList->SetPipelineState(mPSOs["MonsterUI"].Get());
	DrawRenderItems(mCommandList.Get(), mMonster->mMonsterUI.GetRenderItem(eUIList::Rect));

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
	DrawRenderItems(mCommandList.Get(), mMonster->GetRenderItem(RenderLayer::Monster));

	// Shadow
	mCommandList->OMSetStencilRef(0);
	mCommandList->SetPipelineState(mPSOs["Player_shadow"].Get());
	DrawRenderItems(mCommandList.Get(), mPlayer.GetRenderItem(RenderLayer::Shadow));

	mCommandList->SetPipelineState(mPSOs["Monster_shadow"].Get());
	DrawRenderItems(mCommandList.Get(), mMonster->GetRenderItem(RenderLayer::Shadow));

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
			mPlayer.UpdatePlayerPosition(ePlayerMoveList::AddYaw, dx);
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

	// Architecture
	// Collision Chk
	XMVECTOR playerLook = mPlayer.GetCharacterInfo().mMovement.GetPlayerLook();
	XMVECTOR playerPos = mPlayer.GetCharacterInfo().mMovement.GetPlayerPosition();
	auto playerBoundForward = mPlayer.GetCharacterInfo().mBoundingBox;
	auto playerBoundBackward = playerBoundForward;

	playerBoundForward.Center.x += 3.0f * playerLook.m128_f32[0];
	playerBoundForward.Center.y += 3.0f * playerLook.m128_f32[1];
	playerBoundForward.Center.z += 3.0f * playerLook.m128_f32[2];

	playerBoundBackward.Center.x -= 3.0f * playerLook.m128_f32[0];
	playerBoundBackward.Center.y -= 3.0f * playerLook.m128_f32[1];
	playerBoundBackward.Center.z -= 3.0f * playerLook.m128_f32[2];

	checkCollision(
		mRitems[(int)RenderLayer::Architecture],
		playerBoundForward,
		playerBoundBackward,
		isForward,
		isBackward,
		dt);
	checkCollision(
		mRitems[(int)RenderLayer::Wall],
		playerBoundForward,
		playerBoundBackward,
		isForward,
		isBackward,
		dt);

	// Input
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
			if (GetAsyncKeyState(VK_LSHIFT))
			{
				mPlayer.SetClipName("run");

				if (isForward)
					mPlayer.UpdatePlayerPosition(ePlayerMoveList::Walk, 18.0f * dt);

				isForward = true;
			}
			else
			{
				mPlayer.SetClipName("playerWalking");

				if (isForward)
					mPlayer.UpdatePlayerPosition(ePlayerMoveList::Walk, 7.0f * dt);

				isForward = true;
			}

			if (mPlayer.GetCurrentClip() == eClipList::Walking)
			{
				if (mPlayer.isClipEnd())
					mPlayer.SetClipTime(0.0f);
			}
		}
		else
		{
			mPlayer.mCamera.Walk(20.0f * dt);
		}
	}
	else if (GetAsyncKeyState('S') & 0x8000) 
	{
		if (!mCameraDetach)
		{
			mPlayer.SetClipName("WalkingBackward");

			if(isBackward)
				mPlayer.UpdatePlayerPosition(ePlayerMoveList::Walk, -5.0f * dt);
			isBackward = true;

			if (mPlayer.GetCurrentClip() == eClipList::Walking)
			{
				if (mPlayer.isClipEnd())
					mPlayer.SetClipTime(0.0f);
			}
		}
		else
		{
			mPlayer.mCamera.Walk(-20.0f * dt);
		}
	}
	else if(GetAsyncKeyState('1') & 0x8000)
	{
		// Kick Delay, 5 seconds
		if (gt.TotalTime() - HitTime[(int)eUIList::I_Punch] > 3.0f)
		{
			mPlayer.SetClipTime(0.0f);
			mPlayer.Attack(mMonster, "Hook");
			HitTime[(int)eUIList::I_Punch] = gt.TotalTime();
		}
	}
	else if (GetAsyncKeyState('2') & 0x8000)
	{
		// Hook Delay, 3 seconds
		if (gt.TotalTime() - HitTime[(int)eUIList::I_Kick] > 5.0f)
		{
			mPlayer.SetClipTime(0.0f);
			mPlayer.Attack(mMonster, "Kick"); 
			HitTime[(int)eUIList::I_Kick] = gt.TotalTime();
		}
	}
	else if (GetAsyncKeyState('3') & 0x8000)
	{
		// Hook Delay, 3 seconds
		if (gt.TotalTime() - HitTime[(int)eUIList::I_Kick2] > 10.0f)
		{
			mPlayer.SetClipTime(0.0f);
			mPlayer.Attack(mMonster, "Kick2");
			HitTime[(int)eUIList::I_Kick2] = gt.TotalTime();
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
			mPlayer.UpdatePlayerPosition(ePlayerMoveList::AddYaw, -1.0f * dt);
		else
			mPlayer.mCamera.WalkSideway(-10.0f * dt);
	}
	else if (GetAsyncKeyState('D') & 0x8000)
	{
		if (!mCameraDetach)
			mPlayer.UpdatePlayerPosition(ePlayerMoveList::AddYaw, 1.0f * dt);
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

void PortfolioGameApp::checkCollision(
	const std::vector<RenderItem*>& rItems,
	const BoundingBox &playerBoundForward,
	const BoundingBox &playerBoundBackward,
	bool &isForward, 
	bool &isBackward,
	const float &dt)
{
	for (auto&e : rItems)
	{
		if (isForward && playerBoundForward.Contains(e->Bounds) == ContainmentType::INTERSECTS)
		{
			mPlayer.SetClipName("WalkingBackward");

			if (isBackward)
				mPlayer.UpdatePlayerPosition(ePlayerMoveList::Walk, -7.0f * dt);

			isForward = false;
		}
		if (isBackward && playerBoundBackward.Contains(e->Bounds) == ContainmentType::INTERSECTS)
		{
			mPlayer.SetClipName("playerWalking");

			if (isForward)
				mPlayer.UpdatePlayerPosition(ePlayerMoveList::Walk, 7.0f * dt);

			isBackward = false;
		}
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
	mMainPassCB.FogStart = 50.0f;

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
	
	// Spawn Monster
	int playerPosX = PlayerPos.m128_i32[0];
	int playerPosZ = PlayerPos.m128_i32[2];

	if (playerPosX < 0 && playerPosZ > 0)
	{
		if (mZoneIndex != 0)
		{
			mMonster = mMonstersByZone[0].get();
			mZoneIndex = 0;
		}
	}
	else if (playerPosX > 0 && playerPosZ > 0)
	{
		if (mZoneIndex != 1)
		{
			mMonster = mMonstersByZone[1].get();
			mZoneIndex = 1;
		}
	}
	else if (playerPosX > 0 && playerPosZ < 0)
	{
		if (mZoneIndex != 2)
		{
			mMonster = mMonstersByZone[2].get();
			mZoneIndex = 2;
		}
	}
	else if (playerPosX < 0 && playerPosZ < 0)
	{
		if (mZoneIndex != 2)
		{
			mMonster = mMonstersByZone[2].get();
			mZoneIndex = 2;
		}
	}

	
	// when all monsters die, the next room(third room) opens
	// and Block room4
	auto& e = mRitems[(int)RenderLayer::Wall].front();
	if (e->Mat->Name == "stone0" && mZoneIndex == 0 && mMonster->isAllDie())
	{
		XMMATRIX M = XMMatrixRotationY(XM_PIDIV2);
		mAllRitems[e->ObjCBIndex]->Mat = mMaterials.Get("ice0");
		e->Bounds.Transform(e->Bounds, M);
		XMStoreFloat4x4(&e->World, XMLoadFloat4x4(&e->World) * M);
		e->NumFramesDirty = gNumFrameResources;
	}
	else if (e->Mat->Name == "ice0" && mZoneIndex == 1 && mMonster->isAllDie())
	{
		XMMATRIX M = XMMatrixRotationY(XM_PIDIV2);
		mAllRitems[e->ObjCBIndex]->Mat = mMaterials.Get("Transparency");
		e->Bounds.Transform(e->Bounds, M);
		XMStoreFloat4x4(&e->World, XMLoadFloat4x4(&e->World) * M);
		e->NumFramesDirty = gNumFrameResources;
	}
	else if (e->Mat->Name == "Transparency" && mZoneIndex == 2 && mMonster->isAllDie())
	{
		XMMATRIX M = XMMatrixTranslation(0.0f, 0.0f, -500.0f);
		mAllRitems[e->ObjCBIndex]->Mat = mMaterials.Get("tundra0");
		e->Bounds.Transform(e->Bounds, M);
		XMStoreFloat4x4(&e->World, XMLoadFloat4x4(&e->World) * M);
		e->NumFramesDirty = gNumFrameResources;
		
		// TODO: set game end
	}
	

	if (mPlayer.GetHealth() <= 0 && !playerDeathCamFinished)
	{
		mCameraDetach = true;

		XMVECTOR CameraPos= mPlayer.mCamera.GetEyePosition();

		if (MathHelper::getDistance(PlayerPos, CameraPos) < 100.0f)
		{
			mPlayer.mCamera.Walk(-0.25f);
			mPlayer.mCamera.UpdateViewMatrix();
		}
		else
		{
			mPlayer.mUI.SetGameover();
			playerDeathCamFinished = true;
		}
	}

	static float lastTime = gt.TotalTime();
	if (gt.TotalTime() - lastTime > 0.04f)
	{
		mMonster->UpdateMonsterPosition(mPlayer, gt);
		lastTime = gt.TotalTime();
	}
	mMonster->UpdateCharacterCBs(mCurrFrameResource, mMainLight, gt);
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
	UINT monsterCount = mMonster->GetAllRitemsSize();
	UINT matCount = mMaterials.GetSize();
	UINT uiCount = mPlayer.mUI.GetSize();
	UINT monsterUICount = mMonster->GetUISize();
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

void PortfolioGameApp::BuildConstantBufferViews(int cbvOffset, UINT itemSize, UINT cbSize, eUploadBufferIndex e)
{
	UINT CBByteSize = d3dUtil::CalcConstantBufferByteSize(cbSize);

	for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
	{
		auto cbResource = FrameResource::GetResourceByIndex(mFrameResources[frameIndex].get(), e);

		for (UINT i = 0; i < itemSize; ++i)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = cbResource->GetGPUVirtualAddress();

			// Offset to the ith object constant buffer in the buffer.
			cbAddress += i * CBByteSize;

			// Offset to the object cbv in the descriptor heap.
			int heapIndex = cbvOffset + frameIndex * itemSize + i;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, mCbvSrvDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = CBByteSize;

			md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
		}
	}
}

void PortfolioGameApp::BuildConstantBufferViews()
{
	// Object 
	BuildConstantBufferViews(mObjCbvOffset, mAllRitems.size(), sizeof(ObjectConstants), eUploadBufferIndex::ObjectCB);

	// Material 
	BuildConstantBufferViews(mMatCbvOffset, mMaterials.GetSize(), sizeof(MaterialConstants), eUploadBufferIndex::MaterialCB);

	// Pass - 4
	// Last three descriptors are the pass CBVs for each frame resource.
	BuildConstantBufferViews(mPassCbvOffset, 1, sizeof(PassConstants), eUploadBufferIndex::PassCB);

	// Character
	BuildConstantBufferViews(mChaCbvOffset, mPlayer.GetAllRitemsSize(), sizeof(CharacterConstants), eUploadBufferIndex::PlayerCB);
	BuildConstantBufferViews(mMonsterCbvOffset, mMonster->GetAllRitemsSize(), sizeof(CharacterConstants), eUploadBufferIndex::MonsterCB);

	// UI
	BuildConstantBufferViews(mUICbvOffset, mPlayer.mUI.GetSize(), sizeof(UIConstants), eUploadBufferIndex::UICB);
	BuildConstantBufferViews(mMonsterUICbvOffset, mMonster->mMonsterUI.GetSize(), sizeof(UIConstants), eUploadBufferIndex::MonsterUICB);
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
			mMonster->GetAllRitemsSize(),
			mPlayer.mUI.GetSize(),
			mMonster->GetUISize()));
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

	mShaders["standardVS"] = d3dUtil::CompileShader(L"..\\Shaders\\Default.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["skinnedVS"] = d3dUtil::CompileShader(L"..\\Shaders\\Default.hlsl", skinnedDefines, "VS", "vs_5_1");
	mShaders["monsterVS"] = d3dUtil::CompileShader(L"..\\Shaders\\Monster.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["uiVS"] = d3dUtil::CompileShader(L"..\\Shaders\\UI.hlsl", playerUIDefines, "VS", "vs_5_1");
	mShaders["monsterUIVS"] = d3dUtil::CompileShader(L"..\\Shaders\\UI.hlsl", monsterUIDefines, "VS", "vs_5_1");
	mShaders["skyVS"] = d3dUtil::CompileShader(L"..\\Shaders\\Sky.hlsl", nullptr, "VS", "vs_5_1");

	mShaders["opaquePS"] = d3dUtil::CompileShader(L"..\\Shaders\\Default.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["skinnedPS"] = d3dUtil::CompileShader(L"..\\Shaders\\Default.hlsl", skinnedDefines, "PS", "ps_5_1");
	mShaders["monsterPS"] = d3dUtil::CompileShader(L"..\\Shaders\\Monster.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["uiPS"] = d3dUtil::CompileShader(L"..\\Shaders\\UI.hlsl", playerUIDefines, "PS", "ps_5_1");
	mShaders["skyPS"] = d3dUtil::CompileShader(L"..\\Shaders\\Sky.hlsl", nullptr, "PS", "ps_5_1");

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
	MonsterUIPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["uiPS"]->GetBufferPointer()),
		mShaders["uiPS"]->GetBufferSize()
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

void PortfolioGameApp::BuildFbxGeometry()
{
	FBXGenerator fbxGen;

	fbxGen.Begin(md3dDevice.Get(), mCommandList.Get(), mCbvHeap.Get());

	fbxGen.LoadFBXPlayer(mPlayer, mTextures, mMaterials);
	fbxGen.LoadFBXMonster(mMonster, mMonstersByZone, mTextures, mMaterials);

	// Initialize Monster in 1 zone
	mMonster = mMonstersByZone[1].get();

	//LoadFBXArchitecture();
	fbxGen.LoadFBXArchitecture(mGeometries, mTextures, mMaterials);

	fbxGen.End();
}


void PortfolioGameApp::LoadTextures()
{
	mTextures.Begin(md3dDevice.Get(), mCommandList.Get(), mCbvHeap.Get());

	std::vector<std::string> texNames;
	std::vector<std::wstring> texPaths;

	texNames.push_back("bricksTex");
	texPaths.push_back(L"../Resource/Textures/bricks.dds");

	texNames.push_back("bricks3Tex");
	texPaths.push_back(L"../Resource/Textures/bricks3.dds");
	
	texNames.push_back("stoneTex");
	texPaths.push_back(L"../Resource/Textures/stone.dds");

	texNames.push_back("tundraTex");
	texPaths.push_back(L"../Resource/Textures/tundra.jpg");

	texNames.push_back("iceTex");
	texPaths.push_back(L"../Resource/Textures/ice.dds");

	texNames.push_back("redTex");
	texPaths.push_back(L"../Resource/Textures/red.png");

	texNames.push_back("iconPunchTex");
	texPaths.push_back(L"../Resource/UI/iconPunch.png");

	texNames.push_back("iconKickTex");
	texPaths.push_back(L"../Resource/UI/iconKick.png");

	texNames.push_back("iconKick2Tex");
	texPaths.push_back(L"../Resource/UI/iconKick2.png");
	
	texNames.push_back("GameoverTex");
	texPaths.push_back(L"../Resource/UI/Gameover.png");

	texNames.push_back("NameMutantTex");
	texPaths.push_back(L"../Resource/UI/NameMutant.png");

	texNames.push_back("NameWarrokTex");
	texPaths.push_back(L"../Resource/UI/NameWarrok.png");

	texNames.push_back("NameMawTex");
	texPaths.push_back(L"../Resource/UI/NameMaw.png");

	// Cube Map,
	texNames.push_back("skyCubeMap");
	texPaths.push_back(L"../Resource/Textures/snowcube1024.dds");

	mTextures.SetTexture(texNames, texPaths);

	mTextures.End();
}

void PortfolioGameApp::BuildMaterials()
{
	int MatIndex = mMaterials.GetSize();

	std::vector<std::string> matName;
	std::vector<int> texIndices;
	std::vector<XMFLOAT4> diffuses;
	std::vector<XMFLOAT3> fresnels;
	std::vector<float> roughnesses;

	matName.push_back("bricks0");
	texIndices.push_back(mTextures.GetTextureIndex("bricksTex"));
	diffuses.push_back(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
	fresnels.push_back(XMFLOAT3(0.02f, 0.02f, 0.02f));
	roughnesses.push_back(0.1f);

	matName.push_back("bricks3");
	texIndices.push_back(mTextures.GetTextureIndex("bricks3Tex"));
	diffuses.push_back(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
	fresnels.push_back(XMFLOAT3(0.02f, 0.02f, 0.02f));
	roughnesses.push_back(0.1f);

	matName.push_back("stone0");
	texIndices.push_back(mTextures.GetTextureIndex("stoneTex"));
	diffuses.push_back(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
	fresnels.push_back(XMFLOAT3(0.05f, 0.05f, 0.05f));
	roughnesses.push_back(0.3f);

	matName.push_back("tundra0");
	texIndices.push_back(mTextures.GetTextureIndex("tundraTex"));
	diffuses.push_back(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
	fresnels.push_back(XMFLOAT3(0.05f, 0.02f, 0.02f));
	roughnesses.push_back(0.1f);

	matName.push_back("ice0");
	texIndices.push_back(mTextures.GetTextureIndex("iceTex"));
	diffuses.push_back(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
	fresnels.push_back(XMFLOAT3(0.05f, 0.02f, 0.02f));
	roughnesses.push_back(0.1f);

	matName.push_back("red");
	texIndices.push_back(mTextures.GetTextureIndex("redTex"));
	diffuses.push_back(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
	fresnels.push_back(XMFLOAT3(0.05f, 0.02f, 0.02f));
	roughnesses.push_back(0.1f);

	matName.push_back("Transparency");
	texIndices.push_back(mTextures.GetTextureIndex("redTex"));
	diffuses.push_back(XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f));
	fresnels.push_back(XMFLOAT3(0.05f, 0.02f, 0.02f));
	roughnesses.push_back(0.1f);

	matName.push_back("shadow0");
	texIndices.push_back(mTextures.GetTextureIndex("redTex"));
	diffuses.push_back(XMFLOAT4(0.0f, 0.0f, 0.0f, 0.5f));
	fresnels.push_back(XMFLOAT3(0.001f, 0.001f, 0.001f));
	roughnesses.push_back(0.0f);

	matName.push_back("iconPunch");
	texIndices.push_back(mTextures.GetTextureIndex("iconPunchTex"));
	diffuses.push_back(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
	fresnels.push_back(XMFLOAT3(0.001f, 0.001f, 0.001f));
	roughnesses.push_back(0.0f);

	matName.push_back("iconKick");
	texIndices.push_back(mTextures.GetTextureIndex("iconKickTex"));
	diffuses.push_back(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
	fresnels.push_back(XMFLOAT3(0.001f, 0.001f, 0.001f));
	roughnesses.push_back(0.0f);

	matName.push_back("iconKick2");
	texIndices.push_back(mTextures.GetTextureIndex("iconKick2Tex"));
	diffuses.push_back(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
	fresnels.push_back(XMFLOAT3(0.001f, 0.001f, 0.001f));
	roughnesses.push_back(0.0f);

	matName.push_back("Gameover");
	texIndices.push_back(mTextures.GetTextureIndex("GameoverTex"));
	diffuses.push_back(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
	fresnels.push_back(XMFLOAT3(0.05f, 0.02f, 0.02f));
	roughnesses.push_back(0.0f);

	matName.push_back("NameMutant");
	texIndices.push_back(mTextures.GetTextureIndex("NameMutantTex"));
	diffuses.push_back(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
	fresnels.push_back(XMFLOAT3(0.05f, 0.02f, 0.02f));
	roughnesses.push_back(0.0f);

	matName.push_back("NameWarrok");
	texIndices.push_back(mTextures.GetTextureIndex("NameWarrokTex"));
	diffuses.push_back(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
	fresnels.push_back(XMFLOAT3(0.05f, 0.02f, 0.02f));
	roughnesses.push_back(0.0f);

	matName.push_back("NameMaw");
	texIndices.push_back(mTextures.GetTextureIndex("NameMawTex"));
	diffuses.push_back(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
	fresnels.push_back(XMFLOAT3(0.05f, 0.02f, 0.02f));
	roughnesses.push_back(0.0f);

	matName.push_back("sky");
	texIndices.push_back(mTextureOffset);
	diffuses.push_back(XMFLOAT4(0.0f, 0.0f, 0.0f, 0.5f));
	fresnels.push_back(XMFLOAT3(0.001f, 0.001f, 0.001f));
	roughnesses.push_back(0.0f);

	mMaterials.SetMaterial(
		matName, texIndices, diffuses,
		fresnels, roughnesses, MatIndex);
	
}

void PortfolioGameApp::BuildRenderItems()
{
	UINT objCBIndex = -1;

	BuildLandscapeRitems(objCBIndex);

	auto skyRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&skyRitem->World, XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
	skyRitem->TexTransform = MathHelper::Identity4x4();
	skyRitem->ObjCBIndex = ++objCBIndex;
	skyRitem->Mat = mMaterials.Get("sky");
	skyRitem->Geo = mGeometries["shapeGeo"].get();
	skyRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skyRitem->IndexCount = skyRitem->Geo->DrawArgs["sphere"].IndexCount;
	skyRitem->StartIndexLocation = skyRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
	skyRitem->BaseVertexLocation = skyRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
	mRitems[(int)RenderLayer::Sky].push_back(skyRitem.get());
	mAllRitems.push_back(std::move(skyRitem));
	
	// Architecture
	
	// Player
	mPlayer.BuildRenderItem(mMaterials, "playerMat0");
	mPlayer.mUI.BuildRenderItem(mGeometries, mMaterials);

	// Monster
	std::string monsterName;
	for (int i = 0; i < mMonstersByZone.size(); ++i)
	{
		if (i == 0)
			monsterName = "NameMutant";
		else if (i == 1)
			monsterName = "NameWarrok";
		else if (i == 2)
			monsterName = "NameMaw";

		mMonstersByZone[i]->BuildRenderItem(mMaterials, "monsterMat" + i);
		mMonstersByZone[i]->mMonsterUI.BuildRenderItem(mGeometries, mMaterials, monsterName, mMonstersByZone[i]->GetNumberOfMonster());
	}
}

void PortfolioGameApp::BuildLandscapeRitems(UINT& objCBIndex)
{
	// Ground
	float z = 500.0f;
	for (int i = 0; i < 4; ++i)
	{
		auto subRitem = std::make_unique<RenderItem>();
		XMStoreFloat4x4(&subRitem->TexTransform, XMMatrixScaling(30.0f, 30.0f, 1.0f));
		subRitem->ObjCBIndex = ++objCBIndex;
		subRitem->Geo = mGeometries["shapeGeo"].get();
		subRitem->Mat = mMaterials.Get("tundra0");
		subRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		subRitem->IndexCount = subRitem->Geo->DrawArgs["grid"].IndexCount;
		subRitem->StartIndexLocation = subRitem->Geo->DrawArgs["grid"].StartIndexLocation;
		subRitem->BaseVertexLocation = subRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
		subRitem->Geo->DrawArgs["grid"].Bounds.Transform(subRitem->Bounds, XMLoadFloat4x4(&subRitem->World));

		z *= -1.0f;
		if (i > 1)
		{
			XMStoreFloat4x4(&subRitem->World, XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixTranslation(-500.0f, -0.1f, z));

			if (i == 3)
				subRitem->Mat = mMaterials.Get("stone0");
		}
		else
		{
			XMStoreFloat4x4(&subRitem->World, XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixTranslation(500.0f, -0.1f, z));

			if(i == 0)
				subRitem->Mat = mMaterials.Get("ice0");
		}
		mRitems[(int)RenderLayer::Opaque].push_back(subRitem.get());
		mAllRitems.push_back(std::move(subRitem));
	}


	// Area
	XMMATRIX S = XMMatrixScaling(500.0f, 200.0f, 10.0f);
	float x = 400.0f;
	z = 300.0f;
	for (int i = 0; i < 2; ++i)
	{
		// | |
		x *= -1.0f;
		auto subRitem = std::make_unique<RenderItem>();
		XMStoreFloat4x4(&subRitem->World, S * XMMatrixRotationY(XM_PIDIV2) * XMMatrixTranslation(x, 0.0f, 0.0f));
		subRitem->TexTransform = MathHelper::Identity4x4();
		subRitem->ObjCBIndex = ++objCBIndex;
		subRitem->Mat = mMaterials.Get("Transparency");
		subRitem->Geo = mGeometries["shapeGeo"].get();
		subRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		subRitem->IndexCount = subRitem->Geo->DrawArgs["box"].IndexCount;
		subRitem->StartIndexLocation = subRitem->Geo->DrawArgs["box"].StartIndexLocation;
		subRitem->BaseVertexLocation = subRitem->Geo->DrawArgs["box"].BaseVertexLocation;
		subRitem->Geo->DrawArgs["box"].Bounds.Transform(subRitem->Bounds, XMLoadFloat4x4(&subRitem->World));
		mRitems[(int)RenderLayer::Architecture].push_back(subRitem.get());
		mAllRitems.push_back(std::move(subRitem));

		// --
		z *= -1.0f;
		auto subRitem2 = std::make_unique<RenderItem>();
		XMStoreFloat4x4(&subRitem2->World, S * XMMatrixTranslation(0.0f, 0.0f, z));
		subRitem2->TexTransform = MathHelper::Identity4x4();
		subRitem2->ObjCBIndex = ++objCBIndex;
		subRitem2->Mat = mMaterials.Get("Transparency");
		subRitem2->Geo = mGeometries["shapeGeo"].get();
		subRitem2->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		subRitem2->IndexCount = subRitem2->Geo->DrawArgs["box"].IndexCount;
		subRitem2->StartIndexLocation = subRitem2->Geo->DrawArgs["box"].StartIndexLocation;
		subRitem2->BaseVertexLocation = subRitem2->Geo->DrawArgs["box"].BaseVertexLocation;
		subRitem2->Geo->DrawArgs["box"].Bounds.Transform(subRitem2->Bounds, XMLoadFloat4x4(&subRitem2->World));
		mRitems[(int)RenderLayer::Architecture].push_back(subRitem2.get());
		mAllRitems.push_back(std::move(subRitem2));
	}
	// 1 | 4
	BuildSubRitems(
		"shapeGeo",
		"box",
		"ice0",
		RenderLayer::Architecture,
		++objCBIndex,
		XMMatrixScaling(250.0f, 200.0f, 10.0f) * XMMatrixRotationY(XM_PIDIV2) * XMMatrixTranslation(0.0f, 0.0f, -150.0f),
		XMMatrixScaling(5.0f, 4.0f, 1.0f));
	

	// First Room

	// House
	XMMATRIX worldSR = XMMatrixScaling(0.5f, 0.5f, 0.5f) * XMMatrixRotationRollPitchYaw(-XM_PIDIV2, -XM_PIDIV2, 0.0f);
	BuildSubRitems(
		"Architecture",
		"house",
		"archiMat0",
		RenderLayer::Architecture,
		++objCBIndex,
		worldSR * XMMatrixTranslation(-300.0f, 0.0f, -200.0f),
		XMMatrixIdentity());
	BuildSubRitems(
		"Architecture",
		"house",
		"archiMat0",
		RenderLayer::Architecture,
		++objCBIndex,
		worldSR * XMMatrixTranslation(-300.0f, 0.0f, -150.0f),
		XMMatrixIdentity());
	BuildSubRitems(
		"Architecture",
		"house",
		"archiMat0",
		RenderLayer::Architecture,
		++objCBIndex,
		worldSR * XMMatrixTranslation(-300.0f, 0.0f, -100.0f),
		XMMatrixIdentity());

	// Rocks
	BuildSubRitems(
		"Architecture",
		"RockCluster",
		"archiMat1",
		RenderLayer::Architecture,
		++objCBIndex,
		XMMatrixScaling(10.0f, 10.0f, 10.0f) * XMMatrixRotationRollPitchYaw(-XM_PIDIV2, 0.0f, 0.0f) * XMMatrixTranslation(-100.0f, 3.0f, -200.0f),
		XMMatrixIdentity());
	BuildSubRitems(
		"Architecture",
		"RockCluster",
		"archiMat1",
		RenderLayer::Architecture,
		++objCBIndex,
		XMMatrixScaling(20.0f, 20.0f, 20.0f) * XMMatrixRotationRollPitchYaw(-XM_PIDIV2, 0.0f, -XM_PI) * XMMatrixTranslation(-110.0f, 6.0f, -270.0f),
		XMMatrixIdentity());
	BuildSubRitems(
		"Architecture",
		"RockCluster",
		"archiMat1",
		RenderLayer::Architecture,
		++objCBIndex,
		XMMatrixScaling(20.0f, 20.0f, 20.0f) * XMMatrixRotationRollPitchYaw(-XM_PIDIV2, 0.0f, -XM_PIDIV4 * 3.0f) * XMMatrixTranslation(-50.0f, 6.0f, -270.0f),
		XMMatrixIdentity());
	BuildSubRitems(
		"Architecture",
		"RockCluster",
		"archiMat1",
		RenderLayer::Architecture,
		++objCBIndex,
		XMMatrixScaling(10.0f, 10.0f, 10.0f) * XMMatrixRotationRollPitchYaw(-XM_PIDIV2, 0.0f, XM_PI) * XMMatrixTranslation(-170.0f, 3.0f, -250.0f),
		XMMatrixIdentity());
	
	// Tree
	XMMATRIX treeWorldSR = XMMatrixScaling(10.0f, 10.0f, 10.0f) * XMMatrixRotationX(-XM_PIDIV2);
	float treeX = -170.0f;

	for (int i = 0; i < 10; ++i)
	{
		auto seed = std::chrono::system_clock::now().time_since_epoch().count();
		std::mt19937 engine{ (unsigned int)seed };
		std::uniform_int_distribution <> dis{ 15, 25 };
		int x{ dis(engine) };
		treeX -= static_cast<float>(x);

		XMMATRIX treeWorld = treeWorldSR * XMMatrixTranslation(treeX, 0.0f, -210.0f - static_cast<float>(x));
		BuildSubRitems(
			"Architecture",
			"Tree",
			"archiMat4",
			RenderLayer::Architecture,
			++objCBIndex,
			treeWorld,
			XMMatrixIdentity());
		BuildSubRitems(
			"Architecture",
			"Leaf",
			"archiMat5",
			RenderLayer::Architecture,
			++objCBIndex,
			treeWorld,
			XMMatrixIdentity());

		treeWorld = treeWorldSR * XMMatrixTranslation(treeX - static_cast<float>(x), 0.0f, -230.0f - static_cast<float>(x));
		BuildSubRitems(
			"Architecture",
			"Tree",
			"archiMat4",
			RenderLayer::Architecture,
			++objCBIndex,
			treeWorld,
			XMMatrixIdentity());
		BuildSubRitems(
			"Architecture",
			"Leaf",
			"archiMat5",
			RenderLayer::Architecture,
			++objCBIndex,
			treeWorld,
			XMMatrixIdentity());
	}


	// Second Room

	// Canyon
	BuildSubRitems(
		"Architecture",
		"Canyon0",
		"archiMat2",
		RenderLayer::Architecture,
		++objCBIndex,
		//XMMatrixScaling(0.2f, 0.2f, 0.2f) * XMMatrixRotationRollPitchYaw(-XM_PIDIV2, 0.0f, XM_PI) * XMMatrixTranslation(-100.0f, -1.0f, 160.0f),
		XMMatrixScaling(0.2f, 0.2f, 0.2f) * XMMatrixRotationRollPitchYaw(-XM_PIDIV2, 0.0f, 0.0f),
		XMMatrixIdentity());
	BuildSubRitems(
		"Architecture",
		"Canyon2",
		"archiMat2",
		RenderLayer::Architecture,
		++objCBIndex,
		XMMatrixScaling(0.2f, 0.2f, 0.2f) * XMMatrixRotationRollPitchYaw(XM_PIDIV2, 0.0f, XM_PI) * XMMatrixTranslation(170.0f, 72.0f, 340.0f),
		XMMatrixIdentity());
	BuildSubRitems(
		"Architecture",
		"Canyon1",
		"archiMat2",
		RenderLayer::Architecture,
		++objCBIndex,
		XMMatrixScaling(0.2f, 0.2f, 0.2f) * XMMatrixRotationRollPitchYaw(XM_PIDIV2, 0.0f, -XM_PIDIV2) * XMMatrixTranslation(-340.0f, 72.0f, 660.0f),
		XMMatrixIdentity());

	// The wall between the first room and the second room.
	BuildSubRitems(
		"shapeGeo",
		"box",
		"stone0",
		RenderLayer::Wall,
		++objCBIndex,
		XMMatrixScaling(250.0f, 200.0f, 10.0f) * XMMatrixRotationY(XM_PIDIV2) * XMMatrixTranslation(0.0f, 0.0f, 150.0f),
		XMMatrixScaling(5.0f, 4.0f, 1.0f));

	// Third Room

	// House
	BuildSubRitems(
		"Architecture",
		"house",
		"archiMat0",
		RenderLayer::Architecture,
		++objCBIndex,
		worldSR * XMMatrixRotationY(XM_PI) * XMMatrixTranslation(350.0f, 0.0f, 180.0f),
		XMMatrixIdentity());
	BuildSubRitems(
		"Architecture",
		"house",
		"archiMat0",
		RenderLayer::Architecture,
		++objCBIndex,
		worldSR * XMMatrixRotationY(XM_PI) * XMMatrixTranslation(350.0f, 0.0f, 130.0f),
		XMMatrixIdentity());

	// Rocks
	BuildSubRitems(
		"Architecture",
		"Rock0",
		"archiMat3",
		RenderLayer::Architecture,
		++objCBIndex,
		XMMatrixScaling(0.2f, 0.2f, 0.2f) * XMMatrixRotationRollPitchYaw(XM_PI, 0.0f, XM_PI) * XMMatrixTranslation(350.0f, -2.0f, 270.0f),
		XMMatrixIdentity());
	BuildSubRitems(
		"Architecture",
		"Rock0",
		"archiMat3",
		RenderLayer::Architecture,
		++objCBIndex,
		XMMatrixScaling(0.2f, 0.2f, 0.2f) * XMMatrixRotationRollPitchYaw(XM_PI, 0.0f, XM_PI) * XMMatrixTranslation(370.0f, -2.0f, 240.0f),
		XMMatrixIdentity());
	BuildSubRitems(
		"Architecture",
		"Rock0",
		"archiMat3",
		RenderLayer::Architecture,
		++objCBIndex,
		XMMatrixScaling(0.2f, 0.2f, 0.2f) * XMMatrixRotationRollPitchYaw(XM_PI, 0.0f, XM_PIDIV2) * XMMatrixTranslation(390.0f, 20.0f, 270.0f),
		XMMatrixIdentity());
	BuildSubRitems(
		"Architecture",
		"Rock0",
		"archiMat3",
		RenderLayer::Architecture,
		++objCBIndex,
		XMMatrixScaling(0.2f, 0.2f, 0.2f) * XMMatrixRotationRollPitchYaw(XM_PI, 0.0f, XM_PIDIV2) * XMMatrixTranslation(360.0f, 0.0f, 230.0f),
		XMMatrixIdentity());
	BuildSubRitems(
		"Architecture",
		"Rock0",
		"archiMat3",
		RenderLayer::Architecture,
		++objCBIndex,
		XMMatrixScaling(0.2f, 0.2f, 0.2f) * XMMatrixRotationRollPitchYaw(XM_PI, 0.0f, XM_PIDIV2) * XMMatrixTranslation(350.0f, 0.0f, 250.0f),
		XMMatrixIdentity());
	BuildSubRitems(
		"Architecture",
		"Rock0",
		"archiMat3",
		RenderLayer::Architecture,
		++objCBIndex,
		XMMatrixScaling(0.2f, 0.2f, 0.2f) * XMMatrixRotationRollPitchYaw(XM_PI, 0.0f, XM_PI) * XMMatrixTranslation(360.0f, 10.0f, 250.0f),
		XMMatrixIdentity());

	// Tree
	treeWorldSR = XMMatrixScaling(10.0f, 10.0f, 10.0f) * XMMatrixRotationX(-XM_PIDIV2);
	treeX = 90.0f;

	for (int i = 0; i < 20; ++i)
	{
		auto seed = std::chrono::system_clock::now().time_since_epoch().count();
		std::mt19937 engine{ (unsigned int)seed };
		std::uniform_int_distribution <> dis{ 15, 25 };
		int x{ dis(engine) };
		treeX += static_cast<float>(x);

		XMMATRIX treeWorld = treeWorldSR * XMMatrixTranslation(treeX, 0.0f, 300.0f - static_cast<float>(x));
		BuildSubRitems(
			"Architecture",
			"Tree",
			"archiMat4",
			RenderLayer::Architecture,
			++objCBIndex,
			treeWorld,
			XMMatrixIdentity());
		BuildSubRitems(
			"Architecture",
			"Leaf",
			"archiMat5",
			RenderLayer::Architecture,
			++objCBIndex,
			treeWorld,
			XMMatrixIdentity());

		treeWorld = treeWorldSR * XMMatrixTranslation(treeX - static_cast<float>(x), 0.0f, 320.0f - static_cast<float>(x));
		BuildSubRitems(
			"Architecture",
			"Tree",
			"archiMat4",
			RenderLayer::Architecture,
			++objCBIndex,
			treeWorld,
			XMMatrixIdentity());
		BuildSubRitems(
			"Architecture",
			"Leaf",
			"archiMat5",
			RenderLayer::Architecture,
			++objCBIndex,
			treeWorld,
			XMMatrixIdentity());
	}



	// Fourth Room

	BuildSubRitems(
		"Architecture",
		"Canyon0",
		"ice0",
		RenderLayer::Architecture,
		++objCBIndex,
		XMMatrixScaling(0.2f, 0.2f, 0.2f) * XMMatrixRotationRollPitchYaw(-XM_PIDIV2, 0.0f, 0.0f) * XMMatrixTranslation(100.0f, -1.0f, -100.0f),
		XMMatrixIdentity());
	BuildSubRitems(
		"Architecture",
		"Canyon0",
		"ice0",
		RenderLayer::Architecture,
		++objCBIndex,
		XMMatrixScaling(0.2f, 0.2f, 0.2f) * XMMatrixRotationRollPitchYaw(-XM_PIDIV2, 0.0f, 0.0f) * XMMatrixTranslation(400.0f, -1.0f, -100.0f),
		XMMatrixIdentity());
	BuildSubRitems(
		"Architecture",
		"Canyon0",
		"ice0",
		RenderLayer::Architecture,
		++objCBIndex,
		XMMatrixScaling(0.5f, 0.2f, 0.2f) * XMMatrixRotationRollPitchYaw(-XM_PIDIV2, 0.0f, 0.0f) * XMMatrixTranslation(400.0f, -1.0f, -500.0f),
		XMMatrixIdentity());
	BuildSubRitems(
		"shapeGeo",
		"box",
		"ice0",
		RenderLayer::Architecture,
		++objCBIndex,
		XMMatrixScaling(150.0f, 200.0f, 10.0f) * XMMatrixRotationY(XM_PIDIV2) * XMMatrixTranslation(400.0f, 0.0f, -220.0f),
		XMMatrixIdentity());
}
void PortfolioGameApp::BuildSubRitems(
	std::string geoName,
	std::string subRitemName,
	std::string matName, 
	RenderLayer subRtype,
	UINT &objCBIndex,
	FXMMATRIX& worldTransform,
	CXMMATRIX& texTransform	)
{
	auto subRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&subRitem->World, worldTransform);
	XMStoreFloat4x4(&subRitem->TexTransform, texTransform);
	subRitem->ObjCBIndex = objCBIndex;
	subRitem->Mat = mMaterials.Get(matName);
	subRitem->Geo = mGeometries[geoName].get();
	subRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	subRitem->IndexCount = subRitem->Geo->DrawArgs[subRitemName].IndexCount;
	subRitem->StartIndexLocation = subRitem->Geo->DrawArgs[subRitemName].StartIndexLocation;
	subRitem->BaseVertexLocation = subRitem->Geo->DrawArgs[subRitemName].BaseVertexLocation;
	subRitem->Geo->DrawArgs[subRitemName].Bounds.Transform(subRitem->Bounds, XMLoadFloat4x4(&subRitem->World));
	mRitems[(int)subRtype].push_back(subRitem.get());
	mAllRitems.push_back(std::move(subRitem));
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

		UINT monsterIndex = mMonsterCbvOffset + mCurrFrameResourceIndex * mMonster->GetAllRitemsSize() + ri->MonsterCBIndex;
		auto monsterCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		monsterCbvHandle.Offset(monsterIndex, mCbvSrvDescriptorSize);

		UINT uiIndex = mUICbvOffset + mCurrFrameResourceIndex * mPlayer.mUI.GetSize() + ri->ObjCBIndex;
		auto uiCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		uiCbvHandle.Offset(uiIndex, mCbvSrvDescriptorSize);

		UINT monsterUIIndex = mMonsterUICbvOffset + mCurrFrameResourceIndex * mMonster->mMonsterUI.GetSize() + ri->ObjCBIndex;
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

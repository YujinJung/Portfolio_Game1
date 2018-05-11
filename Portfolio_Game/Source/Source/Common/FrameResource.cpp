#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device * device, UINT passCount, UINT objectCount, UINT materialCount, UINT PlayerCount, UINT MonsterCount, UINT UICount, UINT MonsterUICount)
{
	ThrowIfFailed(device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));

	PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);
	MaterialCB = std::make_unique<UploadBuffer<MaterialConstants>>(device, materialCount, true);
	ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
	PlayerCB = std::make_unique<UploadBuffer<CharacterConstants>>(device, PlayerCount, true);
	MonsterCB = std::make_unique<UploadBuffer<CharacterConstants>>(device, MonsterCount, true);
	UICB = std::make_unique<UploadBuffer<UIConstants>>(device, UICount, true);
	MonsterUICB = std::make_unique<UploadBuffer<UIConstants>>(device, MonsterUICount, true);
}

FrameResource::~FrameResource()
{

}
#include "MonsterUI.h"
#include "Materials.h"

using namespace DirectX;

MonsterUI::MonsterUI()
	: UIIndex(0)
{
	mWorldTransform.Scale = { 10.0f, 10.0f, 10.0f };
	mWorldTransform.Position = { 0.0f, 0.0f, 0.0f };
	UIoffset = { 0.0f, 0.0f, 0.0f };
}


MonsterUI::~MonsterUI()
{
}

UINT MonsterUI::GetSize() const
{
	return (UINT)mAllRitems.size();
}

const std::vector<RenderItem*> MonsterUI::GetRenderItem(eUIList Type)const
{
	return mRitems[(int)Type];
}

void MonsterUI::BuildRenderItem(std::unordered_map<std::string, std::unique_ptr<MeshGeometry>>& mGeometries, Materials & mMaterials)
{
	// TODO : Setting the Name

	// place over the head
	auto temp = std::make_unique<RenderItem>();
	// atan(theta) : Theta is associated with PlayerCamera
	XMStoreFloat4x4(&temp->World, XMMatrixScaling(0.01f, 0.01f, 0.001f) * XMMatrixRotationX(atan(3.0f / 2.0f)) * XMMatrixRotationY(XM_PI) * XMMatrixTranslation(0.0f, 0.87f, 0.0f));
	temp->TexTransform = MathHelper::Identity4x4();
	temp->Mat = mMaterials.Get("ice0");
	temp->Geo = mGeometries["shapeGeo"].get();
	temp->ObjCBIndex = UIIndex++;
	temp->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	temp->StartIndexLocation = temp->Geo->DrawArgs["box"].StartIndexLocation;
	temp->BaseVertexLocation = temp->Geo->DrawArgs["box"].BaseVertexLocation;
	temp->IndexCount = temp->Geo->DrawArgs["box"].IndexCount;
	mRitems[(int)eUIList::Rect].push_back(temp.get());
	mAllRitems.push_back(std::move(temp));

	auto mana = std::make_unique<RenderItem>();
	// atan(theta) : Theta is associated with PlayerCamera
	XMStoreFloat4x4(&mana->World, XMMatrixScaling(10.01f, 10.01f, 110.001f) * XMMatrixRotationX(atan(3.0f / 2.0f)) * XMMatrixRotationY(XM_PI) * XMMatrixTranslation(0.0f, 0.89f, 0.0f));
	mana->TexTransform = MathHelper::Identity4x4();
	mana->Mat = mMaterials.Get("bricks0");
	mana->Geo = mGeometries["shapeGeo"].get();
	mana->ObjCBIndex = UIIndex++;
	mana->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	mana->StartIndexLocation = mana->Geo->DrawArgs["box"].StartIndexLocation;
	mana->BaseVertexLocation = mana->Geo->DrawArgs["box"].BaseVertexLocation;
	mana->IndexCount = mana->Geo->DrawArgs["box"].IndexCount;
	mRitems[(int)eUIList::Rect].push_back(mana.get());
	mAllRitems.push_back(std::move(mana));
}

void MonsterUI::UpdateUICBs(UploadBuffer<UIConstants>* currUICB, XMMATRIX playerWorld, bool mTransformDirty)
{
	for (auto& e : mAllRitems)
	{
		if (mTransformDirty) { e->NumFramesDirty = gNumFrameResources; }

		if (e->NumFramesDirty > 0)
		{
			XMMATRIX T = XMMatrixTranslation(mWorldTransform.Position.x + UIoffset.x, mWorldTransform.Position.y + UIoffset.y, mWorldTransform.Position.z + UIoffset.z);
			XMMATRIX world = XMLoadFloat4x4(&e->World) * playerWorld * T;
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);
			XMMATRIX S = XMMatrixScaling(mWorldTransform.Scale.x, mWorldTransform.Scale.y, mWorldTransform.Scale.z);

			UIConstants uiConstants;
			XMStoreFloat4x4(&uiConstants.World, XMMatrixTranspose(world) * S);
			XMStoreFloat4x4(&uiConstants.TexTransform, XMMatrixTranspose(texTransform));

			currUICB->CopyData(e->ObjCBIndex, uiConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}
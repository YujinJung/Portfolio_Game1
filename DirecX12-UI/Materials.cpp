#include "Materials.h"
#include "../Common/UploadBuffer.h"


Materials::Materials()
{
}


Materials::~Materials()
{
}

void Materials::SetMaterial(std::string Name, int MatIndex, int DiffuseSrvHeapIndex, DirectX::XMFLOAT4 DiffuseAlbedo, DirectX::XMFLOAT3 FresnelR0, float Roughness)
{
	auto temp = std::make_unique<Material>();
	temp->Name = Name;
	temp->MatCBIndex = MatIndex;
	temp->DiffuseSrvHeapIndex = DiffuseSrvHeapIndex;
	temp->DiffuseAlbedo = DiffuseAlbedo;
	temp->FresnelR0 = FresnelR0;
	temp->Roughness = Roughness;

	mMaterials[Name] = std::move(temp);
}

Material * Materials::Get(std::string Name)
{
	return mMaterials[Name].get();
}

UINT Materials::GetSize() const
{
	return (UINT)mMaterials.size();
}

void Materials::UpdateMaterialCB(UploadBuffer<MaterialConstants>* currMaterialCB)
{
	for (auto& e : mMaterials)
	{
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0)
		{
			DirectX::XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialCB->CopyData(mat->MatCBIndex, matConstants);

			mat->NumFramesDirty--;
		}
	}
}


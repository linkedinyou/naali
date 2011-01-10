#include "AssetRefListener.h"
#include "IAttribute.h"
#include "AssetReference.h"
#include "IComponent.h"
#include "Framework.h"
#include "AssetAPI.h"
#include "IAsset.h"
#include "IAssetTransfer.h"

AssetPtr AssetRefListener::Asset()
{
    return asset.lock();
}

bool AssetRefListener::IsLoaded()
{
    return loaded_;
}

void AssetRefListener::HandleAssetRefChange(IAttribute *assetRef)
{
    Attribute<AssetReference> *attr = dynamic_cast<Attribute<AssetReference> *>(assetRef);
    if (!attr)
        return; ///\todo Log out warning.

    HandleAssetRefChange(attr->GetOwner()->GetFramework()->Asset(), attr->Get().ref);
}

void AssetRefListener::HandleAssetRefChange(AssetAPI *assetApi, QString assetRef)
{
    assert(assetApi);

    assetRef = assetRef.trimmed();

    AssetTransferPtr transfer = assetApi->RequestAsset(assetRef);
    if (!transfer)
        return; ///\todo Log out warning.

    connect(transfer.get(), SIGNAL(Downloaded(IAssetTransfer*)), this, SLOT(EmitDownloaded(IAssetTransfer*)), Qt::UniqueConnection);
    connect(transfer.get(), SIGNAL(Decoded(AssetPtr)), this, SLOT(EmitDecoded(AssetPtr)), Qt::UniqueConnection);
    connect(transfer.get(), SIGNAL(Loaded(AssetPtr)), this, SLOT(EmitLoaded(AssetPtr)), Qt::UniqueConnection);

    AssetPtr assetData = asset.lock();
    if (assetData)
        disconnect(assetData.get(), SIGNAL(Loaded(AssetPtr)), this, SIGNAL(Loaded(AssetPtr)));
    asset = AssetPtr();
    loaded_ = false;
}

void AssetRefListener::EmitDownloaded(IAssetTransfer *transfer)
{
    assert(transfer);
    if (!transfer)
        return;

    AssetPtr assetData = transfer->asset;
    assert(assetData);
    if (!assetData)
        return;
    asset = assetData;
    
    connect(assetData.get(), SIGNAL(Loaded(AssetPtr)), this, SIGNAL(Loaded(AssetPtr)));
    emit Downloaded(transfer);
}

void AssetRefListener::EmitDecoded(AssetPtr asset)
{
    emit Decoded(asset);
}

void AssetRefListener::EmitLoaded(AssetPtr asset)
{
    loaded_ = true;
    emit Loaded(asset);
}

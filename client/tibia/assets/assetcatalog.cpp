#include "assets/assetcatalog.h"

#include <QJsonArray>

#include <algorithm>

namespace tibia::assets {

void TAssetCatalog::load(const QJsonDocument& document)
{
    m_valid = read(document);
}

bool TAssetCatalog::isCatalogDocument(const QJsonDocument& document)
{
    const QJsonArray array = document.array();
    return !array.isEmpty() && array.first().isObject();
}

bool TAssetCatalog::read(const QJsonDocument& document)
{
    if (!isCatalogDocument(document))
        return m_valid;
    m_valid = true;
    for (const QJsonValue& value : document.array()) {
        const QJsonObject json = value.toObject();
        const TAsset asset(json);
        if (asset.isValid())
            assetRead(asset, json);
        m_valid = m_valid && asset.isValid();
        m_assets.push_back(asset);
    }
    return m_valid;
}

void TAssetCatalog::assetRead(const TAsset&, const QJsonObject&)
{
}

QString TAssetCatalog::fileOf(const TAsset::EType type) const
{
    const auto it = std::find_if(m_assets.begin(), m_assets.end(), [type](const TAsset& asset) { return asset.type() == type; });
    return it == m_assets.end() ? QString() : it->file();
}

}

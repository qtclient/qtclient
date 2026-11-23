#pragma once

#include "assets/asset.h"

#include <QJsonDocument>

#include <functional>

#include <vector>

namespace tibia {
namespace assets {

/// The assets a catalog-content.json lists (CIP: TAssetCatalog).
class TAssetCatalog
{
public:
    TAssetCatalog() = default;
    virtual ~TAssetCatalog() = default;

    /// Reads a catalog document; isValid tells whether every entry was valid.
    void load(const QJsonDocument& document);

    /// Every entry read as a valid asset.
    bool isValid() const { return m_valid; }
    const std::vector<TAsset>& assets() const { return m_assets; }
    size_t assetCount() const { return m_assets.size(); }
    /// The file of the first asset of `type`, empty without one.
    QString fileOf(TAsset::EType type) const;
    /// The first asset `predicate` accepts, or the end.
    std::vector<TAsset>::const_iterator findAsset(const std::function<bool(const TAsset&)>& predicate) const;
    /// The first asset whose file is `filename`, or the end.
    std::vector<TAsset>::const_iterator findIteratorForAssetIdentifiedByFilename(const QString& filename) const;
    /// A copy of the first asset whose file is `filename` (an invalid asset without one).
    TAsset findAssetIdentifiedByFilename(const QString& filename) const;
    bool containsAssetIdentifiedByFilename(const QString& filename) const;

protected:
    /// Called for every valid entry with its JSON (catalog subclasses read more of it).
    virtual void assetRead(const TAsset& asset, const QJsonObject& json);

private:
    /// A catalog document is a non-empty array of objects.
    static bool isCatalogDocument(const QJsonDocument& document);
    bool read(const QJsonDocument& document);

    std::vector<TAsset> m_assets;
    bool m_valid = false;
};

}
}

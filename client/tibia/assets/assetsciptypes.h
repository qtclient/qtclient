#pragma once

// CIP-named assets surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::assets {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TAsset final : public ::tibia::CipTyped {};

class TAssetAsyncLoader final : public cip::tibia::assets::TAssetAsyncLoader
{
    Q_OBJECT
public:
    using cip::tibia::assets::TAssetAsyncLoader::TAssetAsyncLoader;
};

class TAssetAsyncLoaderWorker final : public cip::tibia::assets::TAssetAsyncLoaderWorker
{
    Q_OBJECT
public:
    using cip::tibia::assets::TAssetAsyncLoaderWorker::TAssetAsyncLoaderWorker;
};

class TAssetCatalog final : public ::tibia::CipTyped {};

class TAssetFileSystemHelper final : public ::tibia::CipTyped {};

class TAssetHandler final : public cip::tibia::assets::TAssetHandler
{
    Q_OBJECT
public:
    using cip::tibia::assets::TAssetHandler::TAssetHandler;
};

class TAssetManager final : public cip::tibia::assets::TAssetManager
{
    Q_OBJECT
public:
    using cip::tibia::assets::TAssetManager::TAssetManager;
};

class TContentCatalog final : public ::tibia::CipTyped {};

class TSoundCatalog final : public ::tibia::CipTyped {};

} // namespace tibia::assets


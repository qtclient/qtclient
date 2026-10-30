#pragma once

// CIP-named container surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::container {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TContainer final : public ::tibia::CipTyped {};

class TContainerGameActionHandler final : public cip::tibia::container::TContainerGameActionHandler
{
    Q_OBJECT
public:
    using cip::tibia::container::TContainerGameActionHandler::TContainerGameActionHandler;
};

class TContainerGameActionProvider final : public ::tibia::CipTyped {};

class TContainerProtocolMessageHandler final : public cip::tibia::container::TContainerProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::container::TContainerProtocolMessageHandler::TContainerProtocolMessageHandler;
};

class TContainerRenderProvider final : public ::tibia::CipTyped {};

class TContainerStorage final : public cip::tibia::container::TContainerStorage
{
    Q_OBJECT
public:
    using cip::tibia::container::TContainerStorage::TContainerStorage;
};

class TDepotSearchDetailListStorage final : public cip::tibia::container::TDepotSearchDetailListStorage
{
    Q_OBJECT
public:
    using cip::tibia::container::TDepotSearchDetailListStorage::TDepotSearchDetailListStorage;
};

class TInventoryContainer final : public cip::tibia::container::TInventoryContainer
{
    Q_OBJECT
public:
    using cip::tibia::container::TInventoryContainer::TInventoryContainer;
};

class TObjectAppearanceInstanceInfoStorage final : public cip::tibia::container::TObjectAppearanceInstanceInfoStorage
{
    Q_OBJECT
public:
    using cip::tibia::container::TObjectAppearanceInstanceInfoStorage::TObjectAppearanceInstanceInfoStorage;
};

class TObjectCountStorage final : public cip::tibia::container::TObjectCountStorage
{
    Q_OBJECT
public:
    using cip::tibia::container::TObjectCountStorage::TObjectCountStorage;
};

class TStashStorage final : public cip::tibia::container::TStashStorage
{
    Q_OBJECT
public:
    using cip::tibia::container::TStashStorage::TStashStorage;
};

} // namespace tibia::container

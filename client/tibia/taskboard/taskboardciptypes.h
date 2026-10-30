#pragma once

// CIP-named taskboard surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::taskboard {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


// Preferred / race-picker list models live in gamewindow/taskboardmodels.h
// (real rowCount/data/roles). These cipwire aliases are unused.

class TTaskboardBountyTasksStorage final : public cip::tibia::taskboard::TTaskboardBountyTasksStorage
{
    Q_OBJECT
public:
    using cip::tibia::taskboard::TTaskboardBountyTasksStorage::TTaskboardBountyTasksStorage;
};

class TTaskboardCurrenciesStorage final : public cip::tibia::taskboard::TTaskboardCurrenciesStorage
{
    Q_OBJECT
public:
    using cip::tibia::taskboard::TTaskboardCurrenciesStorage::TTaskboardCurrenciesStorage;
};

class TTaskboardProtocolMessageHandler final : public cip::tibia::taskboard::TTaskboardProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::taskboard::TTaskboardProtocolMessageHandler::TTaskboardProtocolMessageHandler;
};

class TTaskboardShopStorage final : public cip::tibia::taskboard::TTaskboardShopStorage
{
    Q_OBJECT
public:
    using cip::tibia::taskboard::TTaskboardShopStorage::TTaskboardShopStorage;
};

class TTaskboardWeeklyTasksStorage final : public cip::tibia::taskboard::TTaskboardWeeklyTasksStorage
{
    Q_OBJECT
public:
    using cip::tibia::taskboard::TTaskboardWeeklyTasksStorage::TTaskboardWeeklyTasksStorage;
};

} // namespace tibia::taskboard


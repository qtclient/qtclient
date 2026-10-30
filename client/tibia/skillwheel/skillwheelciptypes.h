#pragma once

// CIP-named skillwheel surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::skillwheel {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TModListSortFilterProxyModel final : public cip::tibia::skillwheel::TModListSortFilterProxyModel
{
    Q_OBJECT
public:
    using cip::tibia::skillwheel::TModListSortFilterProxyModel::TModListSortFilterProxyModel;
};

class TSkillWheelGameActionHandler final : public cip::tibia::skillwheel::TSkillWheelGameActionHandler
{
    Q_OBJECT
public:
    using cip::tibia::skillwheel::TSkillWheelGameActionHandler::TSkillWheelGameActionHandler;
};

class TSkillWheelProtocolMessageHandler final : public cip::tibia::skillwheel::TSkillWheelProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::skillwheel::TSkillWheelProtocolMessageHandler::TSkillWheelProtocolMessageHandler;
};

} // namespace tibia::skillwheel


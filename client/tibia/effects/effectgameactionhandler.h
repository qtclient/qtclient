#pragma once

#include "input/gameaction.h"

#include <cip_gen.h>

namespace tibia {
namespace effects {

class TEffectStorage;

/// Shows the tutorial's markers on map fields as field effects (CIP:
/// TEffectGameActionHandler): ADD_GUI_TUTORIAL_MARKER on a field adds the two
/// marker effects, REMOVE_ALL_TUTORIAL_MARKERS stops them.
class TEffectGameActionHandler final : public cip::tibia::effects::TEffectGameActionHandler
{
    Q_OBJECT
public:
    explicit TEffectGameActionHandler(TEffectStorage& storage, QObject* parent = nullptr);
    ~TEffectGameActionHandler() override;

public Q_SLOTS:
    /// Connected to the session router's publishGameActionInternal.
    void handleGameAction(const tibia::input::IGameActionPtr& pGameAction);

private:
    TEffectStorage& m_storage;
};

}
}

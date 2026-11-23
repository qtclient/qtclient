#include "effects/effectgameactionhandler.h"

#include "appearances/appearanceinstance.h"
#include "effects/effectstorage.h"

namespace tibia::effects {

TEffectGameActionHandler::TEffectGameActionHandler(TEffectStorage& storage, QObject* parent)
    : cip::tibia::effects::TEffectGameActionHandler(parent)
    , m_storage(storage)
{
}

TEffectGameActionHandler::~TEffectGameActionHandler() = default;

void TEffectGameActionHandler::handleGameAction(const input::IGameActionPtr& pGameAction)
{
    if (!pGameAction)
        return;
    const input::IGameAction& action = *pGameAction;
    if (action.gameActionType() == input::EGameAction::REMOVE_ALL_TUTORIAL_MARKERS) {
        m_storage.stopTrackedEffects();
        return;
    }
    const auto* marker = input::gameActionAs<input::TGameActionAddGUITutorialMarker>(&action);
    if (!marker)
        return;
    // A marker on a map field. A marker on a creature comes again from the
    // creatures' game action handler, located at the creature's field.
    const Position field = marker->position();
    if (!field.isValid())
        return;
    m_storage.trackEffect(appearances::TAppearanceInstanceFactory::createTutorialFieldEffect(1, field));
    m_storage.trackEffect(appearances::TAppearanceInstanceFactory::createTutorialFieldEffect(0, field));
}

}
